#include "TestCommon.h"
#include "DerivedCharts.h"
#include <cmath>
#include <algorithm>

namespace {
	// a chart with the planets up to Pluto, for a date and a place
	ChartData Natal(int y, int m, int d, int hour, int minute, double latitude, double longitude) {
		ChartData chart;
		chart.AddPlanets(std::vector<Planet>{ Planet::Sun, Planet::Moon, Planet::Mercury, Planet::Venus, Planet::Mars, Planet::Jupiter,
			Planet::Saturn, Planet::Uranus, Planet::Neptune, Planet::Pluto });
		chart.Info().Time = DateTime(y, m, d, hour, minute, 0);
		chart.Info().Latitude = latitude;
		chart.Info().Longitude = longitude;
		chart.SetHouseSystem(HouseSystem::Placidus);
		AstroCalculator calc;
		calc.Calculate(chart);
		return chart;
	}

	double Diff(double a, double b) {
		return AstroPoint::Diff(AstroPoint(a), AstroPoint(b));
	}

	PlanetPosition At(Planet planet, double longitude, double speed = 0) {
		return PlanetPosition{ .Longitude = AstroPoint(longitude), .Speed = speed, .Latitude = 0, .LatitudeSpeed = 0, .Planet = planet };
	}

	const double MicroDegree = 1e-4;
}

TEST_CASE("Years between two moments", "[Derived]") {
	auto natal = DateTime(2000, 1, 1, 12, 0, 0);
	CHECK(DerivedCharts::YearsBetween(natal, natal) == Approx(0).margin(1e-12));
	CHECK(DerivedCharts::YearsBetween(natal, DateTime(natal.Julian() + TropicalYear, true)) == Approx(1).margin(1e-9));
	CHECK(DerivedCharts::YearsBetween(natal, DateTime(2030, 1, 1, 12, 0, 0)) == Approx(30).margin(0.01));
	// going back is negative
	CHECK(DerivedCharts::YearsBetween(DateTime(2030, 1, 1), natal) < 0);
}

TEST_CASE("Secondary progressed time is a day for a year", "[Derived]") {
	auto natal = DateTime(2000, 1, 1, 12, 0, 0);
	CHECK(DerivedCharts::ProgressedTime(natal, natal).Julian() == Approx(natal.Julian()).margin(1e-9));
	// a year on is the next day
	auto progressed = DerivedCharts::ProgressedTime(natal, DateTime(natal.Julian() + TropicalYear, true));
	CHECK(progressed.Julian() - natal.Julian() == Approx(1).margin(1e-9));
	// thirty years on is about thirty days
	progressed = DerivedCharts::ProgressedTime(natal, DateTime(2030, 1, 1, 12, 0, 0));
	CHECK(progressed.Julian() - natal.Julian() == Approx(30).margin(0.01));
	// (before birth it goes back)
	CHECK(DerivedCharts::ProgressedTime(natal, DateTime(1990, 1, 1, 12, 0, 0)).Julian() < natal.Julian());
}

TEST_CASE("Secondary progressions at the birth are the birth", "[Derived]") {
	AstroCalculator calc;
	auto natal = Natal(1985, 6, 15, 14, 30, 40.7, -74.0);
	auto progressed = DerivedCharts::Progress(calc, natal, natal.Info().Time);
	REQUIRE(progressed.PlanetCount() == natal.PlanetCount());
	for (int i = 0; i < natal.PlanetCount(); i++)
		CHECK(Diff(progressed.GetPlanet(i).Longitude, natal.GetPlanet(i).Longitude) < MicroDegree);
	CHECK(Diff(progressed.Houses().Asc, natal.Houses().Asc) < MicroDegree);
	CHECK(Diff(progressed.Houses().MC, natal.Houses().MC) < MicroDegree);
}

