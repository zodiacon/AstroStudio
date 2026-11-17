#include "TestCommon.h"
#include "ChartAnalysis.h"
#include <algorithm>
#include <map>

namespace {
	PlanetPosition At(Planet planet, double longitude) {
		return PlanetPosition{ .Longitude = AstroPoint(longitude), .Speed = 0, .Latitude = 0, .LatitudeSpeed = 0, .Planet = planet };
	}

	// a chart of the planets given, with equal houses from an Ascendant (none if asc is negative)
	ChartData Chart(std::initializer_list<PlanetPosition> planets, double asc = -1, double mc = 0) {
		ChartData chart;
		chart.AddPlanets(planets);
		if (asc >= 0) {
			auto& houses = chart.Houses();
			houses.Asc = asc;
			houses.MC = mc;
			for (int i = 0; i < 12; i++)
				houses.Cusps[i] = AstroPoint(asc + 30 * i).Normalize();
		}
		return chart;
	}

	// every aspect on, wide enough for the hand-made charts
	AspectCalculator Calculator() {
		AspectSettings settings;
		settings.MajorAspectOrb = 6;
		settings.MinorAspectOrb = 3;
		return AspectCalculator(settings);
	}

	std::vector<AspectPattern> OfKind(ChartAnalysisResult const& r, PatternKind kind) {
		std::vector<AspectPattern> result;
		std::ranges::copy_if(r.Patterns, std::back_inserter(result), [&](auto const& p) { return p.Kind == kind; });
		return result;
	}

	using P = Planet;
}

TEST_CASE("Grand trine", "[ChartAnalysis]") {
	SECTION("in one element") {
		// Sun 5 Aries, Moon 7 Leo, Jupiter 3 Sagittarius
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 5), At(P::Moon, 127), At(P::Jupiter, 243), At(P::Saturn, 100) }), Calculator());
		auto trines = OfKind(r, PatternKind::GrandTrine);
		REQUIRE(trines.size() == 1);
		CHECK(trines[0].Planets == std::vector{ P::Sun, P::Moon, P::Jupiter });
		CHECK(trines[0].Element == Element::Fire);
		CHECK_FALSE(trines[0].OutOfSign);
		CHECK(trines[0].Aspects.size() == 3);
		CHECK(trines[0].MaxOrb == Approx(4).margin(1e-4));		// Moon-Jupiter: 116 apart
	}
	SECTION("out of sign") {
		// Sun 28 Pisces, Moon 1 Leo, Jupiter 29 Scorpio: trines across the elements
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 358), At(P::Moon, 121), At(P::Jupiter, 239) }), Calculator());
		auto trines = OfKind(r, PatternKind::GrandTrine);
		REQUIRE(trines.size() == 1);
		CHECK_FALSE(trines[0].Element);
		CHECK(trines[0].OutOfSign);
	}
}

TEST_CASE("Grand cross and T-square", "[ChartAnalysis]") {
	SECTION("a T-square and its apex") {
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 10), At(P::Mars, 190), At(P::Saturn, 100) }), Calculator());
		auto t = OfKind(r, PatternKind::TSquare);
		REQUIRE(t.size() == 1);
		CHECK(t[0].Apex == P::Saturn);
		CHECK(t[0].Modality == Modality::Cardinal);
		CHECK(OfKind(r, PatternKind::GrandCross).empty());
	}
	SECTION("a grand cross hides the T-squares inside it") {
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 10), At(P::Mars, 190), At(P::Saturn, 100), At(P::Moon, 280) }), Calculator());
		auto crosses = OfKind(r, PatternKind::GrandCross);
		REQUIRE(crosses.size() == 1);
		CHECK(crosses[0].Planets == std::vector{ P::Sun, P::Moon, P::Mars, P::Saturn });
		CHECK(crosses[0].Aspects.size() == 6);
		CHECK(crosses[0].Modality == Modality::Cardinal);
		CHECK(OfKind(r, PatternKind::TSquare).empty());
	}
	SECTION("two crosses sharing planets leave no T-square") {
		// Venus at 103 is opposite the Moon and square Sun and Mars: a second cross
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 10), At(P::Mars, 190), At(P::Saturn, 100), At(P::Moon, 280), At(P::Venus, 103) }), Calculator());
		CHECK(OfKind(r, PatternKind::GrandCross).size() == 2);		// Venus makes a second cross with Sun, Mars and Moon
		CHECK(OfKind(r, PatternKind::TSquare).empty());
	}
}

