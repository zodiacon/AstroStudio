#include "TestCommon.h"
#include "Synastry.h"
#include <algorithm>
#include <cmath>

namespace {
	PlanetPosition At(Planet planet, double longitude) {
		return PlanetPosition{ .Longitude = AstroPoint(longitude), .Speed = 0, .Latitude = 0, .LatitudeSpeed = 0, .Planet = planet };
	}

	// equal houses from the Ascendant (cusp n at asc + 30 (n - 1)), the Midheaven 90 degrees before it
	void EqualHouses(ChartData& chart, double asc) {
		auto& houses = chart.Houses();
		houses.Asc = asc;
		houses.MC = asc - 90 < 0 ? asc + 270 : asc - 90;
		for (int i = 0; i < 12; i++)
			houses.Cusps[i] = std::fmod(asc + 30 * i, 360.0);
	}

	// the major aspects only, 3 degrees for every one of them, no extras
	AspectSettings Settings() {
		AspectSettings settings;
		settings.MajorOnly = true;
		settings.MajorAspectOrb = 3;
		return settings;
	}
}

TEST_CASE("The aspects between two charts, tightest first", "[Synastry]") {
	ChartData a, b;
	a.AddPlanets({ At(Planet::Sun, 10), At(Planet::Moon, 135), At(Planet::Venus, 200) });
	b.AddPlanets({ At(Planet::Sun, 191), At(Planet::Moon, 10.5), At(Planet::Mars, 292.5) });
	auto report = Synastry::Make(a, b, Settings());

	// a's Sun conjunct b's Moon (0.5), opposite b's Sun (1), and a's Venus square b's Mars (2.5); the rest are wider than 3 degrees
	// (a's Moon sextile b's Sun by 4, trine b's Moon by 4.5)
	REQUIRE(report.Aspects.size() == 3);
	CHECK(std::ranges::is_sorted(report.Aspects, {}, &AspectData::Orb));
	auto is = [](AspectData const& x, Planet p1, Planet p2, AspectType type) {
		return x.Planet1.Planet == p1 && x.Planet2.Planet == p2 && x.Type == type;
	};
	CHECK(is(report.Aspects[0], Planet::Sun, Planet::Moon, AspectType::Conjunction));
	CHECK(is(report.Aspects[1], Planet::Sun, Planet::Sun, AspectType::Opposition));
	CHECK(is(report.Aspects[2], Planet::Venus, Planet::Mars, AspectType::Square));
	CHECK(report.Aspects[0].Orb == Approx(0.5));
	CHECK(report.Aspects[2].Orb == Approx(2.5));

	// the other way round, the same aspects with the charts' planets changing places
	auto back = Synastry::Make(b, a, Settings());
	REQUIRE(back.Aspects.size() == report.Aspects.size());
	CHECK(is(back.Aspects[0], Planet::Moon, Planet::Sun, AspectType::Conjunction));
	CHECK(is(back.Aspects[2], Planet::Mars, Planet::Venus, AspectType::Square));

	// no houses: no angles and no house overlay
	CHECK(report.AToB.empty());
	CHECK(report.BToA.empty());
	CHECK(report.AInB == std::vector<int>(3, 0));
	CHECK(report.BInA == std::vector<int>(3, 0));
}

TEST_CASE("Each chart's planets in the other's houses and on its angles", "[Synastry]") {
	ChartData a, b;
	a.AddPlanets({ At(Planet::Sun, 10), At(Planet::Moon, 100), At(Planet::Venus, 200) });
	EqualHouses(a, 90);		// cusps 90, 120, ...; the Midheaven at 0
	b.AddPlanets({ At(Planet::Sun, 191), At(Planet::Mars, 300) });
	EqualHouses(b, 0);		// cusps 0, 30, ...; the Midheaven at 270
	REQUIRE(Synastry::HasHouses(a.Houses()));
	CHECK_FALSE(Synastry::HasHouses(ChartData().Houses()));

	auto report = Synastry::Make(a, b, Settings());
	// a's planets in b's houses: 10 is in the first, 100 in the fourth, 200 in the seventh
	CHECK(report.AInB == std::vector<int>{ 1, 4, 7 });
	// b's in a's: 191 is 101 past a's Ascendant - house 4; 300 is 210 past it - house 8
	CHECK(report.BInA == std::vector<int>{ 4, 8 });

	// a's planets to b's angles (Asc 0, MC 270): the Sun 10 from the Ascendant - no; the Moon 100 - no; Venus 200 from the
	// Ascendant (160) - no, but 70 from the Midheaven - no. Nothing within 3 degrees.
	CHECK(report.AToB.empty());
	// b's to a's (Asc 90, MC 0): the Sun at 191 is 101 from the Ascendant - no, 169 from the Midheaven - no; Mars at 300 is 150
	// from the Ascendant - no, 60 from the Midheaven - a sextile, exact
	REQUIRE(report.BToA.size() == 1);
	CHECK(report.BToA[0].Planet == 1);
	CHECK(report.BToA[0].Midheaven);
	CHECK(report.BToA[0].Type == AspectType::Sextile);
	CHECK(report.BToA[0].Orb == Approx(0).margin(1e-4));
	CHECK(report.BToA[0].MaxOrb == Approx(3));

	// a planet left out of the aspects isn't on an angle either
	auto settings = Settings();
	settings.PlanetEnabled[static_cast<int>(Planet::Mars)] = false;
	CHECK(Synastry::Make(a, b, settings).BToA.empty());
	// a planet's extra orb counts on an angle
	settings = Settings();
	settings.PlanetOrbAdd[static_cast<int>(Planet::Moon)] = 8;
	a.AllPlanets()[1].Longitude = AstroPoint(5);		// the Moon 5 past b's Ascendant (0), and 95 from its Midheaven (270)
	auto wide = Synastry::Make(a, b, settings);
	REQUIRE(wide.AToB.size() == 2);
	for (auto const& contact : wide.AToB) {
		CHECK(contact.Planet == 1);
		CHECK(contact.Orb == Approx(5));
		CHECK(contact.MaxOrb == Approx(11));
		CHECK(contact.Type == (contact.Midheaven ? AspectType::Square : AspectType::Conjunction));
	}
}