TEST_CASE("Secondary progressions move the planets by their motion in as many days as years", "[Derived]") {
	AstroCalculator calc;
	auto natal = Natal(2000, 1, 1, 12, 0, 40.7, -74.0);
	auto target = DateTime(2010, 1, 1, 12, 0, 0);
	auto progressed = DerivedCharts::Progress(calc, natal, target);

	// the progressed Sun has gone about ten degrees, the progressed Moon about 130
	double sun = std::fmod(progressed.GetPlanet(0).Longitude.Value - natal.GetPlanet(0).Longitude.Value + 360, 360);
	CHECK(sun > 9.5);
	CHECK(sun < 10.5);
	double moon = std::fmod(progressed.GetPlanet(1).Longitude.Value - natal.GetPlanet(1).Longitude.Value + 360, 360);
	CHECK(moon > 100);
	CHECK(moon < 160);

	// the chart is cast for the progressed moment: its planets are the sky's then
	auto time = DerivedCharts::ProgressedTime(natal.Info().Time, target);
	CHECK(progressed.Info().Time.Julian() == Approx(time.Julian()).margin(1e-9));
	CHECK(Diff(progressed.GetPlanet(4).Longitude, calc.CalcPlanet(Planet::Mars, time).Longitude) < MicroDegree);
	// and it is still the birth's place and house system
	CHECK(progressed.Info().Latitude == Approx(40.7));
	CHECK(progressed.GetHouseSystem() == HouseSystem::Placidus);
}

TEST_CASE("The angles of secondary progressions", "[Derived]") {
	AstroCalculator calc;
	auto natal = Natal(2000, 1, 1, 12, 0, 40.7, -74.0);
	auto target = DateTime(2020, 1, 1, 12, 0, 0);
	auto time = DerivedCharts::ProgressedTime(natal.Info().Time, target);

	SECTION("calculated for the progressed moment") {
		auto progressed = DerivedCharts::Progress(calc, natal, target, { .Angles = ProgressedAngles::Calculated });
		auto expected = calc.CalcHouses(time, 40.7, -74.0, HouseSystem::Placidus);
		CHECK(Diff(progressed.Houses().Asc, expected.Asc) < MicroDegree);
		CHECK(Diff(progressed.Houses().MC, expected.MC) < MicroDegree);
		// a day later in the sky is about a degree more of sidereal time, so the angles have moved on
		CHECK(Diff(progressed.Houses().MC, natal.Houses().MC) > 0.5);
	}
	SECTION("the natal houses") {
		auto progressed = DerivedCharts::Progress(calc, natal, target, { .Angles = ProgressedAngles::Natal });
		CHECK(Diff(progressed.Houses().Asc, natal.Houses().Asc) < MicroDegree);
		CHECK(Diff(progressed.Houses().MC, natal.Houses().MC) < MicroDegree);
		for (int i = 0; i < 12; i++)
			CHECK(Diff(progressed.Houses().Cusps[i], natal.Houses().Cusps[i]) < MicroDegree);
	}
	SECTION("the Midheaven moved on by the solar arc") {
		auto progressed = DerivedCharts::Progress(calc, natal, target, { .Angles = ProgressedAngles::SolarArc });
		double arc = DerivedCharts::Arc(calc, natal, target, ArcKey::Actual);
		CHECK(Diff(progressed.Houses().MC, natal.Houses().MC.Value + arc) < MicroDegree);
		// the tenth cusp is the Midheaven and the houses are still in order round the zodiac
		CHECK(Diff(progressed.Houses().Cusps[9], progressed.Houses().MC) < MicroDegree);
		CHECK(Diff(progressed.Houses().Cusps[6], progressed.Houses().Asc.Opposite()) < MicroDegree);
	}
}

