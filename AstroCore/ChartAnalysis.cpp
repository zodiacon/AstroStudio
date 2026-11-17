#include "pch.h"
#include "ChartAnalysis.h"
#include "ArabicParts.h"
#include "DerivedCharts.h"
#include "Synastry.h"
#include <algorithm>
#include <map>
#include <set>

namespace {
	// the aspects between the bodies, looked up by the pair (in either order)
	class AspectMap {
	public:
		explicit AspectMap(std::vector<AspectData> const& aspects) {
			for (auto const& a : aspects)
				m_Map[Key(a.Planet1.Planet, a.Planet2.Planet)] = a;
		}
		AspectData const* Find(Planet a, Planet b, AspectType type) const {
			auto it = m_Map.find(Key(a, b));
			return it != m_Map.end() && it->second.Type == type ? &it->second : nullptr;
		}
		bool Has(Planet a, Planet b, AspectType type) const {
			return Find(a, b, type) != nullptr;
		}

	private:
		static std::pair<int, int> Key(Planet a, Planet b) {
			auto x = static_cast<int>(a), y = static_cast<int>(b);
			return x < y ? std::pair{ x, y } : std::pair{ y, x };
		}
		std::map<std::pair<int, int>, AspectData> m_Map;
	};

	struct Builder {
		AspectMap const& Map;
		std::map<Planet, ZodiacSign> Signs;

		AspectPattern Make(PatternKind kind, std::vector<Planet> planets, std::vector<std::pair<Planet, Planet>> const& links, AspectType const* types) const {
			AspectPattern p{ kind };
			std::ranges::sort(planets);
			p.Planets = std::move(planets);
			for (size_t i = 0; i < links.size(); i++) {
				auto a = Map.Find(links[i].first, links[i].second, types[i]);
				p.Aspects.push_back(*a);
				p.MaxOrb = std::max(p.MaxOrb, a->Orb);
			}
			return p;
		}
		// the element (modality) all the planets' signs share, if they do
		template<typename T, typename F>
		std::optional<T> Common(std::vector<Planet> const& planets, F of) const {
			std::optional<T> result;
			for (auto planet : planets) {
				auto value = of(Signs.at(planet));
				if (result && *result != value)
					return std::nullopt;
				result = value;
			}
			return result;
		}
	};

	bool Contains(std::vector<Planet> const& set, std::vector<Planet> const& subset) {
		return std::ranges::all_of(subset, [&](auto p) { return std::ranges::find(set, p) != set.end(); });
	}

	constexpr int WeightOf(Planet planet, bool weighted) {
		return weighted && (planet == Planet::Sun || planet == Planet::Moon) ? 2 : 1;
	}

	BalanceSet MakeSet(BalanceKind kind, int groups) {
		BalanceSet set{ kind };
		set.Groups.resize(groups);
		return set;
	}

	void Finish(BalanceSet& set) {
		double best = 0;
		for (auto const& g : set.Groups)
			best = std::max(best, g.Score);
		for (int i = 0; i < static_cast<int>(set.Groups.size()); i++) {
			auto const& g = set.Groups[i];
			if (best > 0 && g.Score == best)
				set.Dominant.push_back(i);
			if (g.Planets() == 0)
				set.Missing.push_back(i);
			else if (g.Planets() == 1)
				set.Singletons.push_back(i);
		}
	}
}

int BalanceGroup::Planets() const {
	return static_cast<int>(std::ranges::count(Points, PointKind::Planet, &BalancePoint::Kind));
}

Element ChartAnalysis::ElementOf(ZodiacSign sign) {
	return static_cast<Element>(static_cast<int>(sign) % 4);
}

Modality ChartAnalysis::ModalityOf(ZodiacSign sign) {
	return static_cast<Modality>(static_cast<int>(sign) % 3);
}

std::vector<Planet> ChartAnalysis::DomicileRulers(ZodiacSign sign, RulerSystem rulers) {
	std::vector<Planet> result{ ArabicParts::Ruler(sign, false) };
	if (rulers == RulerSystem::Modern) {
		auto modern = ArabicParts::Ruler(sign, true);
		if (modern != result[0])
			result.push_back(modern);
	}
	return result;
}

