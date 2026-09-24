#include "TestCommon.h"
#include "ChartData.h"

TEST_CASE("A new chart is empty", "[ChartData]") {
	ChartData chart;
	CHECK(chart.PlanetCount() == 0);
	CHECK(chart.AllPlanets().empty());
	CHECK(chart.Harmonic() == 1);
	CHECK(chart.GetHouseSystem() == HouseSystem::Koch);
	CHECK(chart.Info().Type == InfoType::Unknown);
	CHECK(chart.Info().FirstName.empty());
}

TEST_CASE("Adding planets", "[ChartData]") {
	ChartData chart;

	SECTION("from a list of planets") {
		chart.AddPlanets({ Planet::Sun, Planet::Moon });
		REQUIRE(chart.PlanetCount() == 2);
		CHECK(chart.GetPlanet(0).Planet == Planet::Sun);
		CHECK(chart.GetPlanet(1).Planet == Planet::Moon);
	}
	SECTION("from a vector") {
		chart.AddPlanets(std::vector<Planet>{ Planet::Mars, Planet::Venus, Planet::Saturn });
		REQUIRE(chart.PlanetCount() == 3);
		CHECK(chart.GetPlanet(2).Planet == Planet::Saturn);
	}
	SECTION("with positions") {
		chart.AddPlanets({ PlanetPosition{ .Longitude = AstroPoint(100), .Speed = 1.0, .Planet = Planet::Sun } });
		REQUIRE(chart.PlanetCount() == 1);
		CHECK(chart.GetPlanet(0).Longitude.Value == Approx(100));
		CHECK(chart.GetPlanet(0).Speed == Approx(1.0));
	}
	SECTION("more than once adds to the end") {
		chart.AddPlanets({ Planet::Sun }).AddPlanets({ Planet::Moon });
		REQUIRE(chart.PlanetCount() == 2);
		CHECK(chart.GetPlanet(1).Planet == Planet::Moon);
	}
	SECTION("all the planets can be read and changed") {
		chart.AddPlanets({ Planet::Sun });
		chart.AllPlanets()[0].Longitude = 45;
		CHECK(std::as_const(chart).AllPlanets()[0].Longitude.Value == Approx(45));
	}
}

TEST_CASE("Removing planets", "[ChartData]") {
	ChartData chart;
	chart.AddPlanets({ Planet::Sun, Planet::Moon, Planet::Mars, Planet::Venus });

	chart.RemovePlanets({ Planet::Moon, Planet::Venus });
	REQUIRE(chart.PlanetCount() == 2);
	CHECK(chart.GetPlanet(0).Planet == Planet::Sun);
	CHECK(chart.GetPlanet(1).Planet == Planet::Mars);

	chart.Clear();
	CHECK(chart.PlanetCount() == 0);
}

TEST_CASE("Harmonic must be a sensible number", "[ChartData]") {
	ChartData chart;
	CHECK(chart.Harmonic(5) == 5);
	CHECK(chart.Harmonic() == 5);
	// out of range values are ignored
	CHECK(chart.Harmonic(0) == 5);
	CHECK(chart.Harmonic(-3) == 5);
	CHECK(chart.Harmonic(10000) == 5);
	CHECK(chart.Harmonic(9999) == 9999);
	CHECK(chart.Harmonic(1) == 1);
}

TEST_CASE("House system", "[ChartData]") {
	ChartData chart;
	chart.SetHouseSystem(HouseSystem::Placidus);
	CHECK(chart.GetHouseSystem() == HouseSystem::Placidus);
	CHECK((char)HouseSystem::Placidus == 'P');
	CHECK((char)HouseSystem::Koch == 'K');
	CHECK((char)HouseSystem::EqualWholeSign == 'W');
}

TEST_CASE("Chart info", "[ChartData]") {
	ChartData chart;
	auto& info = chart.Info();
	info.FirstName = L"Ada";
	info.LastName = L"Lovelace";
	info.Type = InfoType::Female;
	info.Latitude = 51.5;
	info.Longitude = -0.12;
	info.City = L"London";
	info.Time = DateTime(1815, 12, 10, 13, 0, 0);
	info.TimeZone.Name = L"GMT Standard Time";
	info.TimeZone.OffsetUT = 0;

	auto const& read = std::as_const(chart).Info();
	CHECK(read.FirstName == L"Ada");
	CHECK(read.LastName == L"Lovelace");
	CHECK(read.Type == InfoType::Female);
	CHECK(read.City == L"London");
	CHECK(read.Time.Year() == 1815);
	CHECK(read.TimeZone.Name == L"GMT Standard Time");
}

TEST_CASE("Chart info starts out zeroed", "[ChartData]") {
	ChartData chart;
	CHECK(chart.Info().Latitude == 0);
	CHECK(chart.Info().Longitude == 0);
	CHECK(chart.Info().Elevation == 0);
	CHECK(chart.Info().TimeZone.OffsetUT == 0);
}

TEST_CASE("Calculating a chart's planets and houses", "[ChartData]") {
	AstroCalculator calc;
	ChartData chart;
	chart.AddPlanets({ Planet::Sun, Planet::Moon });
	chart.Info().Time = DateTime(2000, 1, 1, 12, 0, 0);
	chart.Info().Latitude = 40.7128;
	chart.Info().Longitude = -74.0060;

	chart.CalcPlanets(calc);
	chart.CalcHouses(calc);

	CHECK(chart.GetPlanet(0).Longitude.Value == Approx(280.37).margin(0.02));
	CHECK(chart.GetPlanet(1).Planet == Planet::Moon);
	CHECK(chart.Houses().Asc.Value >= 0);
	CHECK(chart.Houses().Asc.Value < 360);
	CHECK(chart.Houses().Cusps[0].Value == Approx(chart.Houses().Asc.Value));

	// the planets follow the chart's time
	chart.Info().Time = DateTime(2000, 7, 1, 12, 0, 0);
	chart.CalcPlanets(calc);
	CHECK(chart.GetPlanet(0).Longitude.Sign() == ZodiacSign::Cancer);
}
