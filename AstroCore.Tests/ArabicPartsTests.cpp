#include "TestCommon.h"
#include "ArabicParts.h"
#include "DerivedCharts.h"
#include "Midpoints.h"
#include <algorithm>
#include <cmath>

namespace {
	PlanetPosition At(Planet planet, double longitude) {
		return PlanetPosition{ .Longitude = AstroPoint(longitude), .Speed = 0, .Latitude = 0, .LatitudeSpeed = 0, .Planet = planet };
	}

	// A chart made by hand: the Ascendant at 100 (10 Cancer), equal houses from it (cusp n at 100 + 30 (n - 1)), the Midheaven at
	// 10, the Moon at 50, Venus at 25 and the Sun where the test says - 300 is above the horizon (house 8), 250 below it (house 5).
	ChartData Chart(double sun, bool venus = true) {
		ChartData chart;
		chart.AddPlanets({ At(Planet::Sun, sun), At(Planet::Moon, 50) });
		if (venus)
			chart.AddPlanets({ At(Planet::Venus, 25) });
		chart.AddPlanets({ At(Planet::Mars, 200), At(Planet::Jupiter, 330), At(Planet::Saturn, 170) });
		auto& houses = chart.Houses();
		houses.Asc = 100;
		houses.MC = 10;
		for (int i = 0; i < 12; i++)
			houses.Cusps[i] = 100 + 30 * i;
		return chart;
	}

	PartData const& Part(std::vector<PartData> const& parts, PCWSTR name) {
		auto it = std::ranges::find(parts, std::wstring(name), &PartData::Name);
		REQUIRE(it != parts.end());
		return *it;
	}

	double Diff(double a, double b) {
		return AstroPoint::Diff(AstroPoint(a), AstroPoint(b));
	}
}

TEST_CASE("Day and night charts", "[ArabicParts]") {
	// the Ascendant at 100, the Descendant at 280: the Sun is up from 280 forward through 0 to 100
	CHECK(ArabicParts::IsDay(AstroPoint(300), AstroPoint(100)));
	CHECK(ArabicParts::IsDay(AstroPoint(0), AstroPoint(100)));
	CHECK(ArabicParts::IsDay(AstroPoint(90), AstroPoint(100)));		// just before the Ascendant: house 12
	CHECK(ArabicParts::IsDay(AstroPoint(290), AstroPoint(100)));		// just after the Descendant: house 7
	CHECK_FALSE(ArabicParts::IsDay(AstroPoint(110), AstroPoint(100)));	// house 1
	CHECK_FALSE(ArabicParts::IsDay(AstroPoint(200), AstroPoint(100)));
	CHECK_FALSE(ArabicParts::IsDay(AstroPoint(270), AstroPoint(100)));	// house 6
	// an Ascendant near 0 Aries: the half that is up runs through 180
	CHECK(ArabicParts::IsDay(AstroPoint(200), AstroPoint(10)));
	CHECK_FALSE(ArabicParts::IsDay(AstroPoint(20), AstroPoint(10)));
	CHECK(ArabicParts::IsDay(AstroPoint(5), AstroPoint(10)));

	CHECK(ArabicParts::IsDayChart(Chart(300)) == true);
	CHECK(ArabicParts::IsDayChart(Chart(250)) == false);
	ChartData noSun;
	noSun.AddPlanets({ At(Planet::Moon, 50) });
	CHECK_FALSE(ArabicParts::IsDayChart(noSun).has_value());
}

TEST_CASE("The Part of Fortune and the Part of Spirit", "[ArabicParts]") {
	SECTION("by day") {
		auto parts = ArabicParts::Calculate(Chart(300));
		auto& fortune = Part(parts, L"Fortune");
		CHECK(fortune.Longitude.Value == Approx(210));		// 100 + 50 - 300
		CHECK_FALSE(fortune.Night);
		CHECK_FALSE(fortune.Reversed);
		CHECK(fortune.House == 4);		// 190 to 220
		CHECK(fortune.Formula == L"Asc + Moon - Sun");
		auto& spirit = Part(parts, L"Spirit");
		CHECK(spirit.Longitude.Value == Approx(350));		// 100 + 300 - 50
		CHECK(spirit.Formula == L"Asc + Sun - Moon");
	}
	SECTION("by night they change places") {
		auto parts = ArabicParts::Calculate(Chart(250));
		auto& fortune = Part(parts, L"Fortune");
		CHECK(fortune.Longitude.Value == Approx(300));		// 100 + 250 - 50
		CHECK(fortune.Night);
		CHECK(fortune.Reversed);
		CHECK(fortune.Formula == L"Asc + Sun - Moon");
		auto& spirit = Part(parts, L"Spirit");
		CHECK(spirit.Longitude.Value == Approx(260));		// 100 + 50 - 250, wrapped
		CHECK(spirit.Formula == L"Asc + Moon - Sun");
	}
	SECTION("Fortune by day is Spirit by night and the other way round") {
		auto day = ArabicParts::Calculate(Chart(300));
		auto asDay = ArabicParts::Calculate(Chart(300), PartOptions{ .Sect = SectMode::Night });
		CHECK(Part(asDay, L"Fortune").Longitude.Value == Approx(Part(day, L"Spirit").Longitude.Value));
		CHECK(Part(asDay, L"Spirit").Longitude.Value == Approx(Part(day, L"Fortune").Longitude.Value));
	}
}