TEST_CASE("Yod, kite and mystic rectangle", "[ChartAnalysis]") {
	SECTION("yod") {
		// Venus 0 Taurus sextile Mars 0 Cancer, both quincunx Pluto 0 Sagittarius
		auto r = ChartAnalysis::Run(Chart({ At(P::Venus, 30), At(P::Mars, 90), At(P::Pluto, 240) }), Calculator());
		auto yods = OfKind(r, PatternKind::Yod);
		REQUIRE(yods.size() == 1);
		CHECK(yods[0].Apex == P::Pluto);
	}
	SECTION("a yod needs the quincunx") {
		AspectSettings settings;
		settings.AspectEnabled[static_cast<int>(AspectType::Quincunx)] = false;
		auto r = ChartAnalysis::Run(Chart({ At(P::Venus, 30), At(P::Mars, 90), At(P::Pluto, 240) }), AspectCalculator(settings));
		CHECK(OfKind(r, PatternKind::Yod).empty());
	}
	SECTION("kite, with its grand trine") {
		// grand trine Sun 0, Moon 120, Jupiter 240; Saturn at 180 opposite the Sun, sextile the other two
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 0), At(P::Moon, 120), At(P::Jupiter, 240), At(P::Saturn, 180) }), Calculator());
		auto kites = OfKind(r, PatternKind::Kite);
		REQUIRE(kites.size() == 1);
		CHECK(kites[0].Apex == P::Sun);
		CHECK(kites[0].Planets.size() == 4);
		CHECK(OfKind(r, PatternKind::GrandTrine).size() == 1);
		CHECK(r.Patterns.front().Kind == PatternKind::Kite);		// (in order of kind)
	}
	SECTION("mystic rectangle") {
		// oppositions Sun 0 - Moon 180 and Venus 60 - Mars 240
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 0), At(P::Moon, 180), At(P::Venus, 60), At(P::Mars, 240) }), Calculator());
		auto rects = OfKind(r, PatternKind::MysticRectangle);
		REQUIRE(rects.size() == 1);
		CHECK(rects[0].Aspects.size() == 6);
		CHECK(OfKind(r, PatternKind::GrandCross).empty());
	}
}

TEST_CASE("Stellium", "[ChartAnalysis]") {
	SECTION("in a sign and a house") {
		// Sun, Mercury, Venus in Capricorn; equal houses from 0 Capricorn put them in house 1
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 275), At(P::Mercury, 280), At(P::Venus, 290), At(P::Moon, 100) }, 270), Calculator());
		auto s = OfKind(r, PatternKind::Stellium);
		REQUIRE(s.size() == 1);		// the house one is the same planets, so not listed twice
		CHECK(s[0].Sign == ZodiacSign::Capricorn);
		CHECK(s[0].House == 1);
		CHECK(s[0].Element == Element::Earth);
	}
	SECTION("in a house across signs") {
		// equal houses from 15 Aries: 28 Aries, 2 Taurus and 10 Taurus are in house 1
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 28), At(P::Mercury, 32), At(P::Venus, 40) }, 15), Calculator());
		auto s = OfKind(r, PatternKind::Stellium);
		REQUIRE(s.size() == 1);
		CHECK_FALSE(s[0].Sign);
		CHECK(s[0].House == 1);
	}
	SECTION("too few") {
		ChartAnalysisOptions options;
		options.StelliumSize = 4;
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 275), At(P::Mercury, 280), At(P::Venus, 290) }), Calculator(), options);
		CHECK(OfKind(r, PatternKind::Stellium).empty());
	}
}