std::optional<ZodiacSign> ChartAnalysis::Exaltation(Planet planet) {
	switch (planet) {
		case Planet::Sun: return ZodiacSign::Aries;
		case Planet::Moon: return ZodiacSign::Taurus;
		case Planet::Mercury: return ZodiacSign::Virgo;
		case Planet::Venus: return ZodiacSign::Pisces;
		case Planet::Mars: return ZodiacSign::Capricorn;
		case Planet::Jupiter: return ZodiacSign::Cancer;
		case Planet::Saturn: return ZodiacSign::Libra;
	}
	return std::nullopt;
}

bool ChartAnalysis::HasDignities(Planet planet, RulerSystem rulers) {
	if (planet >= Planet::Sun && planet <= Planet::Saturn)
		return true;
	return rulers == RulerSystem::Modern && planet >= Planet::Uranus && planet <= Planet::Pluto;
}

Planet ChartAnalysis::TermRuler(AstroPoint const& longitude) {
	// the Egyptian terms: for each sign the five planets and the degree each one's term ends at
	using enum Planet;
	struct Term { Planet Ruler; int End; };
	static constexpr Term terms[12][5] = {
		{ { Jupiter, 6 }, { Venus, 12 }, { Mercury, 20 }, { Mars, 25 }, { Saturn, 30 } },		// Aries
		{ { Venus, 8 }, { Mercury, 14 }, { Jupiter, 22 }, { Saturn, 27 }, { Mars, 30 } },		// Taurus
		{ { Mercury, 6 }, { Jupiter, 12 }, { Venus, 17 }, { Mars, 24 }, { Saturn, 30 } },		// Gemini
		{ { Mars, 7 }, { Venus, 13 }, { Mercury, 19 }, { Jupiter, 26 }, { Saturn, 30 } },		// Cancer
		{ { Jupiter, 6 }, { Venus, 11 }, { Saturn, 18 }, { Mercury, 24 }, { Mars, 30 } },		// Leo
		{ { Mercury, 7 }, { Venus, 17 }, { Jupiter, 21 }, { Mars, 28 }, { Saturn, 30 } },		// Virgo
		{ { Saturn, 6 }, { Mercury, 14 }, { Jupiter, 21 }, { Venus, 28 }, { Mars, 30 } },		// Libra
		{ { Mars, 7 }, { Venus, 11 }, { Mercury, 19 }, { Jupiter, 24 }, { Saturn, 30 } },		// Scorpio
		{ { Jupiter, 12 }, { Venus, 17 }, { Mercury, 21 }, { Saturn, 26 }, { Mars, 30 } },		// Sagittarius
		{ { Mercury, 7 }, { Jupiter, 14 }, { Venus, 22 }, { Saturn, 26 }, { Mars, 30 } },		// Capricorn
		{ { Mercury, 7 }, { Venus, 13 }, { Jupiter, 20 }, { Mars, 25 }, { Saturn, 30 } },		// Aquarius
		{ { Venus, 12 }, { Jupiter, 16 }, { Mercury, 19 }, { Mars, 28 }, { Saturn, 30 } },		// Pisces
	};
	auto const& sign = terms[static_cast<int>(longitude.Sign())];
	double degree = longitude.DegreeInSign();
	for (auto const& term : sign)
		if (degree < term.End)
			return term.Ruler;
	return sign[4].Ruler;
}

Planet ChartAnalysis::FaceRuler(AstroPoint const& longitude) {
	// the Chaldean order, from Mars at 0 Aries through the 36 decans
	using enum Planet;
	static constexpr Planet order[] = { Mars, Sun, Venus, Mercury, Moon, Saturn, Jupiter };
	int decan = std::clamp(static_cast<int>(AstroPoint(longitude.Value).Normalize().Value / 10), 0, 35);
	return order[decan % 7];
}

std::array<Planet, 3> ChartAnalysis::TriplicityRulers(Element element) {
	using enum Planet;
	switch (element) {
		case Element::Fire: return { Sun, Jupiter, Saturn };
		case Element::Earth: return { Venus, Moon, Mars };
		case Element::Air: return { Saturn, Mercury, Jupiter };
		default: return { Venus, Mars, Moon };
	}
}