TEST_CASE("The solar arc", "[Derived]") {
	AstroCalculator calc;
	auto natal = Natal(2000, 1, 1, 12, 0, 40.7, -74.0);
	auto target = DateTime(2010, 1, 1, 12, 0, 0);
	double years = DerivedCharts::YearsBetween(natal.Info().Time, target);

	// the true arc is what the progressed Sun has travelled
	auto time = DerivedCharts::ProgressedTime(natal.Info().Time, target);
	double sun = calc.CalcPlanet(Planet::Sun, time).Longitude.Value - calc.CalcPlanet(Planet::Sun, natal.Info().Time).Longitude.Value;
	CHECK(DerivedCharts::Arc(calc, natal, target, ArcKey::Actual) == Approx(sun).margin(1e-6));
	// the mean arcs are a rate a year
	CHECK(DerivedCharts::Arc(calc, natal, target, ArcKey::Naibod) == Approx(years * 0.98564736).margin(1e-9));
	CHECK(DerivedCharts::Arc(calc, natal, target, ArcKey::Ptolemy) == Approx(years).margin(1e-9));
	// none at the birth, and backward before it
	CHECK(DerivedCharts::Arc(calc, natal, natal.Info().Time, ArcKey::Actual) == Approx(0).margin(1e-6));
	CHECK(DerivedCharts::Arc(calc, natal, DateTime(1990, 1, 1, 12, 0, 0), ArcKey::Actual) < 0);
	CHECK(DerivedCharts::Arc(calc, natal, DateTime(1990, 1, 1, 12, 0, 0), ArcKey::Naibod) < 0);
}

TEST_CASE("Solar arc directions move everything by the same arc", "[Derived]") {
	AstroCalculator calc;
	auto natal = Natal(1975, 3, 21, 8, 15, 51.5, -0.1);
	auto target = DateTime(2005, 3, 21, 8, 15, 0);

	for (auto key : { ArcKey::Actual, ArcKey::Naibod, ArcKey::Ptolemy }) {
		double arc = DerivedCharts::Arc(calc, natal, target, key);
		auto directed = DerivedCharts::Progress(calc, natal, target, { .Method = ProgressionMethod::SolarArc, .Key = key });
		INFO("key " << (int)key << " arc " << arc);
		REQUIRE(directed.PlanetCount() == natal.PlanetCount());
		for (int i = 0; i < natal.PlanetCount(); i++)
			CHECK(Diff(directed.GetPlanet(i).Longitude, natal.GetPlanet(i).Longitude.Value + arc) < MicroDegree);
		CHECK(Diff(directed.Houses().Asc, natal.Houses().Asc.Value + arc) < MicroDegree);
		CHECK(Diff(directed.Houses().MC, natal.Houses().MC.Value + arc) < MicroDegree);
		for (int i = 0; i < 12; i++)
			CHECK(Diff(directed.Houses().Cusps[i], natal.Houses().Cusps[i].Value + arc) < MicroDegree);
		// the birth moment stays what the sky is at
		CHECK(directed.Info().Time.Julian() == Approx(natal.Info().Time.Julian()));
		// and a longitude is still a longitude
		for (auto const& p : directed.AllPlanets()) {
			CHECK(p.Longitude.Value >= 0);
			CHECK(p.Longitude.Value < 360);
		}
	}
}

TEST_CASE("Solar arc directions at the birth are the birth", "[Derived]") {
	AstroCalculator calc;
	auto natal = Natal(1975, 3, 21, 8, 15, 51.5, -0.1);
	auto directed = DerivedCharts::Progress(calc, natal, natal.Info().Time, { .Method = ProgressionMethod::SolarArc });
	for (int i = 0; i < natal.PlanetCount(); i++)
		CHECK(Diff(directed.GetPlanet(i).Longitude, natal.GetPlanet(i).Longitude) < MicroDegree);
	CHECK(Diff(directed.Houses().MC, natal.Houses().MC) < MicroDegree);
}