TEST_CASE("Balance", "[ChartAnalysis]") {
	// Sun 5 Aries (fire, cardinal), Moon 10 Taurus (earth, fixed), Mars 15 Leo (fire, fixed); Asc 0 Cancer, MC 0 Aries
	auto chart = Chart({ At(P::Sun, 5), At(P::Moon, 40), At(P::Mars, 135) }, 90, 0);
	SECTION("weighted") {
		auto r = ChartAnalysis::Run(chart, Calculator());
		REQUIRE(r.HasHouses);
		auto const& fire = r.Elements.Groups[static_cast<int>(Element::Fire)];
		CHECK(fire.Score == 5);		// Sun 2, Mars 1, MC 2
		CHECK(fire.Planets() == 2);
		CHECK(r.Elements.Groups[static_cast<int>(Element::Water)].Score == 2);		// the Ascendant alone
		CHECK(r.Elements.Dominant == std::vector{ 0 });
		// water has only an angle, air nothing: both lack planets; earth has one
		CHECK(r.Elements.Missing == std::vector{ 2, 3 });
		CHECK(r.Elements.Singletons == std::vector{ 1 });
		CHECK(r.Modalities.Groups[static_cast<int>(Modality::Cardinal)].Score == 6);		// Sun, Asc, MC
		CHECK(r.Modalities.Groups[static_cast<int>(Modality::Mutable)].Points.empty());
		// houses from 90: Sun (5) in 10, Moon (40) in 11, Mars (135) in 2
		CHECK(r.Hemispheres.Groups[0].Planets() == 2);		// above
		CHECK(r.Sides.Groups[0].Planets() == 3);			// all eastern
		CHECK(r.Quadrants.Groups[3].Planets() == 2);
	}
	SECTION("unweighted") {
		ChartAnalysisOptions options;
		options.Weighted = false;
		auto r = ChartAnalysis::Run(chart, Calculator(), options);
		CHECK(r.Elements.Groups[0].Score == 3);
	}
	SECTION("no houses") {
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 5), At(P::Moon, 40) }), Calculator());
		CHECK_FALSE(r.HasHouses);
		CHECK(r.Hemispheres.Groups.empty());
		CHECK(r.Quadrants.Groups.empty());
		CHECK_FALSE(r.Ruler);
		CHECK(r.Elements.Groups[0].Score == 2);		// no angles
	}
}