TEST_CASE("Sect can be chosen and turned off", "[ArabicParts]") {
	// a day chart taken as a night one
	auto night = ArabicParts::Calculate(Chart(300), PartOptions{ .Sect = SectMode::Night });
	CHECK(Part(night, L"Fortune").Night);
	CHECK(Part(night, L"Fortune").Longitude.Value == Approx(350));		// 100 + 300 - 50, turned round
	// a night chart taken as a day one
	auto day = ArabicParts::Calculate(Chart(250), PartOptions{ .Sect = SectMode::Day });
	CHECK_FALSE(Part(day, L"Fortune").Night);
	CHECK(Part(day, L"Fortune").Longitude.Value == Approx(AstroPoint(100 + 50 - 250).Value));		// 260
	// nothing turns round when that is off, though the chart is still a night chart
	auto same = ArabicParts::Calculate(Chart(250), PartOptions{ .ReverseAtNight = false });
	CHECK(Part(same, L"Fortune").Night);
	CHECK_FALSE(Part(same, L"Fortune").Reversed);
	CHECK(Part(same, L"Fortune").Longitude.Value == Approx(260));
}

TEST_CASE("Parts that use other parts", "[ArabicParts]") {
	SECTION("by day") {
		auto parts = ArabicParts::Calculate(Chart(300));
		// Eros: from Spirit (350) to Venus (25): 100 + 25 - 350 = -225
		CHECK(Part(parts, L"Eros").Longitude.Value == Approx(135));
		// Courage: from Mars (200) to Fortune (210); Victory: from Spirit to Jupiter (330); Nemesis: from Saturn (170) to Fortune
		CHECK(Part(parts, L"Courage").Longitude.Value == Approx(AstroPoint(100 + 210 - 200).Value));
		CHECK(Part(parts, L"Victory").Longitude.Value == Approx(AstroPoint(100 + 330 - 350).Value));
		CHECK(Part(parts, L"Nemesis").Longitude.Value == Approx(AstroPoint(100 + 210 - 170).Value));
	}
	SECTION("by night they use the turned parts, and turn round themselves") {
		auto parts = ArabicParts::Calculate(Chart(250));
		// Spirit is 260 now; Eros turns round: 100 + Spirit - Venus
		CHECK(Part(parts, L"Eros").Longitude.Value == Approx(AstroPoint(100 + 260 - 25).Value));
		// Fortune is 300; Courage turns round: 100 + Mars - Fortune
		CHECK(Part(parts, L"Courage").Longitude.Value == Approx(AstroPoint(100 + 200 - 300).Value).margin(1e-9));
	}
	SECTION("a part that comes before the one it uses is not calculated") {
		std::vector<PartDefinition> definitions{
			{ L"Early", PartPoint::Asc(), PartPoint::OtherPart(L"Late"), PartPoint::Of(Planet::Sun), PartReversal::Never },
			{ L"Late", PartPoint::Asc(), PartPoint::Of(Planet::Moon), PartPoint::Of(Planet::Sun), PartReversal::Never },
		};
		auto parts = ArabicParts::Calculate(Chart(300), definitions);
		REQUIRE(parts.size() == 1);
		CHECK(parts[0].Name == L"Late");
	}
}

TEST_CASE("Parts that stay the same at night", "[ArabicParts]") {
	// Marriage (Lilly): Asc + 7th cusp - Venus = 100 + 280 - 25 = 355; Death: Asc + 8th cusp - Moon = 100 + 310 - 50 = 360
	for (double sun : { 300.0, 250.0 }) {
		INFO("Sun at " << sun);
		auto parts = ArabicParts::Calculate(Chart(sun));
		CHECK(Part(parts, L"Marriage").Longitude.Value == Approx(355));
		CHECK_FALSE(Part(parts, L"Marriage").Reversed);
		CHECK(Part(parts, L"Marriage").Formula == L"Asc + cusp 7 - Venus");
		CHECK(Part(parts, L"Death").Longitude.Value == Approx(0).margin(1e-9));
	}
}

namespace {
	// the test chart with Mercury at 150, so that the parts that need it can be made
	ChartData WithMercury(double sun) {
		auto chart = Chart(sun);
		chart.AddPlanets({ At(Planet::Mercury, 150) });
		return chart;
	}