TEST_CASE("Primary directions turn the sky and leave the planets", "[Derived]") {
	AstroCalculator calc;
	auto natal = Natal(1990, 8, 10, 22, 0, 34.05, -118.25);
	auto target = DateTime(2020, 8, 10, 22, 0, 0);
	double years = DerivedCharts::YearsBetween(natal.Info().Time, target);

	SECTION("the planets stay") {
		auto directed = DerivedCharts::Progress(calc, natal, target, { .Method = ProgressionMethod::Primary, .Key = ArcKey::Ptolemy });
		for (int i = 0; i < natal.PlanetCount(); i++)
			CHECK(directed.GetPlanet(i).Longitude.Value == Approx(natal.GetPlanet(i).Longitude.Value).margin(1e-12));
	}
	SECTION("the sidereal time advances by the arc") {
		auto ptolemy = DerivedCharts::Progress(calc, natal, target, { .Method = ProgressionMethod::Primary, .Key = ArcKey::Ptolemy });
		CHECK(Diff(ptolemy.Houses().Armc, natal.Houses().Armc.Value + years) < MicroDegree);
		auto naibod = DerivedCharts::Progress(calc, natal, target, { .Method = ProgressionMethod::Primary, .Key = ArcKey::Naibod });
		CHECK(Diff(naibod.Houses().Armc, natal.Houses().Armc.Value + years * NaibodArc) < MicroDegree);
		// Actual is not defined for these: the Naibod arc
		auto actual = DerivedCharts::Progress(calc, natal, target, { .Method = ProgressionMethod::Primary, .Key = ArcKey::Actual });
		CHECK(Diff(actual.Houses().Armc, naibod.Houses().Armc) < MicroDegree);
	}
	SECTION("so the angles move on by about the arc") {
		auto directed = DerivedCharts::Progress(calc, natal, target, { .Method = ProgressionMethod::Primary, .Key = ArcKey::Ptolemy });
		// thirty degrees of right ascension is some 25 to 35 degrees of the Midheaven's longitude
		double moved = std::fmod(directed.Houses().MC.Value - natal.Houses().MC.Value + 360, 360);
		CHECK(moved > 25);
		CHECK(moved < 35);
		CHECK(Diff(directed.Houses().Cusps[9], directed.Houses().MC) < MicroDegree);
	}
}

TEST_CASE("Primary directions at the birth are the birth", "[Derived]") {
	AstroCalculator calc;
	auto natal = Natal(1990, 8, 10, 22, 0, 34.05, -118.25);
	auto directed = DerivedCharts::Progress(calc, natal, natal.Info().Time, { .Method = ProgressionMethod::Primary, .Key = ArcKey::Naibod });
	// the houses worked out from the sidereal time are the houses of the moment
	CHECK(Diff(directed.Houses().Asc, natal.Houses().Asc) < MicroDegree);
	CHECK(Diff(directed.Houses().MC, natal.Houses().MC) < MicroDegree);
	for (int i = 0; i < 12; i++)
		CHECK(Diff(directed.Houses().Cusps[i], natal.Houses().Cusps[i]) < 1e-3);
}

TEST_CASE("Returns of the Sun", "[Derived]") {
	AstroCalculator calc;
	auto natal = Natal(2000, 1, 1, 12, 0, 40.7, -74.0);
	double sun = natal.GetPlanet(0).Longitude.Value;
	const double year = TropicalYear;

	auto next = DerivedCharts::FindReturn(calc, natal, Planet::Sun, DateTime(natal.Info().Time.Julian() + 10, true), ReturnSearch::Next);
	REQUIRE(next);
	// about a year after the birth, with the Sun exactly where it was
	CHECK(next->Julian() - natal.Info().Time.Julian() == Approx(year).margin(1.0));
	CHECK(Diff(calc.CalcPlanet(Planet::Sun, *next).Longitude, sun) < MicroDegree);

	auto previous = DerivedCharts::FindReturn(calc, natal, Planet::Sun, DateTime(natal.Info().Time.Julian() + 400, true), ReturnSearch::Previous);
	REQUIRE(previous);
	CHECK(previous->Julian() == Approx(next->Julian()).margin(1e-5));

	// nearest: the one before is 20 days away, the one after 345
	auto closest = DerivedCharts::FindReturn(calc, natal, Planet::Sun, DateTime(natal.Info().Time.Julian() + 385, true), ReturnSearch::Nearest);
	REQUIRE(closest);
	CHECK(closest->Julian() == Approx(next->Julian()).margin(1e-5));
	// and now the one after is nearer
	closest = DerivedCharts::FindReturn(calc, natal, Planet::Sun, DateTime(natal.Info().Time.Julian() + 300, true), ReturnSearch::Nearest);
	REQUIRE(closest);
	CHECK(closest->Julian() == Approx(next->Julian()).margin(1e-5));
}

