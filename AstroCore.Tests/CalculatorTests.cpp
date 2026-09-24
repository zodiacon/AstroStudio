#include "TestCommon.h"
#include "AstroCalculator.h"
#include "ChartData.h"

// AstroCalculator uses the built-in Moshier ephemeris, which needs no data files.
// The expected values below are independent of the code under test: equinox times and J2000.

namespace {
	// difference between two longitudes, the short way
	double Diff(double a, double b) {
		return AstroPoint::Diff(AstroPoint(a), AstroPoint(b));
	}
}

TEST_CASE("The Sun at J2000", "[Calculator]") {
	AstroCalculator calc;
	auto sun = calc.CalcPlanet(Planet::Sun, DateTime(2000, 1, 1, 12, 0, 0));
	CHECK(sun.Planet == Planet::Sun);
	CHECK(sun.Longitude.Value == Approx(280.37).margin(0.02));
	CHECK(sun.Latitude == Approx(0).margin(0.001));
	CHECK(sun.Longitude.Sign() == ZodiacSign::Capricorn);
	// about a degree a day in January
	CHECK(sun.Speed == Approx(1.019).margin(0.01));
}

TEST_CASE("The Sun at the equinoxes and solstices", "[Calculator]") {
	AstroCalculator calc;
	// 2024: March equinox 03:06 UT, June solstice 20:51 UT, September equinox 12:44 UT, December solstice 09:21 UT
	CHECK(Diff(calc.CalcPlanet(Planet::Sun, DateTime(2024, 3, 20, 3, 6, 0)).Longitude, 0) < 0.01);
	CHECK(Diff(calc.CalcPlanet(Planet::Sun, DateTime(2024, 6, 20, 20, 51, 0)).Longitude, 90) < 0.01);
	CHECK(Diff(calc.CalcPlanet(Planet::Sun, DateTime(2024, 9, 22, 12, 44, 0)).Longitude, 180) < 0.01);
	CHECK(Diff(calc.CalcPlanet(Planet::Sun, DateTime(2024, 12, 21, 9, 21, 0)).Longitude, 270) < 0.01);
}

TEST_CASE("Positions are on a circle and in the right sign", "[Calculator]") {
	AstroCalculator calc;
	auto sun = calc.CalcPlanet(Planet::Sun, DateTime(2024, 4, 10, 0, 0, 0));
	CHECK(sun.Longitude.Sign() == ZodiacSign::Aries);
	sun = calc.CalcPlanet(Planet::Sun, DateTime(2024, 8, 10, 0, 0, 0));
	CHECK(sun.Longitude.Sign() == ZodiacSign::Leo);

	for (int day = 0; day < 400; day += 13)
		for (auto planet : { Planet::Sun, Planet::Moon, Planet::Mercury, Planet::Venus, Planet::Mars, Planet::Jupiter,
			Planet::Saturn, Planet::Uranus, Planet::Neptune, Planet::Pluto, Planet::TrueNode, Planet::Chiron }) {
			auto p = calc.CalcPlanet(planet, DateTime(2024, 1, 1).AddDays(day));
			INFO("planet " << (int)planet << " day " << day);
			CHECK(p.Longitude.Value >= 0);
			CHECK(p.Longitude.Value < 360);
		}
}

TEST_CASE("Planet speeds", "[Calculator]") {
	AstroCalculator calc;
	auto when = DateTime(2024, 6, 1, 0, 0, 0);
	CHECK(std::abs(calc.CalcPlanet(Planet::Sun, when).Speed) < 1.1);
	CHECK(calc.CalcPlanet(Planet::Sun, when).Speed > 0.9);
	CHECK(calc.CalcPlanet(Planet::Moon, when).Speed > 11);
	CHECK(calc.CalcPlanet(Planet::Moon, when).Speed < 15.5);
	CHECK(std::abs(calc.CalcPlanet(Planet::Pluto, when).Speed) < 0.1);
}

TEST_CASE("Mercury turns retrograde in April 2024", "[Calculator]") {
	AstroCalculator calc;
	// Mercury was retrograde from 1 April to 25 April 2024
	auto before = calc.CalcPlanet(Planet::Mercury, DateTime(2024, 3, 20, 0, 0, 0));
	CHECK(before.Speed > 0);
	CHECK((before.Longitude.Flags & AstroPointFlags::Retro) == AstroPointFlags::None);

	auto during = calc.CalcPlanet(Planet::Mercury, DateTime(2024, 4, 12, 0, 0, 0));
	CHECK(during.Speed < 0);
	CHECK((during.Longitude.Flags & AstroPointFlags::Retro) == AstroPointFlags::Retro);

	auto after = calc.CalcPlanet(Planet::Mercury, DateTime(2024, 5, 10, 0, 0, 0));
	CHECK(after.Speed > 0);
	CHECK((after.Longitude.Flags & AstroPointFlags::Retro) == AstroPointFlags::None);
}