TEST_CASE("Dignities and receptions", "[ChartAnalysis]") {
	using enum ZodiacSign;
	auto has = [](DignityData const& d, Dignity x) { return std::ranges::find(d.Dignities, x) != d.Dignities.end(); };
	constexpr auto Trad = RulerSystem::Traditional;
	SECTION("the major dignities") {
		// 15 Leo by night: the Sun's domicile only (Jupiter's night triplicity, Saturn's term, Jupiter's face)
		auto d = ChartAnalysis::DignityOf(P::Sun, AstroPoint(135), Trad, false);
		CHECK(d.Dignities == std::vector{ Dignity::Domicile });
		CHECK(d.Score == 5);
		// 15 Aquarius: detriment, and no dignity at all - peregrine as well
		d = ChartAnalysis::DignityOf(P::Sun, AstroPoint(315), Trad, false);
		CHECK(d.Dignities == std::vector{ Dignity::Detriment });
		CHECK(d.Peregrine);
		CHECK(d.Score == -10);
		// 15 Aries: Saturn's fall, but the participating ruler of the fire triplicity
		d = ChartAnalysis::DignityOf(P::Saturn, AstroPoint(15), Trad, true);
		CHECK(d.Dignities == std::vector{ Dignity::Triplicity, Dignity::Fall });
		CHECK(d.Score == -1);
		// 15 Virgo: domicile and exaltation (Venus's term and face)
		d = ChartAnalysis::DignityOf(P::Mercury, AstroPoint(165), Trad, true);
		CHECK(d.Dignities == std::vector{ Dignity::Domicile, Dignity::Exaltation });
		CHECK(d.Score == 9);
		// 5 Gemini: Mercury's term, Jupiter's face - nothing for Mars
		d = ChartAnalysis::DignityOf(P::Mars, AstroPoint(65), Trad, true);
		CHECK(d.Dignities.empty());
		CHECK(d.Peregrine);
		CHECK(d.Score == -5);
	}
	SECTION("the minor dignities") {
		// the Egyptian terms give each planet its years: Saturn 57, Jupiter 79, Mars 66, Venus 82, Mercury 76
		std::map<P, int> years;
		for (int degree = 0; degree < 360; degree++)
			years[ChartAnalysis::TermRuler(AstroPoint(degree + 0.5))]++;
		CHECK(years[P::Saturn] == 57);
		CHECK(years[P::Jupiter] == 79);
		CHECK(years[P::Mars] == 66);
		CHECK(years[P::Venus] == 82);
		CHECK(years[P::Mercury] == 76);
		CHECK(ChartAnalysis::TermRuler(AstroPoint(5.99)) == P::Jupiter);
		CHECK(ChartAnalysis::TermRuler(AstroPoint(6)) == P::Venus);
		CHECK(ChartAnalysis::TermRuler(AstroPoint(359.9)) == P::Saturn);
		// the faces in the Chaldean order from Mars, which also ends the zodiac
		CHECK(ChartAnalysis::FaceRuler(AstroPoint(5)) == P::Mars);
		CHECK(ChartAnalysis::FaceRuler(AstroPoint(15)) == P::Sun);
		CHECK(ChartAnalysis::FaceRuler(AstroPoint(25)) == P::Venus);
		CHECK(ChartAnalysis::FaceRuler(AstroPoint(35)) == P::Mercury);
		CHECK(ChartAnalysis::FaceRuler(AstroPoint(355)) == P::Mars);
		// the Moon in Taurus: its night triplicity, not by day; with the sect unknown both count
		auto has = [](DignityData const& d, Dignity x) { return std::ranges::find(d.Dignities, x) != d.Dignities.end(); };
		CHECK(has(ChartAnalysis::DignityOf(P::Moon, AstroPoint(45), Trad, false), Dignity::Triplicity));
		CHECK_FALSE(has(ChartAnalysis::DignityOf(P::Moon, AstroPoint(45), Trad, true), Dignity::Triplicity));
		CHECK(has(ChartAnalysis::DignityOf(P::Moon, AstroPoint(45), Trad, std::nullopt), Dignity::Triplicity));
		// Jupiter at 3 Aries: its own term
		auto d = ChartAnalysis::DignityOf(P::Jupiter, AstroPoint(3), Trad, true);
		CHECK(has(d, Dignity::Term));
		CHECK(d.TermRuler == P::Jupiter);
		CHECK(d.FaceRuler == P::Mars);
		CHECK(d.TriplicityRulers == std::array{ P::Sun, P::Jupiter, P::Saturn });
	}
	SECTION("modern rulers add the outer planets and keep the old ones") {
		CHECK(has(ChartAnalysis::DignityOf(P::Mars, AstroPoint(215), RulerSystem::Modern), Dignity::Domicile));
		CHECK(has(ChartAnalysis::DignityOf(P::Pluto, AstroPoint(215), RulerSystem::Modern), Dignity::Domicile));
		CHECK(has(ChartAnalysis::DignityOf(P::Uranus, AstroPoint(125), RulerSystem::Modern), Dignity::Detriment));
		CHECK_FALSE(ChartAnalysis::HasDignities(P::Pluto, RulerSystem::Traditional));
		CHECK(ChartAnalysis::HasDignities(P::Pluto, RulerSystem::Modern));
		// the outer planets have no minor dignities, so they are never peregrine
		auto pluto = ChartAnalysis::DignityOf(P::Pluto, AstroPoint(65), RulerSystem::Modern);
		CHECK_FALSE(pluto.Peregrine);
		CHECK(pluto.Score == 0);
	}
	SECTION("in a chart") {
		// Venus in Aries and Mars in Libra: a mutual reception; Pluto in Aquarius, Uranus in Scorpio: only with modern rulers
		auto chart = Chart({ At(P::Venus, 10), At(P::Mars, 190), At(P::Uranus, 220), At(P::Pluto, 310) });
		auto r = ChartAnalysis::Run(chart, Calculator());
		CHECK(r.Dignities.size() == 2);
		REQUIRE(r.Receptions.size() == 1);
		CHECK(r.Receptions[0].A == P::Venus);
		CHECK(r.Receptions[0].B == P::Mars);
		CHECK_FALSE(r.Receptions[0].Exaltation);

		ChartAnalysisOptions modern;
		modern.Rulers = RulerSystem::Modern;
		r = ChartAnalysis::Run(chart, Calculator(), modern);
		CHECK(r.Dignities.size() == 4);
		CHECK(r.Receptions.size() == 2);
	}
	SECTION("by exaltation") {
		// the Sun in Taurus (the Moon's exaltation) and the Moon in Aries (the Sun's)
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 40), At(P::Moon, 10) }), Calculator());
		REQUIRE(r.Receptions.size() == 1);
		CHECK(r.Receptions[0].Exaltation);
	}
}