	// what a part comes to by day (the Sun at 300: Fortune 210, Spirit 350) and by night (the Sun at 250: Fortune 300, Spirit 260)
	void CheckPart(PCWSTR name, double byDay, double byNight, PCWSTR dayFormula = nullptr, PCWSTR nightFormula = nullptr) {
		std::string label;
		for (auto c = name; *c; ++c)
			label += static_cast<char>(*c);
		INFO(label);
		auto day = ArabicParts::Calculate(WithMercury(300));
		auto night = ArabicParts::Calculate(WithMercury(250));
		CHECK(Diff(Part(day, name).Longitude.Value, byDay) < 1e-9);
		CHECK(Diff(Part(night, name).Longitude.Value, byNight) < 1e-9);
		if (dayFormula)
			CHECK(Part(day, name).Formula == dayFormula);
		if (nightFormula)
			CHECK(Part(night, name).Formula == nightFormula);
	}
}

// The formulas as the sources give them (Wikipedia's table of the Hermetic lots; Paulus Alexandrinus, Valens, Dorotheus as
// given by Seven Stars Astrology and astrology-x-files.com; Lilly and al-Biruni for the medieval ones). Here Asc = 100, Moon 50,
// Venus 25, Mars 200, Jupiter 330, Saturn 170, Mercury 150.
TEST_CASE("The Hermetic lots", "[ArabicParts]") {
	// Necessity by day: Asc + Fortune - Mercury = 100 + 210 - 150; by night: Asc + Mercury - Fortune = 100 + 150 - 300
	CheckPart(L"Necessity", 160, 310, L"Asc + Fortune - Mercury", L"Asc + Mercury - Fortune");
	// Courage: Asc + Fortune - Mars by day, Asc + Mars - Fortune by night
	CheckPart(L"Courage", 110, 0, L"Asc + Fortune - Mars", L"Asc + Mars - Fortune");
	// Nemesis: Asc + Fortune - Saturn by day, Asc + Saturn - Fortune by night
	CheckPart(L"Nemesis", 140, 330, L"Asc + Fortune - Saturn", L"Asc + Saturn - Fortune");
	// Eros: Asc + Venus - Spirit by day (100 + 25 - 350), Asc + Spirit - Venus by night (100 + 260 - 25)
	CheckPart(L"Eros", 135, 335, L"Asc + Venus - Spirit", L"Asc + Spirit - Venus");
	// Victory: Asc + Jupiter - Spirit by day, Asc + Spirit - Jupiter by night
	CheckPart(L"Victory", 80, 30, L"Asc + Jupiter - Spirit", L"Asc + Spirit - Jupiter");
}

TEST_CASE("Exaltation uses the Sun by day and the Moon by night", "[ArabicParts]") {
	// Valens: Asc + 19 Aries - Sun by day (100 + 19 - 300), Asc + 3 Taurus - Moon by night (100 + 33 - 50)
	CheckPart(L"Exaltation", 179, 83, L"Asc + 19 Aries - Sun", L"Asc + 3 Taurus - Moon");
	// with the turn switched off it stays the day form
	auto same = ArabicParts::Calculate(WithMercury(250), PartOptions{ .ReverseAtNight = false });
	CHECK(Part(same, L"Exaltation").Formula == L"Asc + 19 Aries - Sun");
	CHECK_FALSE(Part(same, L"Exaltation").Reversed);
	CHECK(ArabicParts::Position(PartPoint::At(33), Chart(300))->Value == Approx(33));
	CHECK(ArabicParts::Describe(PartPoint::At(33)) == L"3 Taurus");
	CHECK(ArabicParts::Describe(PartPoint::At(359)) == L"29 Pisces");
}

TEST_CASE("Family parts", "[ArabicParts]") {
	// Father: from the Sun to Saturn, reversed at night; Mother: from Venus to the Moon, reversed at night
	CheckPart(L"Father", 330, 180, L"Asc + Saturn - Sun", L"Asc + Sun - Saturn");
	CheckPart(L"Mother", 125, 75, L"Asc + Moon - Venus", L"Asc + Venus - Moon");
	// Paulus does not turn these round: Brethren from Saturn to Jupiter, Children from Jupiter to Saturn, Sons from Jupiter to
	// Mercury, Daughters from Jupiter to Venus, and Marriage from Saturn to Venus for men, Venus to Saturn for women
	CheckPart(L"Brethren", 260, 260, L"Asc + Jupiter - Saturn", L"Asc + Jupiter - Saturn");
	CheckPart(L"Children", 300, 300, L"Asc + Saturn - Jupiter", L"Asc + Saturn - Jupiter");
	CheckPart(L"Sons", 280, 280, L"Asc + Mercury - Jupiter");
	CheckPart(L"Daughters", 155, 155, L"Asc + Venus - Jupiter");
	CheckPart(L"Marriage (men)", 315, 315, L"Asc + Venus - Saturn");
	CheckPart(L"Marriage (women)", 245, 245, L"Asc + Saturn - Venus");
}