DignityData ChartAnalysis::DignityOf(Planet planet, AstroPoint const& longitude, RulerSystem rulers, std::optional<bool> day) {
	auto sign = longitude.Sign();
	DignityData d{ planet, sign };
	d.TermRuler = TermRuler(longitude);
	d.FaceRuler = FaceRuler(longitude);
	d.TriplicityRulers = TriplicityRulers(ElementOf(sign));
	const bool seven = planet >= Planet::Sun && planet <= Planet::Saturn;
	auto add = [&](Dignity dignity, int score) {
		d.Dignities.push_back(dignity);
		d.Score += score;
	};

	auto opposite = static_cast<ZodiacSign>((static_cast<int>(sign) + 6) % 12);
	auto rules = [&](ZodiacSign s) {
		auto list = DomicileRulers(s, rulers);
		return std::ranges::find(list, planet) != list.end();
	};
	if (rules(sign))
		add(Dignity::Domicile, 5);
	auto exaltation = Exaltation(planet);
	if (exaltation == sign)
		add(Dignity::Exaltation, 4);
	if (seven) {
		auto const& t = d.TriplicityRulers;
		bool byDay = day != false && t[0] == planet, byNight = day != true && t[1] == planet;
		if (byDay || byNight || t[2] == planet)
			add(Dignity::Triplicity, 3);
		if (d.TermRuler == planet)
			add(Dignity::Term, 2);
		if (d.FaceRuler == planet)
			add(Dignity::Face, 1);
		if (d.Dignities.empty()) {
			d.Peregrine = true;
			d.Score -= 5;
		}
	}
	if (rules(opposite))
		add(Dignity::Detriment, -5);
	if (exaltation && static_cast<ZodiacSign>((static_cast<int>(*exaltation) + 6) % 12) == sign)
		add(Dignity::Fall, -4);
	return d;
}

std::optional<ShapeData> ChartAnalysis::ShapeOf(std::vector<PlanetPosition> const& bodies) {
	const int n = static_cast<int>(bodies.size());
	if (n < 7)
		return std::nullopt;
	auto sorted = bodies;
	for (auto& b : sorted)
		b.Longitude = AstroPoint(b.Longitude.Value).Normalize();
	std::ranges::sort(sorted, [](auto const& a, auto const& b) { return a.Longitude.Value < b.Longitude.Value; });
	// gap[i]: the empty arc from planet i forward to planet i + 1
	std::vector<double> gap(n);
	for (int i = 0; i < n; i++) {
		double g = sorted[(i + 1) % n].Longitude.Value - sorted[i].Longitude.Value;
		gap[i] = g < 0 ? g + 360 : g;
	}
	int largest = static_cast<int>(std::ranges::max_element(gap) - gap.begin());
	ShapeData s{};
	s.LargestGap = gap[largest];
	s.Span = 360 - s.LargestGap;
	auto after = [&](int i) { return sorted[(i + 1) % n].Planet; };
	constexpr double eps = 1e-9;

	if (s.Span <= 120 + eps) {
		s.Shape = ChartShape::Bundle;
		s.Leading = after(largest);
		return s;
	}
	if (s.Span <= 180 + eps) {
		s.Shape = ChartShape::Bowl;
		s.Leading = after(largest);
		return s;
	}
	// a bucket: one planet alone, the others in half the wheel
	for (int k = 0; k < n; k++) {
		int before = (k + n - 1) % n;
		double merged = gap[before] + gap[k];
		if (gap[before] >= 45 && gap[k] >= 45 && merged >= 180 - eps) {
			s.Shape = ChartShape::Bucket;
			s.Handle = sorted[k].Planet;
			s.Leading = after(k);
			return s;
		}
	}
	// (an empty trine give or take ten degrees: Jones read the shapes by eye, and 119 degrees empty is still a locomotive)
	if (s.LargestGap >= 110 - eps) {
		s.Shape = ChartShape::Locomotive;
		s.Leading = after(largest);
		return s;
	}
	// two empty spaces of 60 degrees or more make two groups (a seesaw); three or more, several clusters (a splay)
	auto wide = std::ranges::count_if(gap, [](double g) { return g >= 60; });
	if (wide >= 3) {
		s.Shape = ChartShape::Splay;
		return s;
	}
	if (wide == 2) {
		s.Shape = ChartShape::Seesaw;
		return s;
	}
	std::set<ZodiacSign> signs;
	for (auto const& b : sorted)
		signs.insert(b.Longitude.Sign());
	s.Shape = s.LargestGap < 60 && signs.size() >= 7 ? ChartShape::Splash : ChartShape::Splay;
	return s;
}