TEST_CASE("Dispositors", "[ChartAnalysis]") {
	SECTION("a final dispositor") {
		// Sun in Leo (its own), Moon in Leo, Mercury in Cancer (-> Moon -> Sun)
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 130), At(P::Moon, 140), At(P::Mercury, 100) }), Calculator());
		CHECK(r.Dispositors.Final == P::Sun);
		REQUIRE(r.Dispositors.Ends.size() == 1);
		CHECK(r.Dispositors.Nodes[2].Ruler == P::Moon);
	}
	SECTION("a loop") {
		// Venus in Aries, Mars in Libra, the Sun in Taurus (-> Venus)
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 40), At(P::Venus, 10), At(P::Mars, 190) }), Calculator());
		CHECK_FALSE(r.Dispositors.Final);
		REQUIRE(r.Dispositors.Ends.size() == 1);
		CHECK(r.Dispositors.Ends[0] == std::vector{ P::Venus, P::Mars });
	}
	SECTION("a ruler that isn't there ends a chain") {
		// Mercury in Leo, no Sun
		auto r = ChartAnalysis::Run(Chart({ At(P::Mercury, 130), At(P::Jupiter, 250) }), Calculator());
		CHECK_FALSE(r.Dispositors.Nodes[0].RulerInChart);
		CHECK_FALSE(r.Dispositors.Final);		// Jupiter is in its own sign, but Mercury's chain doesn't reach it
	}
	SECTION("modern rulers change the chains") {
		// Moon in Scorpio, Mars in Aries, Pluto in Aries
		auto chart = Chart({ At(P::Moon, 220), At(P::Mars, 10), At(P::Pluto, 20) });
		auto r = ChartAnalysis::Run(chart, Calculator());
		CHECK(r.Dispositors.Final == P::Mars);
		ChartAnalysisOptions modern;
		modern.Rulers = RulerSystem::Modern;
		r = ChartAnalysis::Run(chart, Calculator(), modern);
		CHECK(r.Dispositors.Nodes[0].Ruler == P::Pluto);
		CHECK(r.Dispositors.Final == P::Mars);		// Moon -> Pluto -> Mars
	}
	SECTION("the chart ruler") {
		// Ascendant in Scorpio, Mars in Capricorn
		auto chart = Chart({ At(P::Mars, 280), At(P::Pluto, 200) }, 215);
		auto r = ChartAnalysis::Run(chart, Calculator());
		REQUIRE(r.Ruler);
		CHECK(r.Ruler->Body == P::Mars);
		CHECK(r.Ruler->Sign == ZodiacSign::Capricorn);
		CHECK(r.Ruler->House == 3);
		ChartAnalysisOptions modern;
		modern.Rulers = RulerSystem::Modern;
		CHECK(ChartAnalysis::Run(chart, Calculator(), modern).Ruler->Body == P::Pluto);
	}
}

namespace {
	// ten planets (Sun to Pluto) at these longitudes
	std::vector<PlanetPosition> Ten(std::initializer_list<double> longitudes) {
		std::vector<PlanetPosition> result;
		int i = 0;
		for (double longitude : longitudes)
			result.push_back(At(static_cast<P>(i++), longitude));
		return result;
	}
}