TEST_CASE("Parts of misfortune, health and everyday matters", "[ArabicParts]") {
	// Affliction: from Saturn to Mars by day, Mars to Saturn by night; Sickness: the same both ways
	CheckPart(L"Affliction", 130, 70, L"Asc + Mars - Saturn", L"Asc + Saturn - Mars");
	CheckPart(L"Sickness", 130, 130);
	CheckPart(L"Debt", 120, 120, L"Asc + Saturn - Mercury");
	CheckPart(L"Discord", 230, 230, L"Asc + Jupiter - Mars");
	CheckPart(L"Servants", 0, 0, L"Asc + Moon - Mercury");
	// Merchandise: Asc + Fortune - Spirit (100 + 210 - 350); at night Fortune and Spirit are 300 and 260
	CheckPart(L"Merchandise", 320, 140, L"Asc + Fortune - Spirit");
	// Travel: Asc + 9th cusp (340) - the ruler of the 9th (Pisces: Jupiter, 330)
	CheckPart(L"Travel", 110, 110, L"Asc + cusp 9 - ruler of 9");
	// Destroyer: from the ruler of the Ascendant (the Moon, for Cancer) to the Moon
	CheckPart(L"Destroyer", 100, 100, L"Asc + Moon - ruler of 1", L"Asc + ruler of 1 - Moon");
}

TEST_CASE("Points a part can be made of", "[ArabicParts]") {
	auto chart = Chart(300);
	PartOptions options;
	auto position = [&](PartPoint const& point) { return ArabicParts::Position(point, chart, options); };
	CHECK(position(PartPoint::Of(Planet::Mars))->Value == Approx(200));
	CHECK(position(PartPoint::Asc())->Value == Approx(100));
	CHECK(position(PartPoint::MC())->Value == Approx(10));
	CHECK(position(PartPoint::Desc())->Value == Approx(280));
	CHECK(position(PartPoint::IC())->Value == Approx(190));
	CHECK(position(PartPoint::Cusp(1))->Value == Approx(100));
	CHECK(position(PartPoint::Cusp(12))->Value == Approx(70));		// 100 + 330, wrapped
	// a planet the chart doesn't have, a house that doesn't exist, a part that isn't there
	CHECK_FALSE(position(PartPoint::Of(Planet::Pluto)).has_value());
	CHECK_FALSE(position(PartPoint::Cusp(0)).has_value());
	CHECK_FALSE(position(PartPoint::Cusp(13)).has_value());
	CHECK_FALSE(position(PartPoint::RulerOfCusp(13)).has_value());
	CHECK_FALSE(position(PartPoint::OtherPart(L"Fortune")).has_value());

	// the ruler of a cusp: the sign on cusp 1 (10 Cancer) is ruled by the Moon (50); cusp 2 (130, Leo) by the Sun (300)
	CHECK(position(PartPoint::RulerOfCusp(1))->Value == Approx(50));
	CHECK(position(PartPoint::RulerOfCusp(2))->Value == Approx(300));
	// a ruler the chart doesn't have: cusp 3 is 160 (Virgo, Mercury)
	CHECK_FALSE(position(PartPoint::RulerOfCusp(3)).has_value());
	// a part already calculated
	std::vector<PartData> parts{ PartData{ .Name = L"Mine", .Longitude = AstroPoint(77) } };
	CHECK(ArabicParts::Position(PartPoint::OtherPart(L"Mine"), chart, options, parts)->Value == Approx(77));

	SECTION("a part built on them") {
		// Asc + ruler of the 1st - Sun, always: 100 + 50 - 300 = -150
		PartDefinition definition{ L"Test", PartPoint::Asc(), PartPoint::RulerOfCusp(1), PartPoint::Of(Planet::Sun), PartReversal::Never };
		auto part = ArabicParts::Calculate(chart, definition);
		REQUIRE(part.has_value());
		CHECK(part->Longitude.Value == Approx(210));
		CHECK(part->Formula == L"Asc + ruler of 1 - Sun");
		// Descendant and IC as a base and as a subtrahend
		definition = { L"Test2", PartPoint::Desc(), PartPoint::IC(), PartPoint::MC(), PartReversal::Never };
		part = ArabicParts::Calculate(chart, definition);
		CHECK(part->Longitude.Value == Approx(AstroPoint(280 + 190 - 10).Value));
		CHECK(part->Formula == L"Desc + IC - MC");
	}
}