std::vector<AspectPattern> ChartAnalysis::Patterns(ChartData const& chart, std::vector<PlanetPosition> const& bodies,
	AspectCalculator const& calculator, int stelliumSize) {
	AspectMap map(calculator.Calculate(bodies));
	Builder b{ map };
	for (auto const& body : bodies)
		b.Signs[body.Planet] = body.Longitude.Sign();

	std::vector<Planet> planets;
	for (auto const& body : bodies)
		planets.push_back(body.Planet);
	const int n = static_cast<int>(planets.size());
	auto has = [&](int i, int j, AspectType t) { return map.Has(planets[i], planets[j], t); };

	std::vector<AspectPattern> result;
	std::vector<std::pair<int, int>> oppositions, sextiles;
	for (int i = 0; i < n; i++)
		for (int j = i + 1; j < n; j++) {
			if (has(i, j, AspectType::Opposition))
				oppositions.emplace_back(i, j);
			if (has(i, j, AspectType::Sextile))
				sextiles.emplace_back(i, j);
		}

	// grand crosses and mystic rectangles: two oppositions and what joins their ends
	std::vector<std::vector<Planet>> crosses;
	for (size_t x = 0; x < oppositions.size(); x++)
		for (size_t y = x + 1; y < oppositions.size(); y++) {
			auto [a1, a2] = oppositions[x];
			auto [b1, b2] = oppositions[y];
			if (a1 == b1 || a1 == b2 || a2 == b1 || a2 == b2)
				continue;
			auto P = [&](int i) { return planets[i]; };
			if (has(a1, b1, AspectType::Square) && has(a1, b2, AspectType::Square) && has(a2, b1, AspectType::Square) && has(a2, b2, AspectType::Square)) {
				static constexpr AspectType types[] = { AspectType::Opposition, AspectType::Opposition,
					AspectType::Square, AspectType::Square, AspectType::Square, AspectType::Square };
				auto p = b.Make(PatternKind::GrandCross, { P(a1), P(a2), P(b1), P(b2) },
					{ { P(a1), P(a2) }, { P(b1), P(b2) }, { P(a1), P(b1) }, { P(a1), P(b2) }, { P(a2), P(b1) }, { P(a2), P(b2) } }, types);
				p.Modality = b.Common<Modality>(p.Planets, ModalityOf);
				p.OutOfSign = !p.Modality;
				crosses.push_back(p.Planets);
				result.push_back(std::move(p));
				continue;
			}
			// a mystic rectangle: a1 sextile one end of the other opposition and trine the other, and a2 the other way round
			for (auto [c, d] : { std::pair{ b1, b2 }, std::pair{ b2, b1 } }) {
				if (has(a1, c, AspectType::Sextile) && has(a1, d, AspectType::Trine) && has(a2, d, AspectType::Sextile) && has(a2, c, AspectType::Trine)) {
					static constexpr AspectType types[] = { AspectType::Opposition, AspectType::Opposition,
						AspectType::Sextile, AspectType::Sextile, AspectType::Trine, AspectType::Trine };
					result.push_back(b.Make(PatternKind::MysticRectangle, { P(a1), P(a2), P(c), P(d) },
						{ { P(a1), P(a2) }, { P(c), P(d) }, { P(a1), P(c) }, { P(a2), P(d) }, { P(a1), P(d) }, { P(a2), P(c) } }, types));
					break;
				}
			}
		}

	// grand trines, and the kites built on them
	for (int i = 0; i < n; i++)
		for (int j = i + 1; j < n; j++) {
			if (!has(i, j, AspectType::Trine))
				continue;
			for (int k = j + 1; k < n; k++) {
				if (!has(i, k, AspectType::Trine) || !has(j, k, AspectType::Trine))
					continue;
				static constexpr AspectType trines[] = { AspectType::Trine, AspectType::Trine, AspectType::Trine };
				auto pi = planets[i], pj = planets[j], pk = planets[k];
				auto p = b.Make(PatternKind::GrandTrine, { pi, pj, pk }, { { pi, pj }, { pi, pk }, { pj, pk } }, trines);
				p.Element = b.Common<Element>(p.Planets, ElementOf);
				p.OutOfSign = !p.Element;
				result.push_back(p);

				int corners[] = { i, j, k };
				for (int c = 0; c < 3; c++) {
					int head = corners[c], o1 = corners[(c + 1) % 3], o2 = corners[(c + 2) % 3];
					for (int t = 0; t < n; t++) {
						if (t == i || t == j || t == k)
							continue;
						if (has(t, head, AspectType::Opposition) && has(t, o1, AspectType::Sextile) && has(t, o2, AspectType::Sextile)) {
							static constexpr AspectType types[] = { AspectType::Trine, AspectType::Trine, AspectType::Trine,
								AspectType::Opposition, AspectType::Sextile, AspectType::Sextile };
							auto pt = planets[t];
							auto kite = b.Make(PatternKind::Kite, { pi, pj, pk, pt },
								{ { pi, pj }, { pi, pk }, { pj, pk }, { pt, planets[head] }, { pt, planets[o1] }, { pt, planets[o2] } }, types);
							kite.Apex = planets[head];
							kite.Element = p.Element;
							result.push_back(std::move(kite));
						}
					}
				}
			}
		}

	// T-squares (those that are part of a grand cross are not listed again)
	for (auto [a, c] : oppositions)
		for (int x = 0; x < n; x++) {
			if (x == a || x == c || !has(a, x, AspectType::Square) || !has(c, x, AspectType::Square))
				continue;
			std::vector<Planet> members{ planets[a], planets[c], planets[x] };
			if (std::ranges::any_of(crosses, [&](auto const& cross) { return Contains(cross, members); }))
				continue;
			static constexpr AspectType types[] = { AspectType::Opposition, AspectType::Square, AspectType::Square };
			auto p = b.Make(PatternKind::TSquare, members, { { planets[a], planets[c] }, { planets[a], planets[x] }, { planets[c], planets[x] } }, types);
			p.Apex = planets[x];
			p.Modality = b.Common<Modality>(p.Planets, ModalityOf);
			p.OutOfSign = !p.Modality;
			result.push_back(std::move(p));
		}

	// yods
	for (auto [a, c] : sextiles)
		for (int x = 0; x < n; x++) {
			if (x == a || x == c || !has(a, x, AspectType::Quincunx) || !has(c, x, AspectType::Quincunx))
				continue;
			static constexpr AspectType types[] = { AspectType::Sextile, AspectType::Quincunx, AspectType::Quincunx };
			auto p = b.Make(PatternKind::Yod, { planets[a], planets[c], planets[x] },
				{ { planets[a], planets[c] }, { planets[a], planets[x] }, { planets[c], planets[x] } }, types);
			p.Apex = planets[x];
			result.push_back(std::move(p));
		}

	// stelliums: in a sign (with its house if they share one), then in a house unless the same planets already made one in a sign
	auto const& houses = chart.Houses();
	const bool hasHouses = Synastry::HasHouses(houses);
	std::map<int, std::vector<Planet>> bySign, byHouse;
	std::map<Planet, int> houseOf;
	for (auto const& body : bodies) {
		bySign[static_cast<int>(body.Longitude.Sign())].push_back(body.Planet);
		if (hasHouses) {
			auto h = DerivedCharts::HouseOf(houses, body.Longitude);
			houseOf[body.Planet] = h;
			if (h > 0)
				byHouse[h].push_back(body.Planet);
		}
	}
	std::vector<std::vector<Planet>> signStelliums;
	for (auto& [sign, members] : bySign) {
		if (static_cast<int>(members.size()) < stelliumSize)
			continue;
		AspectPattern p{ PatternKind::Stellium };
		std::ranges::sort(members);
		p.Planets = members;
		p.Sign = static_cast<ZodiacSign>(sign);
		p.Element = ElementOf(*p.Sign);
		p.Modality = ModalityOf(*p.Sign);
		if (hasHouses && std::ranges::all_of(members, [&](auto pl) { return houseOf[pl] == houseOf[members[0]]; }))
			p.House = houseOf[members[0]];
		signStelliums.push_back(members);
		result.push_back(std::move(p));
	}
	for (auto& [house, members] : byHouse) {
		if (static_cast<int>(members.size()) < stelliumSize)
			continue;
		std::ranges::sort(members);
		if (std::ranges::find(signStelliums, members) != signStelliums.end())
			continue;
		AspectPattern p{ PatternKind::Stellium };
		p.Planets = members;
		p.House = house;
		result.push_back(std::move(p));
	}

	std::ranges::stable_sort(result, [](auto const& x, auto const& y) {
		if (x.Kind != y.Kind)
			return x.Kind < y.Kind;
		return x.MaxOrb < y.MaxOrb;
	});
	return result;
}