TEST_CASE("Returns of the Moon", "[Derived]") {
	AstroCalculator calc;
	auto natal = Natal(2000, 1, 1, 12, 0, 40.7, -74.0);
	double moon = natal.GetPlanet(1).Longitude.Value;

	auto next = DerivedCharts::FindReturn(calc, natal, Planet::Moon, DateTime(natal.Info().Time.Julian() + 1, true), ReturnSearch::Next);
	REQUIRE(next);
	// a month later (27.3 days, give or take a little for the Sun's changing pull)
	CHECK(next->Julian() - natal.Info().Time.Julian() == Approx(27.32).margin(0.5));
	CHECK(Diff(calc.CalcPlanet(Planet::Moon, *next).Longitude, moon) < MicroDegree);

	// the ones after that are about as far apart again
	auto after = DerivedCharts::FindReturn(calc, natal, Planet::Moon, DateTime(next->Julian() + 1, true), ReturnSearch::Next);
	REQUIRE(after);
	CHECK(after->Julian() - next->Julian() == Approx(27.32).margin(0.5));
}

TEST_CASE("A planet that turns retrograde passes a longitude more than once", "[Derived]") {
	AstroCalculator calc;
	// Mercury, in the spring of 2024, was at 26 degrees of Aries three times: going forward, back, and forward again
	double target = 26;
	std::vector<double> times;
	double from = DateTime(2024, 3, 1).Julian();
	for (int i = 0; i < 6; i++) {
		auto found = DerivedCharts::FindLongitude(calc, Planet::Mercury, target, DateTime(from, true), ReturnSearch::Next);
		REQUIRE(found);
		CHECK(Diff(calc.CalcPlanet(Planet::Mercury, *found).Longitude, target) < MicroDegree);
		CHECK(found->Julian() > from);
		times.push_back(found->Julian());
		from = found->Julian() + 1e-3;
		if (found->Julian() > DateTime(2024, 6, 30).Julian())
			break;
	}
	// found in order, and more than once in that season
	CHECK(std::is_sorted(times.begin(), times.end()));
	CHECK(times.size() >= 3);
}

TEST_CASE("Finding a longitude stays clear of the start", "[Derived]") {
	AstroCalculator calc;
	// standing exactly on the longitude isn't finding it: the next time is later
	auto start = DateTime(2000, 1, 1, 12, 0, 0);
	double sun = calc.CalcPlanet(Planet::Sun, start).Longitude.Value;
	auto next = DerivedCharts::FindLongitude(calc, Planet::Sun, sun, start, ReturnSearch::Next);
	REQUIRE(next);
	CHECK(next->Julian() - start.Julian() > 300);
	auto previous = DerivedCharts::FindLongitude(calc, Planet::Sun, sun, start, ReturnSearch::Previous);
	REQUIRE(previous);
	CHECK(start.Julian() - previous->Julian() > 300);
}