TEST_CASE("Sign rulers", "[ArabicParts]") {
	const Planet traditional[] = { Planet::Mars, Planet::Venus, Planet::Mercury, Planet::Moon, Planet::Sun, Planet::Mercury,
		Planet::Venus, Planet::Mars, Planet::Jupiter, Planet::Saturn, Planet::Saturn, Planet::Jupiter };
	for (int i = 0; i < 12; i++)
		CHECK(ArabicParts::Ruler(ZodiacSign(i)) == traditional[i]);
	CHECK(ArabicParts::Ruler(ZodiacSign::Scorpio, true) == Planet::Pluto);
	CHECK(ArabicParts::Ruler(ZodiacSign::Aquarius, true) == Planet::Uranus);
	CHECK(ArabicParts::Ruler(ZodiacSign::Pisces, true) == Planet::Neptune);
	CHECK(ArabicParts::Ruler(ZodiacSign::Aries, true) == Planet::Mars);		// the others don't change
	CHECK(ArabicParts::Ruler(ZodiacSign::Capricorn, true) == Planet::Saturn);

	// the option reaches RulerOfCusp: cusp 2 is 130 (Leo), so move the cusp to Pisces and give the chart Neptune
	auto chart = Chart(300);
	chart.Houses().Cusps[0] = 340;		// Pisces
	chart.AddPlanets({ At(Planet::Neptune, 123) });
	CHECK(ArabicParts::Position(PartPoint::RulerOfCusp(1), chart)->Value == Approx(330));		// Jupiter
	CHECK(ArabicParts::Position(PartPoint::RulerOfCusp(1), chart, PartOptions{ .ModernRulers = true })->Value == Approx(123));
}

TEST_CASE("Parts a chart can't have are left out", "[ArabicParts]") {
	auto parts = ArabicParts::Calculate(Chart(300, /*venus*/ false));
	auto has = [&](PCWSTR name) { return std::ranges::any_of(parts, [&](auto const& p) { return p.Name == name; }); };
	CHECK(has(L"Fortune"));
	CHECK(has(L"Spirit"));
	CHECK(has(L"Courage"));
	CHECK_FALSE(has(L"Eros"));			// Venus
	CHECK_FALSE(has(L"Mother"));		// Venus
	CHECK_FALSE(has(L"Marriage"));		// Venus
	CHECK(has(L"Death"));

	// no Sun at all: nothing that needs it, and the rest are counted as day
	ChartData chart;
	chart.AddPlanets({ At(Planet::Moon, 50), At(Planet::Venus, 25) });
	chart.Houses().Asc = 100;
	for (int i = 0; i < 12; i++)
		chart.Houses().Cusps[i] = 100 + 30 * i;
	parts = ArabicParts::Calculate(chart);
	CHECK_FALSE(has(L"Fortune"));
	REQUIRE(has(L"Mother"));
	CHECK_FALSE(Part(parts, L"Mother").Night);
	CHECK(Part(parts, L"Mother").Longitude.Value == Approx(125));		// 100 + 50 - 25 (from Venus to the Moon)

	CHECK(ArabicParts::Calculate(ChartData{}).empty());
}

TEST_CASE("Describing a formula", "[ArabicParts]") {
	auto const& fortune = ArabicParts::Standard()[0];
	CHECK(fortune.Name == L"Fortune");
	CHECK(ArabicParts::Describe(fortune) == L"Asc + Moon - Sun");
	CHECK(ArabicParts::Describe(fortune, true) == L"Asc + Sun - Moon");
	CHECK(ArabicParts::Describe(PartPoint::Of(Planet::TrueNode)) == L"True Node");
	CHECK(ArabicParts::Describe(PartPoint::OtherPart(L"Spirit")) == L"Spirit");
}

TEST_CASE("The standard parts", "[ArabicParts]") {
	auto const& standard = ArabicParts::Standard();
	CHECK(standard.size() == 26);
	CHECK(standard[0].Name == L"Fortune");
	// names are unique, and a part only builds on ones before it (so the standard list can be calculated in order)
	for (size_t i = 0; i < standard.size(); i++) {
		for (size_t j = 0; j < i; j++)
			CHECK(standard[i].Name != standard[j].Name);
		for (auto const* point : { &standard[i].Base, &standard[i].Plus, &standard[i].Minus }) {
			if (point->Kind != PartPointKind::Part)
				continue;
			bool earlier = std::ranges::any_of(standard.begin(), standard.begin() + i, [&](auto const& d) { return d.Name == point->Part; });
			CHECK(earlier);
		}
	}
	// the test chart has no Mercury: Necessity, Sons, Debt and Servants can't be made
	CHECK(ArabicParts::Calculate(Chart(300)).size() == 22);
}