TEST_CASE("Harmonics multiply the longitude", "[Calculator]") {
	AstroCalculator calc;
	auto when = DateTime(2024, 7, 4, 15, 30, 0);
	auto base = calc.CalcPlanet(Planet::Mars, when).Longitude.Value;
	for (int h : { 2, 3, 5, 9, 12 }) {
		auto p = calc.CalcPlanet(Planet::Mars, when, h).Longitude.Value;
		INFO("harmonic " << h);
		CHECK(Diff(p, base * h) < 1e-6);
		CHECK(p >= 0);
		CHECK(p < 360);
	}
}

TEST_CASE("Speed can be left out", "[Calculator]") {
	AstroCalculator calc;
	auto when = DateTime(2024, 7, 4);
	auto with = calc.CalcPlanet(Planet::Venus, when, 1, true);
	auto without = calc.CalcPlanet(Planet::Venus, when, 1, false);
	CHECK(without.Longitude.Value == Approx(with.Longitude.Value));
}

TEST_CASE("Houses", "[Calculator]") {
	AstroCalculator calc;
	// New York
	const double lat = 40.7128, lon = -74.0060;
	auto when = DateTime(2000, 1, 1, 12, 0, 0);

	SECTION("the angles") {
		auto houses = calc.CalcHouses(when, lat, lon, HouseSystem::Placidus);
		// the first cusp is the ascendant, the tenth the midheaven, the seventh is opposite the first
		CHECK(Diff(houses.Cusps[0], houses.Asc) < 1e-6);
		CHECK(Diff(houses.Cusps[9], houses.MC) < 1e-6);
		CHECK(Diff(houses.Cusps[6], houses.Asc.Opposite()) < 1e-6);
		CHECK(Diff(houses.Cusps[3], houses.MC.Opposite()) < 1e-6);
	}
	SECTION("cusps go round the zodiac") {
		for (auto system : { HouseSystem::Placidus, HouseSystem::Koch, HouseSystem::Regiomontanus, HouseSystem::Campanus, HouseSystem::Porphyrius }) {
			auto houses = calc.CalcHouses(when, lat, lon, system);
			for (int i = 0; i < 12; i++) {
				double from = houses.Cusps[i];
				double to = houses.Cusps[(i + 1) % 12];
				double span = to - from;
				if (span < 0)
					span += 360;
				INFO("system " << (char)system << " house " << i + 1);
				CHECK(span > 0);
				CHECK(span < 90);
			}
		}
	}
	SECTION("equal houses are 30 degrees wide") {
		auto houses = calc.CalcHouses(when, lat, lon, HouseSystem::Equal);
		CHECK(Diff(houses.Cusps[0], houses.Asc) < 1e-6);
		for (int i = 0; i < 12; i++)
			CHECK(Diff(houses.Cusps[i], AstroPoint(houses.Asc.Value + 30 * i)) < 1e-6);
	}
	SECTION("whole sign houses start at the beginning of a sign") {
		auto houses = calc.CalcHouses(when, lat, lon, HouseSystem::EqualWholeSign);
		for (int i = 0; i < 12; i++)
			CHECK(houses.Cusps[i].DegreeInSign() == Approx(0).margin(1e-6));
		CHECK(houses.Cusps[0].Sign() == houses.Asc.Sign());
	}
	SECTION("the ascendant moves with the time") {
		auto later = calc.CalcHouses(when.AddDays(1.0 / 24), lat, lon, HouseSystem::Placidus);
		auto now = calc.CalcHouses(when, lat, lon, HouseSystem::Placidus);
		// about 15 degrees of the ecliptic in an hour, not more than 30
		CHECK(Diff(now.Asc, later.Asc) > 5);
		CHECK(Diff(now.Asc, later.Asc) < 30);
		// the midheaven goes forward with the sky
		CHECK(Diff(now.MC, later.MC) > 10);
		CHECK(Diff(now.MC, later.MC) < 20);
	}
	SECTION("the same time on the other side of the earth") {
		// twelve hours later and half a turn round: the ascendant is the same, roughly
		auto a = calc.CalcHouses(when, 0, 0, HouseSystem::Equal);
		auto b = calc.CalcHouses(when.AddDays(0.5), 0, 180, HouseSystem::Equal);
		CHECK(Diff(a.Asc, b.Asc) < 30);
	}
}