TEST_CASE("Chart shape", "[ChartAnalysis]") {
	auto shape = [](std::initializer_list<double> longitudes) { return ChartAnalysis::ShapeOf(Ten(longitudes)); };
	SECTION("bundle") {
		auto s = shape({ 10, 20, 30, 40, 50, 60, 70, 80, 90, 125 });
		REQUIRE(s);
		CHECK(s->Shape == ChartShape::Bundle);
		CHECK(s->Leading == P::Sun);
		CHECK(s->Span == Approx(115));
	}
	SECTION("bowl, led by the planet after the empty half") {
		auto s = shape({ 200, 220, 240, 260, 280, 300, 320, 340, 0, 15 });
		REQUIRE(s);
		CHECK(s->Shape == ChartShape::Bowl);
		CHECK(s->Leading == P::Sun);
	}
	SECTION("bucket") {
		auto s = shape({ 0, 15, 30, 50, 70, 90, 110, 130, 150, 260 });
		REQUIRE(s);
		CHECK(s->Shape == ChartShape::Bucket);
		CHECK(s->Handle == P::Pluto);
		CHECK(s->Leading == P::Sun);
	}
	SECTION("locomotive") {
		auto s = shape({ 0, 25, 50, 75, 100, 125, 150, 175, 200, 225 });
		REQUIRE(s);
		CHECK(s->Shape == ChartShape::Locomotive);
		// (an empty space a little short of a trine still counts)
		s = shape({ 0, 25, 50, 75, 100, 125, 150, 175, 200, 245 });
		REQUIRE(s);
		CHECK(s->Shape == ChartShape::Locomotive);
		CHECK(s->Leading == P::Sun);
	}
	SECTION("seesaw") {
		auto s = shape({ 0, 20, 40, 60, 80, 180, 200, 220, 240, 260 });
		REQUIRE(s);
		CHECK(s->Shape == ChartShape::Seesaw);
		CHECK_FALSE(s->Leading);
	}
	SECTION("splash and splay") {
		auto s = shape({ 5, 41, 77, 113, 149, 185, 221, 257, 293, 329 });
		REQUIRE(s);
		CHECK(s->Shape == ChartShape::Splash);
		s = shape({ 0, 5, 10, 100, 105, 110, 200, 205, 210, 300 });
		REQUIRE(s);
		CHECK(s->Shape == ChartShape::Splay);
	}
	SECTION("too few planets") {
		CHECK_FALSE(ChartAnalysis::ShapeOf({ At(P::Sun, 0), At(P::Moon, 100) }));
	}
}

TEST_CASE("Conditions of the planets", "[ChartAnalysis]") {
	SECTION("unaspected and retrograde") {
		// Sun trine Moon; Mars only semi-square the Sun (a minor aspect); Saturn going backward
		auto saturn = At(P::Saturn, 300);
		saturn.Speed = -0.05;
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 0), At(P::Moon, 120), At(P::Mars, 45), saturn }), Calculator());
		CHECK(r.Unaspected == std::vector{ P::Mars });
		CHECK(r.Retrograde == std::vector{ P::Saturn });
	}
	SECTION("on the angles, the tightest first") {
		// Ascendant 100, Midheaven 10: the Sun 1 from the IC, Venus 2 from the Descendant, Mars 3 from the Ascendant, Jupiter far
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 189), At(P::Venus, 282), At(P::Mars, 103), At(P::Jupiter, 50) }, 100, 10), Calculator());
		REQUIRE(r.OnAngles.size() == 3);
		CHECK(r.OnAngles[0].Body == P::Sun);
		CHECK(r.OnAngles[0].Angle == AngleKind::Imum);
		CHECK(r.OnAngles[0].Orb == Approx(1));
		CHECK(r.OnAngles[1].Angle == AngleKind::Descendant);
		CHECK(r.OnAngles[2].Angle == AngleKind::Ascendant);
	}
	SECTION("sect and houses") {
		// equal houses from 100: the Sun at 300 is in house 7, above the horizon
		auto r = ChartAnalysis::Run(Chart({ At(P::Sun, 300), At(P::Moon, 105), At(P::Mars, 135) }, 100, 10), Calculator());
		CHECK(r.DayChart == true);
		REQUIRE(r.Houses.Groups.size() == 12);
		CHECK(r.Houses.Groups[0].Planets() == 1);		// the Moon
		CHECK(r.Houses.Groups[1].Planets() == 1);		// Mars
		CHECK(r.Houses.Groups[6].Planets() == 1);		// the Sun
		CHECK(r.Angularity.Groups[0].Planets() == 2);	// angular: houses 1 and 7
		CHECK(r.Angularity.Groups[1].Planets() == 1);
		CHECK_FALSE(ChartAnalysis::Run(Chart({ At(P::Sun, 300) }), Calculator()).DayChart);
	}
}