TEST_CASE("Parts of a real chart", "[ArabicParts]") {
	// noon at Greenwich on 1 January 2000, then every three hours: the sect from the Sun's longitude agrees with the house it is in
	AstroCalculator calc;
	for (int hour = 0; hour < 24; hour += 3) {
		ChartData chart;
		chart.AddPlanets(std::vector<Planet>{ Planet::Sun, Planet::Moon, Planet::Mercury, Planet::Venus, Planet::Mars, Planet::Jupiter,
			Planet::Saturn });
		chart.Info().Time = DateTime(2000, 1, 1, hour, 0, 0);
		chart.Info().Latitude = 51.5;
		chart.Info().Longitude = 0;
		chart.SetHouseSystem(HouseSystem::Placidus);
		calc.Calculate(chart);
		INFO("hour " << hour);

		auto sun = chart.GetPlanet(0).Longitude, moon = chart.GetPlanet(1).Longitude;
		auto asc = chart.Houses().Asc;
		int sunHouse = DerivedCharts::HouseOf(chart.Houses(), sun);
		REQUIRE(sunHouse != 0);
		CHECK(ArabicParts::IsDayChart(chart).value() == (sunHouse >= 7));

		auto parts = ArabicParts::Calculate(chart);
		REQUIRE(parts.size() == ArabicParts::Standard().size());		// (all seven traditional planets: every part can be made)
		auto& fortune = Part(parts, L"Fortune");
		auto& spirit = Part(parts, L"Spirit");
		bool day = sunHouse >= 7;
		CHECK(fortune.Night == !day);
		CHECK(Diff(fortune.Longitude, day ? asc + moon - sun : asc + sun - moon) < 1e-9);
		CHECK(Diff(spirit.Longitude, day ? asc + sun - moon : asc + moon - sun) < 1e-9);
		// Fortune and Spirit are always as far from the Ascendant as the other is, on the other side of it
		CHECK(Diff(AstroPoint(fortune.Longitude.Value - asc.Value + spirit.Longitude.Value - asc.Value).Value, 0) < 1e-9);
		CHECK(fortune.House >= 1);
		CHECK(fortune.House == DerivedCharts::HouseOf(chart.Houses(), fortune.Longitude));
	}
}

TEST_CASE("Aspects between the parts and the planets", "[ArabicParts]") {
	// Fortune by day is at 210; the planets are the Sun 300, Moon 50, Venus 25, Mars 200, Jupiter 330, Saturn 170
	auto chart = Chart(300);
	auto fortune = ArabicParts::Calculate(chart, ArabicParts::Standard()[0]);
	REQUIRE(fortune.has_value());
	REQUIRE(fortune->Longitude.Value == Approx(210));
	std::vector<PartData> parts{ *fortune };
	auto planets = chart.AllPlanets();
	auto typeOf = [](std::vector<PartAspect> const& aspects, Planet planet) {
		auto it = std::ranges::find_if(aspects, [&](auto const& a) { return a.Planet.Planet == planet; });
		return it == aspects.end() ? AspectType::None : it->Type;
	};

	SECTION("with the default orbs") {
		auto aspects = ArabicParts::Aspects(parts, planets, AspectCalculator());
		// square to the Sun (90 away), opposition to Venus (185 away: 5 off), trine to Jupiter (120), novile to Saturn (40)
		REQUIRE(aspects.size() == 4);
		CHECK(aspects[0].Planet.Planet == Planet::Sun);
		CHECK(aspects[0].Type == AspectType::Square);
		CHECK(aspects[0].Orb == Approx(0).margin(1e-4));
		CHECK(aspects[0].Angle == Approx(90));
		CHECK(aspects[0].Part == 0);
		CHECK(aspects[1].Planet.Planet == Planet::Venus);
		CHECK(aspects[1].Type == AspectType::Opposition);
		CHECK(aspects[1].Orb == Approx(5));
		CHECK(aspects[1].MaxOrb == Approx(8));
		CHECK(aspects[2].Planet.Planet == Planet::Jupiter);
		CHECK(aspects[2].Type == AspectType::Trine);
		CHECK(aspects[3].Planet.Planet == Planet::Saturn);
		CHECK(aspects[3].Type == AspectType::Novile);
		CHECK(aspects[3].MaxOrb == Approx(2));
		// nothing to the Moon (160 away) or Mars (10 away: outside the 8 degrees of a conjunction)
		CHECK(typeOf(aspects, Planet::Moon) == AspectType::None);
		CHECK(typeOf(aspects, Planet::Mars) == AspectType::None);
	}
	SECTION("the settings decide") {
		AspectSettings settings;
		settings.MajorOnly = true;
		CHECK(typeOf(ArabicParts::Aspects(parts, planets, AspectCalculator(settings)), Planet::Saturn) == AspectType::None);

		settings = AspectSettings();
		settings.PlanetEnabled[static_cast<int>(Planet::Venus)] = false;
		CHECK(typeOf(ArabicParts::Aspects(parts, planets, AspectCalculator(settings)), Planet::Venus) == AspectType::None);

		settings = AspectSettings();
		settings.AspectEnabled[static_cast<int>(AspectType::Trine)] = false;
		CHECK(typeOf(ArabicParts::Aspects(parts, planets, AspectCalculator(settings)), Planet::Jupiter) == AspectType::None);

		// the opposition to Venus is 5 off: a 4 degree orb is too tight, unless Venus has 2 more
		settings = AspectSettings();
		settings.MajorAspectOrb = 4;
		CHECK(typeOf(ArabicParts::Aspects(parts, planets, AspectCalculator(settings)), Planet::Venus) == AspectType::None);
		settings.PlanetOrbAdd[static_cast<int>(Planet::Venus)] = 2;
		auto wider = ArabicParts::Aspects(parts, planets, AspectCalculator(settings));
		REQUIRE(typeOf(wider, Planet::Venus) == AspectType::Opposition);
		CHECK(std::ranges::find_if(wider, [](auto const& a) { return a.Planet.Planet == Planet::Venus; })->MaxOrb == Approx(6));
	}
	SECTION("each part on its own") {
		// a second part at 120: opposite the Sun (300), and 80 from Mars (200)
		PartData second;
		second.Name = L"Second";
		second.Longitude = AstroPoint(120);
		parts.push_back(second);
		auto aspects = ArabicParts::Aspects(parts, planets, AspectCalculator());
		bool any = false;
		for (auto const& a : aspects) {
			CHECK(a.Part < 2);
			any = any || a.Part == 1;
			CHECK(Diff(parts[a.Part].Longitude.Value, a.Planet.Longitude.Value) == Approx(a.Angle).margin(1e-3));
		}
		CHECK(any);
		// part 0's aspects come first, in the order of the planets
		CHECK(aspects.front().Part == 0);
	}
	SECTION("nothing to work with") {
		CHECK(ArabicParts::Aspects({}, planets, AspectCalculator()).empty());
		CHECK(ArabicParts::Aspects(parts, {}, AspectCalculator()).empty());
	}
}