TEST_CASE("The composite of two charts", "[Derived]") {
	AstroCalculator calc;
	auto a = Natal(1980, 5, 10, 8, 0, 40.7, -74.0);
	auto b = Natal(1984, 11, 2, 20, 30, 51.5, -0.1);

	auto composite = DerivedCharts::Composite(calc, a, b);
	REQUIRE(composite.PlanetCount() == a.PlanetCount());

	for (int i = 0; i < composite.PlanetCount(); i++) {
		double la = a.GetPlanet(i).Longitude.Value, lb = b.GetPlanet(i).Longitude.Value;
		// the midpoint on the shorter way round
		double delta = std::fmod(lb - la, 360);
		if (delta > 180)
			delta -= 360;
		else if (delta < -180)
			delta += 360;
		INFO("planet " << i);
		CHECK(Diff(composite.GetPlanet(i).Longitude, la + delta / 2) < MicroDegree);
		CHECK(composite.GetPlanet(i).Speed == Approx((a.GetPlanet(i).Speed + b.GetPlanet(i).Speed) / 2).margin(1e-12));
		bool retro = (composite.GetPlanet(i).Longitude.Flags & AstroPointFlags::Retro) == AstroPointFlags::Retro;
		CHECK(retro == (composite.GetPlanet(i).Speed < 0));
	}

	// the midpoint of the two places and times
	CHECK(composite.Info().Latitude == Approx((40.7 + 51.5) / 2));
	CHECK(composite.Info().Longitude == Approx((-74.0 + -0.1) / 2));
	CHECK(composite.Info().Time.Julian() == Approx((a.Info().Time.Julian() + b.Info().Time.Julian()) / 2).margin(1e-6));
	CHECK(composite.Info().Type == InfoType::Event);
	CHECK(composite.GetHouseSystem() == a.GetHouseSystem());
	// the Midheaven is the midpoint of the two
	CHECK(Diff(composite.Houses().MC, AstroPoint::MidPoint(a.Houses().MC, b.Houses().MC)) < MicroDegree);
	CHECK(Diff(composite.Houses().Cusps[9], composite.Houses().MC) < MicroDegree);
}

TEST_CASE("The composite does not depend on the order", "[Derived]") {
	AstroCalculator calc;
	auto a = Natal(1980, 5, 10, 8, 0, 40.7, -74.0);
	auto b = Natal(1984, 11, 2, 20, 30, 40.7, -74.0);		// the same latitude: the houses are then the same in either order

	auto ab = DerivedCharts::Composite(calc, a, b);
	auto ba = DerivedCharts::Composite(calc, b, a);
	for (int i = 0; i < ab.PlanetCount(); i++)
		CHECK(Diff(ab.GetPlanet(i).Longitude, ba.GetPlanet(i).Longitude) < MicroDegree);
	CHECK(Diff(ab.Houses().MC, ba.Houses().MC) < MicroDegree);
	CHECK(Diff(ab.Houses().Asc, ba.Houses().Asc) < MicroDegree);
}

TEST_CASE("The composite of a chart with itself is that chart", "[Derived]") {
	AstroCalculator calc;
	auto a = Natal(1980, 5, 10, 8, 0, 40.7, -74.0);

	for (auto method : { CompositeHouses::MidpointMC, CompositeHouses::MidpointARMC }) {
		auto composite = DerivedCharts::Composite(calc, a, a, method);
		for (int i = 0; i < a.PlanetCount(); i++)
			CHECK(Diff(composite.GetPlanet(i).Longitude, a.GetPlanet(i).Longitude) < MicroDegree);
		CHECK(Diff(composite.Houses().MC, a.Houses().MC) < 1e-3);
		CHECK(Diff(composite.Houses().Asc, a.Houses().Asc) < 1e-3);
	}
}

TEST_CASE("The composite has the planets both charts have", "[Derived]") {
	AstroCalculator calc;
	auto a = Natal(1980, 5, 10, 8, 0, 40.7, -74.0);
	ChartData b;
	b.AddPlanets(std::vector<Planet>{ Planet::Moon, Planet::Sun, Planet::Chiron });
	b.Info().Time = DateTime(1990, 1, 1, 12, 0, 0);
	b.Info().Latitude = 40.7;
	b.Info().Longitude = -74.0;
	calc.Calculate(b);

	auto composite = DerivedCharts::Composite(calc, a, b);
	// in a's order: the Sun and the Moon (Chiron is not in a)
	REQUIRE(composite.PlanetCount() == 2);
	CHECK(composite.GetPlanet(0).Planet == Planet::Sun);
	CHECK(composite.GetPlanet(1).Planet == Planet::Moon);
}