TEST_CASE("Sun ingress", "[Calculator]") {
	AstroCalculator calc;
	// 2024: the Sun entered Aries at 03:06 UT on 20 March and Libra at 12:44 UT on 22 September
	auto aries = calc.CalcPlanetIngress(Planet::Sun, DateTime(2024, 3, 15, 0, 0, 0));
	CHECK(aries.Planet == Planet::Sun);
	CHECK(aries.Sign == ZodiacSign::Aries);
	CHECK(aries.Time.Julian() == Approx(DateTime(2024, 3, 20, 3, 6, 0).Julian()).margin(0.005));
	CHECK_FALSE(aries.IsRetro);

	auto libra = calc.CalcPlanetIngress(Planet::Sun, DateTime(2024, 9, 17, 0, 0, 0));
	CHECK(libra.Sign == ZodiacSign::Libra);
	CHECK(libra.Time.Julian() == Approx(DateTime(2024, 9, 22, 12, 44, 0).Julian()).margin(0.005));

	// at the moment of the ingress the Sun is at the start of the sign
	auto at = calc.CalcPlanet(Planet::Sun, aries.Time);
	CHECK(Diff(at.Longitude, 0) < 0.001);
}

TEST_CASE("Moon ingress is within a couple of days", "[Calculator]") {
	AstroCalculator calc;
	auto start = DateTime(2024, 5, 1, 0, 0, 0);
	auto ingress = calc.CalcPlanetIngress(Planet::Moon, start);
	CHECK(ingress.Time.Julian() > start.Julian());
	CHECK(ingress.Time.Julian() - start.Julian() < 3);
	auto sign = calc.CalcPlanet(Planet::Moon, start).Longitude.Sign();
	CHECK(ingress.Sign == AstroPoint((int)sign * 30.0).NextSign().Sign());
}

TEST_CASE("Mercury station", "[Calculator]") {
	AstroCalculator calc;
	// Mercury turned retrograde on 1 April 2024 (and direct on 25 April)
	auto station = calc.CalcPlanetStation(Planet::Mercury, DateTime(2024, 3, 25, 0, 0, 0));
	CHECK(station.Planet == Planet::Mercury);
	CHECK(station.Time.Julian() == Approx(DateTime(2024, 4, 1, 12, 0, 0).Julian()).margin(1.0));
	CHECK(std::abs(calc.CalcPlanet(Planet::Mercury, station.Time).Speed) < 0.01);
	// it stands in Aries
	CHECK(station.Position.Sign() == ZodiacSign::Aries);

	auto direct = calc.CalcPlanetStation(Planet::Mercury, DateTime(2024, 4, 18, 0, 0, 0));
	CHECK(direct.Time.Julian() == Approx(DateTime(2024, 4, 25, 12, 0, 0).Julian()).margin(1.0));
	CHECK(direct.Position.Sign() == ZodiacSign::Aries);
}

TEST_CASE("The calculator fills in a chart", "[Calculator]") {
	AstroCalculator calc;
	ChartData chart;
	chart.AddPlanets({ Planet::Sun, Planet::Moon, Planet::Mars });
	chart.Info().Time = DateTime(2000, 1, 1, 12, 0, 0);
	chart.Info().Latitude = 40.7128;
	chart.Info().Longitude = -74.0060;
	chart.SetHouseSystem(HouseSystem::Placidus);

	CHECK(calc.Calculate(chart));

	REQUIRE(chart.PlanetCount() == 3);
	CHECK(chart.GetPlanet(0).Planet == Planet::Sun);
	CHECK(chart.GetPlanet(0).Longitude.Value == Approx(280.37).margin(0.02));
	CHECK(chart.GetPlanet(1).Planet == Planet::Moon);
	CHECK(chart.Houses().Cusps[0].Value == Approx(chart.Houses().Asc.Value));

	// harmonics apply to the planets
	chart.Harmonic(2);
	calc.Calculate(chart);
	CHECK(Diff(chart.GetPlanet(0).Longitude, 2 * 280.37) < 0.05);
}