TEST_CASE("The Part of Fortune as a point of a chart", "[ArabicParts][Fortune]") {
	SECTION("worked out from the Sun, the Moon and the Ascendant") {
		// the hand-made chart of the tests above: the Ascendant at 100, the Moon at 50; by day the Sun at 300, by night at 250
		auto day = Chart(300);
		day.AddPlanets({ Planet::PartOfFortune });
		day.UpdatePartOfFortune();
		REQUIRE(day.AllPlanets().back().Planet == Planet::PartOfFortune);
		CHECK(day.AllPlanets().back().Longitude.Value == Approx(210));		// 100 + 50 - 300

		auto night = Chart(250);
		night.AddPlanets({ Planet::PartOfFortune });
		night.UpdatePartOfFortune();
		CHECK(night.AllPlanets().back().Longitude.Value == Approx(300));	// 100 + 250 - 50
		// it is the part the parts calculate
		CHECK(night.AllPlanets().back().Longitude.Value == Approx(Part(ArabicParts::Calculate(night), L"Fortune").Longitude.Value));
	}
	SECTION("only if it is asked for, and only if the chart has what it needs") {
		auto chart = Chart(300);
		auto before = chart.AllPlanets().size();
		chart.UpdatePartOfFortune();
		CHECK(chart.AllPlanets().size() == before);		// (nothing added)

		ChartData noSun;
		noSun.AddPlanets({ At(Planet::Moon, 50), At(Planet::PartOfFortune, 7) });
		noSun.Houses().Asc = 100;
		noSun.UpdatePartOfFortune();
		CHECK(noSun.AllPlanets()[1].Longitude.Value == Approx(7));		// (left as it was)
	}
	SECTION("the calculator does not ask the ephemeris for it") {
		AstroCalculator calc;
		auto point = calc.CalcPlanet(Planet::PartOfFortune, DateTime(2000, 1, 1, 12, 0, 0));
		CHECK(point.Planet == Planet::PartOfFortune);
		CHECK(point.Longitude.Value == 0);
		CHECK(point.Speed == 0);
	}
}