TEST_CASE("Aspects between two sets of planets", "[Derived]") {
	AspectCalculator aspects;
	std::vector<PlanetPosition> a{ At(Planet::Sun, 0), At(Planet::Mars, 100) };
	std::vector<PlanetPosition> b{ At(Planet::Moon, 120), At(Planet::Venus, 300) };

	auto found = aspects.CalcBetween(a, b);
	// Sun trine Moon, Sun sextile Venus; Mars is in aspect to neither
	REQUIRE(found.size() == 2);
	CHECK(found[0].Planet1.Planet == Planet::Sun);
	CHECK(found[0].Planet2.Planet == Planet::Moon);
	CHECK(found[0].Type == AspectType::Trine);
	CHECK(found[1].Planet1.Planet == Planet::Sun);
	CHECK(found[1].Planet2.Planet == Planet::Venus);
	CHECK(found[1].Type == AspectType::Sextile);

	// the same planet in both sets is a pair like any other
	auto same = aspects.CalcBetween({ At(Planet::Sun, 10) }, { At(Planet::Sun, 12) });
	REQUIRE(same.size() == 1);
	CHECK(same[0].Type == AspectType::Conjunction);

	CHECK(aspects.CalcBetween({}, b).empty());
	CHECK(aspects.CalcBetween(a, {}).empty());
}

TEST_CASE("Aspects between sets respect the aspect settings", "[Derived]") {
	AspectSettings settings;
	settings.PlanetEnabled[static_cast<int>(Planet::Moon)] = false;
	AspectCalculator aspects(settings);
	std::vector<PlanetPosition> a{ At(Planet::Sun, 0) };
	std::vector<PlanetPosition> b{ At(Planet::Moon, 120), At(Planet::Venus, 300) };
	auto found = aspects.CalcBetween(a, b);
	REQUIRE(found.size() == 1);
	CHECK(found[0].Planet2.Planet == Planet::Venus);
}

TEST_CASE("The house a longitude falls in", "[Derived]") {
	HouseData houses;
	for (int i = 0; i < 12; i++)
		houses.Cusps[i] = AstroPoint(30.0 * i + 10);		// 10, 40, ... 340: the twelfth house runs across 0 Aries

	CHECK(DerivedCharts::HouseOf(houses, AstroPoint(15)) == 1);
	CHECK(DerivedCharts::HouseOf(houses, AstroPoint(45)) == 2);
	CHECK(DerivedCharts::HouseOf(houses, AstroPoint(339)) == 11);
	CHECK(DerivedCharts::HouseOf(houses, AstroPoint(350)) == 12);
	CHECK(DerivedCharts::HouseOf(houses, AstroPoint(5)) == 12);

	auto overlay = DerivedCharts::HouseOverlay(houses, { At(Planet::Sun, 15), At(Planet::Moon, 5), At(Planet::Mars, 100) });
	REQUIRE(overlay.size() == 3);
	CHECK(overlay[0] == 1);
	CHECK(overlay[1] == 12);
	CHECK(overlay[2] == 4);
}

TEST_CASE("Houses from the Midheaven", "[Derived]") {
	AstroCalculator calc;
	auto natal = Natal(2000, 1, 1, 12, 0, 40.7, -74.0);
	// the natal Midheaven gives back the natal houses
	auto houses = DerivedCharts::HousesFromMC(calc, natal.Houses().MC.Value, 40.7, natal.Info().Time, HouseSystem::Placidus);
	CHECK(Diff(houses.MC, natal.Houses().MC) < MicroDegree);
	CHECK(Diff(houses.Asc, natal.Houses().Asc) < MicroDegree);
	CHECK(Diff(houses.Armc, natal.Houses().Armc) < 1e-3);
}