ChartAnalysisResult ChartAnalysis::Run(ChartData const& chart, AspectCalculator const& calculator, ChartAnalysisOptions const& options) {
	ChartAnalysisResult r;
	std::vector<PlanetPosition> bodies;
	for (auto planet : options.Bodies) {
		auto const& all = chart.AllPlanets();
		auto it = std::ranges::find(all, planet, &PlanetPosition::Planet);
		if (it != all.end())
			bodies.push_back(*it);
	}
	auto const& houses = chart.Houses();
	r.HasHouses = Synastry::HasHouses(houses);
	if (auto sun = std::ranges::find(bodies, Planet::Sun, &PlanetPosition::Planet); r.HasHouses && sun != bodies.end())
		r.DayChart = ArabicParts::IsDay(sun->Longitude, houses.Asc);

	r.Patterns = Patterns(chart, bodies, calculator, options.StelliumSize);

	// the balance
	r.Elements = MakeSet(BalanceKind::Element, 4);
	r.Modalities = MakeSet(BalanceKind::Modality, 3);
	r.Hemispheres = MakeSet(BalanceKind::Hemisphere, r.HasHouses ? 2 : 0);
	r.Sides = MakeSet(BalanceKind::Side, r.HasHouses ? 2 : 0);
	r.Quadrants = MakeSet(BalanceKind::Quadrant, r.HasHouses ? 4 : 0);
	r.Angularity = MakeSet(BalanceKind::Angularity, r.HasHouses ? 3 : 0);
	r.Houses = MakeSet(BalanceKind::House, r.HasHouses ? 12 : 0);
	auto add = [](BalanceSet& set, int group, BalancePoint const& point) {
		set.Groups[group].Points.push_back(point);
		set.Groups[group].Score += point.Weight;
	};
	auto addSign = [&](ZodiacSign sign, BalancePoint const& point) {
		add(r.Elements, static_cast<int>(ElementOf(sign)), point);
		add(r.Modalities, static_cast<int>(ModalityOf(sign)), point);
	};
	for (auto const& body : bodies) {
		BalancePoint point{ PointKind::Planet, body.Planet, static_cast<double>(WeightOf(body.Planet, options.Weighted)) };
		addSign(body.Longitude.Sign(), point);
		if (r.HasHouses) {
			auto h = DerivedCharts::HouseOf(houses, body.Longitude);
			if (h > 0) {
				add(r.Hemispheres, h >= 7 ? 0 : 1, point);
				add(r.Sides, h >= 10 || h <= 3 ? 0 : 1, point);
				add(r.Quadrants, (h - 1) / 3, point);
				add(r.Angularity, (h - 1) % 3, point);
				add(r.Houses, h - 1, point);
			}
		}
	}
	if (r.HasHouses) {
		double weight = options.Weighted ? 2 : 1;
		addSign(houses.Asc.Sign(), { PointKind::Ascendant, Planet::Sun, weight });
		addSign(houses.MC.Sign(), { PointKind::Midheaven, Planet::Sun, weight });
	}
	for (auto set : { &r.Elements, &r.Modalities, &r.Hemispheres, &r.Sides, &r.Quadrants, &r.Angularity, &r.Houses })
		Finish(*set);

	// dignities and receptions
	for (auto const& body : bodies)
		if (HasDignities(body.Planet, options.Rulers))
			r.Dignities.push_back(DignityOf(body.Planet, body.Longitude, options.Rulers, r.DayChart));
	for (size_t i = 0; i < r.Dignities.size(); i++)
		for (size_t j = i + 1; j < r.Dignities.size(); j++) {
			auto const& a = r.Dignities[i];
			auto const& b = r.Dignities[j];
			auto rules = [&](Planet p, ZodiacSign s) {
				auto list = DomicileRulers(s, options.Rulers);
				return std::ranges::find(list, p) != list.end();
			};
			auto first = std::min(a.Body, b.Body), second = std::max(a.Body, b.Body);
			if (rules(a.Body, b.Sign) && rules(b.Body, a.Sign))
				r.Receptions.push_back({ first, second, false });
			if (Exaltation(a.Body) == b.Sign && Exaltation(b.Body) == a.Sign)
				r.Receptions.push_back({ first, second, true });
		}

	// dispositors
	const bool modern = options.Rulers == RulerSystem::Modern;
	std::map<Planet, Planet> parent;
	for (auto const& body : bodies) {
		auto ruler = ArabicParts::Ruler(body.Longitude.Sign(), modern);
		bool inChart = std::ranges::find(bodies, ruler, &PlanetPosition::Planet) != bodies.end();
		r.Dispositors.Nodes.push_back({ body.Planet, ruler, inChart });
		if (inChart)
			parent[body.Planet] = ruler;
	}
	std::set<Planet> inCycle;
	bool allReachOne = true;
	std::set<std::vector<Planet>> seen;
	for (auto const& body : bodies) {
		// follow the chain until it stops (a ruler outside) or comes round
		std::vector<Planet> path{ body.Planet };
		for (;;) {
			auto it = parent.find(path.back());
			if (it == parent.end()) {
				allReachOne = false;
				break;
			}
			auto loop = std::ranges::find(path, it->second);
			if (loop != path.end()) {
				std::vector<Planet> cycle(loop, path.end());
				std::ranges::rotate(cycle, std::ranges::min_element(cycle));	// (the same loop reached from anywhere is the same)
				if (seen.insert(cycle).second)
					r.Dispositors.Ends.push_back(cycle);
				break;
			}
			path.push_back(it->second);
		}
	}
	if (allReachOne && r.Dispositors.Ends.size() == 1 && r.Dispositors.Ends[0].size() == 1)
		r.Dispositors.Final = r.Dispositors.Ends[0][0];

	// the chart ruler
	if (r.HasHouses) {
		ChartRuler ruler{ ArabicParts::Ruler(houses.Asc.Sign(), modern), houses.Asc.Sign() };
		auto const& all = chart.AllPlanets();
		if (auto it = std::ranges::find(all, ruler.Body, &PlanetPosition::Planet); it != all.end()) {
			ruler.Sign = it->Longitude.Sign();
			ruler.House = DerivedCharts::HouseOf(houses, it->Longitude);
		}
		r.Ruler = ruler;
	}

	// the shape, and the conditions of the planets
	r.Shape = ShapeOf(bodies);
	{
		std::set<Planet> aspected;
		for (auto const& a : calculator.Calculate(bodies))
			if (a.IsMajor()) {
				aspected.insert(a.Planet1.Planet);
				aspected.insert(a.Planet2.Planet);
			}
		for (auto const& body : bodies)
			if (!aspected.contains(body.Planet))
				r.Unaspected.push_back(body.Planet);
	}
	for (auto const& body : bodies)
		if (body.Speed < 0)
			r.Retrograde.push_back(body.Planet);
	if (r.HasHouses) {
		std::pair<AngleKind, AstroPoint> angles[] = {
			{ AngleKind::Ascendant, houses.Asc }, { AngleKind::Midheaven, houses.MC },
			{ AngleKind::Descendant, houses.Asc.Opposite() }, { AngleKind::Imum, houses.MC.Opposite() },
		};
		for (auto const& body : bodies) {
			std::optional<AngularPlanet> best;
			for (auto const& [kind, longitude] : angles) {
				double orb = AstroPoint::Diff(body.Longitude, longitude);
				if (orb <= options.AngleOrb && (!best || orb < best->Orb))
					best = AngularPlanet{ body.Planet, kind, orb };
			}
			if (best)
				r.OnAngles.push_back(*best);
		}
		std::ranges::stable_sort(r.OnAngles, {}, &AngularPlanet::Orb);
	}
	return r;
}