TEST_CASE("The Part of Fortune follows the chart's calculation", "[ArabicParts][Fortune]") {
	auto chartFor = [](int hour, int harmonic) {
		ChartData chart;
		chart.AddPlanets(std::vector<Planet>{ Planet::Sun, Planet::Moon, Planet::Mercury, Planet::PartOfFortune });
		chart.Info().Time = DateTime(2000, 1, 1, hour, 0, 0);
		chart.Info().Latitude = 51.5;
		chart.Info().Longitude = 0;
		chart.SetHouseSystem(HouseSystem::Placidus);
		chart.Harmonic(harmonic);
		return chart;
	};
	AstroCalculator calc;

	for (int hour : { 2, 12, 22 }) {
		INFO("hour " << hour);
		auto chart = chartFor(hour, 1);
		calc.Calculate(chart);
		auto fortune = std::ranges::find(chart.AllPlanets(), Planet::PartOfFortune, &PlanetPosition::Planet);
		REQUIRE(fortune != chart.AllPlanets().end());
		// as the parts calculate it (which needs the Sun and the Moon of the chart, and not the point itself)
		auto expected = ArabicParts::Calculate(chart, ArabicParts::Standard()[0]);
		REQUIRE(expected.has_value());
		CHECK(Diff(fortune->Longitude.Value, expected->Longitude.Value) < 1e-9);
	}

	SECTION("CalcPlanets and CalcHouses keep it right on their own") {
		auto chart = chartFor(12, 1);
		calc.Calculate(chart);
		double first = std::ranges::find(chart.AllPlanets(), Planet::PartOfFortune, &PlanetPosition::Planet)->Longitude.Value;
		chart.Info().Time = DateTime(2000, 1, 1, 15, 30, 0);
		chart.CalcPlanets(calc);
		chart.CalcHouses(calc);
		double second = std::ranges::find(chart.AllPlanets(), Planet::PartOfFortune, &PlanetPosition::Planet)->Longitude.Value;
		CHECK(Diff(first, second) > 1);
		auto expected = ArabicParts::Calculate(chart, ArabicParts::Standard()[0]);
		CHECK(Diff(second, expected->Longitude.Value) < 1e-9);
	}
	SECTION("in a harmonic chart it is the harmonic of the real one") {
		auto plain = chartFor(10, 1);
		calc.Calculate(plain);
		auto third = chartFor(10, 3);
		calc.Calculate(third);
		double real = std::ranges::find(plain.AllPlanets(), Planet::PartOfFortune, &PlanetPosition::Planet)->Longitude.Value;
		double harmonic = std::ranges::find(third.AllPlanets(), Planet::PartOfFortune, &PlanetPosition::Planet)->Longitude.Value;
		CHECK(Diff(harmonic, AstroPoint(real * 3).Value) < 1e-6);
	}
}

TEST_CASE("The Part of Fortune in derived charts", "[ArabicParts][Fortune][Derived]") {
	AstroCalculator calc;
	auto natal = [&](int y, int m, int d, int hour, double latitude, double longitude) {
		ChartData chart;
		chart.AddPlanets(std::vector<Planet>{ Planet::Sun, Planet::Moon, Planet::Mercury, Planet::Venus, Planet::Mars, Planet::PartOfFortune });
		chart.Info().Time = DateTime(y, m, d, hour, 0, 0);
		chart.Info().Latitude = latitude;
		chart.Info().Longitude = longitude;
		chart.SetHouseSystem(HouseSystem::Placidus);
		calc.Calculate(chart);
		return chart;
	};
	auto fortuneOf = [](ChartData const& chart) {
		return std::ranges::find(chart.AllPlanets(), Planet::PartOfFortune, &PlanetPosition::Planet)->Longitude.Value;
	};
	auto a = natal(1980, 5, 17, 9, 40.7, -74);
	auto b = natal(1985, 11, 2, 21, 51.5, 0);

	SECTION("progressions") {
		for (auto method : { ProgressionMethod::Secondary, ProgressionMethod::SolarArc }) {
			ProgressionOptions options;
			options.Method = method;
			auto progressed = DerivedCharts::Progress(calc, a, DateTime(2010, 5, 17, 9, 0, 0), options);
			auto expected = ArabicParts::Calculate(progressed, ArabicParts::Standard()[0]);
			REQUIRE(expected.has_value());
			CHECK(Diff(fortuneOf(progressed), expected->Longitude.Value) < 1e-9);
			CHECK(Diff(fortuneOf(progressed), fortuneOf(a)) > 1);		// (it has moved)
		}
	}
	SECTION("composite and Davison charts") {
		for (auto chart : { DerivedCharts::Composite(calc, a, b), DerivedCharts::Davison(calc, a, b) }) {
			auto expected = ArabicParts::Calculate(chart, ArabicParts::Standard()[0]);
			REQUIRE(expected.has_value());
			CHECK(Diff(fortuneOf(chart), expected->Longitude.Value) < 1e-9);
		}
	}
}

TEST_CASE("The Part of Fortune makes no aspects and no midpoints unless asked", "[ArabicParts][Fortune]") {
	CHECK_FALSE(AspectSettings().IsEnabled(Planet::PartOfFortune));
	CHECK(AspectSettings().IsEnabled(Planet::Sun));
	// with the planets, and switched on, it is one like the others
	std::vector<PlanetPosition> planets{ At(Planet::Sun, 100), At(Planet::PartOfFortune, 190) };
	CHECK(AspectCalculator().Calculate(planets).empty());
	AspectSettings settings;
	settings.PlanetEnabled[static_cast<int>(Planet::PartOfFortune)] = true;
	auto aspects = AspectCalculator(settings).Calculate(planets);
	REQUIRE(aspects.size() == 1);
	CHECK(aspects[0].Type == AspectType::Square);

	// (midpoints are of the bodies)
	ChartData chart;
	chart.AddPlanets({ At(Planet::Sun, 100), At(Planet::Moon, 20), At(Planet::PartOfFortune, 190) });
	CHECK(Midpoints::Points(chart).size() == 2);
	CHECK(Midpoints::Calculate(chart).size() == 1);
}