TEST_CASE("Davison chart is a real chart for the midpoint in time and place", "[Derived]") {
	AstroCalculator calc;
	auto a = Natal(1980, 3, 10, 6, 0, 40, -74);
	auto b = Natal(1990, 3, 10, 6, 0, 50, -70);
	auto davison = DerivedCharts::Davison(calc, a, b);

	auto const& info = davison.Info();
	CHECK(info.Time.Julian() == Approx((a.Info().Time.Julian() + b.Info().Time.Julian()) / 2).margin(1e-9));
	CHECK(info.Latitude == Approx(45));
	CHECK(info.Longitude == Approx(-72));
	CHECK(info.TimeZone.Name.empty());
	REQUIRE(davison.AllPlanets().size() == a.AllPlanets().size());
	CHECK(davison.GetHouseSystem() == HouseSystem::Placidus);

	// the planets are where they were at that moment, and the houses are for that place
	auto sun = calc.CalcPlanet(Planet::Sun, info.Time, 1, true);
	CHECK(Diff(davison.AllPlanets()[0].Longitude.Value, sun.Longitude.Value) < MicroDegree);
	auto houses = calc.CalcHouses(info.Time, info.Latitude, info.Longitude, HouseSystem::Placidus);
	CHECK(Diff(davison.Houses().Asc.Value, houses.Asc.Value) < MicroDegree);
}

TEST_CASE("Davison longitude takes the short way round the antimeridian", "[Derived]") {
	AstroCalculator calc;
	auto a = Natal(2000, 1, 1, 12, 0, 0, 170);
	auto b = Natal(2000, 1, 1, 12, 0, 0, -170);
	auto davison = DerivedCharts::Davison(calc, a, b);
	CHECK(std::fabs(std::fabs(davison.Info().Longitude) - 180) < 1e-6);
}

TEST_CASE("A recipe builds the same chart as making it directly", "[Derived]") {
	AstroCalculator calc;
	auto a = Natal(1980, 3, 10, 6, 0, 40, -74);
	auto b = Natal(1990, 7, 22, 15, 30, 51, 0);

	// the recipe's sources are only their details: no positions
	auto bare = [](ChartData chart) {
		for (auto& planet : chart.AllPlanets())
			planet.Longitude = AstroPoint(0);
		chart.Houses() = {};
		return chart;
	};

	DerivedRecipe recipe;
	recipe.A = bare(a);
	recipe.B = bare(b);
	for (auto houses : { CompositeHouses::MidpointMC, CompositeHouses::MidpointARMC }) {
		recipe.Kind = DerivedKind::Composite;
		recipe.Houses = houses;
		auto built = DerivedCharts::Build(calc, recipe);
		auto direct = DerivedCharts::Composite(calc, a, b, houses);
		REQUIRE(built.AllPlanets().size() == direct.AllPlanets().size());
		for (size_t i = 0; i < built.AllPlanets().size(); i++)
			CHECK(Diff(built.AllPlanets()[i].Longitude.Value, direct.AllPlanets()[i].Longitude.Value) < MicroDegree);
		CHECK(Diff(built.Houses().MC.Value, direct.Houses().MC.Value) < MicroDegree);
	}

	recipe.Kind = DerivedKind::Davison;
	auto davison = DerivedCharts::Build(calc, recipe);
	auto directDavison = DerivedCharts::Davison(calc, a, b);
	CHECK(davison.Info().Time.Julian() == Approx(directDavison.Info().Time.Julian()));
	for (size_t i = 0; i < davison.AllPlanets().size(); i++)
		CHECK(Diff(davison.AllPlanets()[i].Longitude.Value, directDavison.AllPlanets()[i].Longitude.Value) < MicroDegree);

	recipe.Kind = DerivedKind::SolarArc;
	recipe.Target = DateTime(2030, 1, 1, 0, 0, 0);
	recipe.Key = ArcKey::Naibod;
	auto arc = DerivedCharts::Build(calc, recipe);
	ProgressionOptions options;
	options.Method = ProgressionMethod::SolarArc;
	options.Key = ArcKey::Naibod;
	auto directArc = DerivedCharts::Progress(calc, a, recipe.Target, options);
	for (size_t i = 0; i < arc.AllPlanets().size(); i++)
		CHECK(Diff(arc.AllPlanets()[i].Longitude.Value, directArc.AllPlanets()[i].Longitude.Value) < MicroDegree);
	CHECK(Diff(arc.Houses().Asc.Value, directArc.Houses().Asc.Value) < MicroDegree);
}
