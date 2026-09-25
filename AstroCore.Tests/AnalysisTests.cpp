#include "TestCommon.h"
#include "Analysis.h"
#include <algorithm>
#include <cmath>

namespace {
	ChartData Natal(std::vector<Planet> planets, DateTime time, double latitude = 40, double longitude = -74) {
		ChartData chart;
		chart.AddPlanets(planets);
		chart.Info().Time = time;
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

	// only the conjunction, within an orb
	AspectSettings ConjunctionOnly(double orb) {
		AspectSettings aspects;
		aspects.AspectEnabled.fill(false);
		aspects.AspectEnabled[static_cast<int>(AspectType::Conjunction)] = true;
		aspects.CustomOrb(AspectType::Conjunction, orb);
		return aspects;
	}

	AnalysisSettings Transits(DateTime from, DateTime to, std::vector<Planet> movers, std::vector<Planet> targets, AspectSettings aspects) {
		AnalysisSettings settings;
		settings.Type = AnalysisType::TransitsToNatal;
		settings.From = from;
		settings.To = to;
		settings.Movers = std::move(movers);
		settings.Targets = std::move(targets);
		settings.Aspects = std::move(aspects);
		settings.NatalAngles = false;
		return settings;
	}

	std::vector<AnalysisEvent> Of(AnalysisResult const& result, AnalysisEventKind kind) {
		std::vector<AnalysisEvent> events;
		for (auto const& event : result.Events)
			if (event.Kind == kind)
				events.push_back(event);
		return events;
	}

	double SunAt(AstroCalculator const& calc, DateTime const& time) {
		return calc.CalcPlanet(Planet::Sun, time, 1, false).Longitude.Value;
	}

	// the longitude of a planet in a derived chart
	double LongitudeIn(ChartData const& chart, Planet planet) {
		for (auto const& p : chart.AllPlanets())
			if (p.Planet == planet)
				return p.Longitude.Value;
		return -1;
	}
}

TEST_CASE("A transit conjunction enters its orb, is exact, and leaves", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun }, DateTime(2000, 1, 1, 12, 0, 0));
	auto settings = Transits(DateTime(2000, 12, 25, 0, 0, 0), DateTime(2001, 1, 8, 0, 0, 0), { Planet::Sun }, { Planet::Sun }, ConjunctionOnly(1));
	auto result = Analysis::Run(calc, natal, settings);
	REQUIRE_FALSE(result.Cancelled);
	REQUIRE(result.Events.size() == 3);

	// the Sun comes back to its place about a day before the anniversary
	auto const& enter = result.Events[0];
	auto const& exact = result.Events[1];
	auto const& leave = result.Events[2];
	CHECK(enter.Kind == AnalysisEventKind::EnterOrb);
	CHECK(exact.Kind == AnalysisEventKind::Exact);
	CHECK(leave.Kind == AnalysisEventKind::LeaveOrb);
	CHECK(enter.Time.Julian() < exact.Time.Julian());
	CHECK(exact.Time.Julian() < leave.Time.Julian());
	for (auto const& event : result.Events) {
		CHECK(event.Mover == Planet::Sun);
		CHECK(event.Target == Planet::Sun);
		CHECK(event.TargetKind == AnalysisTarget::Planet);
		CHECK(event.Aspect == AspectType::Conjunction);
		CHECK(event.Pass == 1);
		CHECK_FALSE(event.Retrograde);
		CHECK(event.Orb == Approx(1));
	}

	double natalSun = natal.AllPlanets()[0].Longitude.Value;
	CHECK(Diff(SunAt(calc, exact.Time), natalSun) < 1e-5);
	CHECK(Diff(SunAt(calc, enter.Time), natalSun) == Approx(1).margin(1e-5));
	CHECK(Diff(SunAt(calc, leave.Time), natalSun) == Approx(1).margin(1e-5));
	// (the Sun makes about a degree a day, so the orb of 1 degree is about two days wide)
	CHECK(leave.Time.Julian() - enter.Time.Julian() == Approx(2).margin(0.1));
}

TEST_CASE("A retrograde planet makes an aspect three times", "[Analysis]") {
	AstroCalculator calc;
	// Mars turned retrograde on 30 October 2022 and direct on 12 January 2023
	auto retro = calc.CalcPlanetStation(Planet::Mars, DateTime(2022, 10, 1, 0, 0, 0));
	auto direct = calc.CalcPlanetStation(Planet::Mars, DateTime(retro.Time.Julian() + 5, true));
	REQUIRE(retro.IsTurningRetrograde);
	REQUIRE_FALSE(direct.IsTurningRetrograde);

	// a natal point in the stretch it goes back over
	auto natal = Natal({ Planet::Sun }, DateTime(2000, 1, 1, 12, 0, 0));
	double middle = AstroPoint::MidPoint(retro.Position, direct.Position).Value;
	natal.AllPlanets()[0].Longitude = AstroPoint(middle);

	auto settings = Transits(DateTime(retro.Time.Julian() - 120, true), DateTime(direct.Time.Julian() + 120, true), { Planet::Mars }, { Planet::Sun }, ConjunctionOnly(1));
	auto result = Analysis::Run(calc, natal, settings);
	auto exact = Of(result, AnalysisEventKind::Exact);
	REQUIRE(exact.size() == 3);
	CHECK(exact[0].Pass == 1);
	CHECK(exact[1].Pass == 2);
	CHECK(exact[2].Pass == 3);
	CHECK_FALSE(exact[0].Retrograde);
	CHECK(exact[1].Retrograde);
	CHECK_FALSE(exact[2].Retrograde);
	CHECK(Of(result, AnalysisEventKind::EnterOrb).size() == 3);
	CHECK(Of(result, AnalysisEventKind::LeaveOrb).size() == 3);
	for (auto const& event : exact)
		CHECK(Diff(calc.CalcPlanet(Planet::Mars, event.Time, 1, false).Longitude.Value, middle) < 1e-5);
}

TEST_CASE("The Moon is left out of long transit ranges", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun, Planet::Moon, Planet::Mars }, DateTime(2000, 1, 1, 12, 0, 0));
	AspectSettings aspects;

	auto shortRange = Transits(DateTime(2024, 1, 1, 0, 0, 0), DateTime(2024, 1, 31, 0, 0, 0), { Planet::Moon, Planet::Sun }, { Planet::Mars }, aspects);
	CHECK_FALSE(shortRange.MoonDropped());
	CHECK(shortRange.EffectiveMovers() == std::vector<Planet>{ Planet::Moon, Planet::Sun });

	auto longRange = Transits(DateTime(2024, 1, 1, 0, 0, 0), DateTime(2024, 6, 1, 0, 0, 0), { Planet::Moon, Planet::Sun }, { Planet::Mars }, aspects);
	CHECK(longRange.MoonDropped());
	CHECK(longRange.EffectiveMovers() == std::vector<Planet>{ Planet::Sun });
	for (auto const& event : Analysis::Run(calc, natal, longRange).Events)
		CHECK(event.Mover != Planet::Moon);

	// the edge: two months
	auto edge = Transits(DateTime(2024, 1, 1, 0, 0, 0), DateTime(2024, 3, 2, 0, 0, 0), { Planet::Moon }, { Planet::Mars }, aspects);
	CHECK_FALSE(edge.MoonDropped());
	edge.To = DateTime(2024, 3, 4, 0, 0, 0);
	CHECK(edge.MoonDropped());

	// progressed and directed Moons move slowly and stay
	longRange.Type = AnalysisType::ProgressedToNatal;
	CHECK_FALSE(longRange.MoonDropped());
	CHECK(longRange.EffectiveMovers().size() == 2);
	longRange.Type = AnalysisType::TransitsToProgressed;
	CHECK(longRange.MoonDropped());
}

TEST_CASE("An aspect already in orb when the range begins or ends is reported", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun }, DateTime(2000, 1, 1, 12, 0, 0));
	auto wide = Analysis::Run(calc, natal, Transits(DateTime(2000, 12, 25, 0, 0, 0), DateTime(2001, 1, 8, 0, 0, 0), { Planet::Sun }, { Planet::Sun }, ConjunctionOnly(1)));
	REQUIRE(Of(wide, AnalysisEventKind::Exact).size() == 1);
	double exact = Of(wide, AnalysisEventKind::Exact)[0].Time.Julian();

	auto inside = Analysis::Run(calc, natal, Transits(DateTime(exact - 0.2, true), DateTime(exact + 0.2, true), { Planet::Sun }, { Planet::Sun }, ConjunctionOnly(1)));
	REQUIRE(inside.Events.size() == 3);
	CHECK(inside.Events[0].Kind == AnalysisEventKind::InOrbAtStart);
	CHECK(inside.Events[1].Kind == AnalysisEventKind::Exact);
	CHECK(inside.Events[2].Kind == AnalysisEventKind::InOrbAtEnd);
	CHECK(inside.Events[0].Pass == 1);
	CHECK(inside.Events[2].Pass == 1);

	// starting in the orb, then leaving it
	auto leaving = Analysis::Run(calc, natal, Transits(DateTime(exact - 0.2, true), DateTime(exact + 3, true), { Planet::Sun }, { Planet::Sun }, ConjunctionOnly(1)));
	REQUIRE(leaving.Events.size() == 3);
	CHECK(leaving.Events[0].Kind == AnalysisEventKind::InOrbAtStart);
	CHECK(leaving.Events[1].Kind == AnalysisEventKind::Exact);
	CHECK(leaving.Events[2].Kind == AnalysisEventKind::LeaveOrb);
}

TEST_CASE("Sign and house ingresses", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun }, DateTime(2000, 1, 1, 12, 0, 0));
	auto settings = Transits(DateTime(2024, 3, 10, 0, 0, 0), DateTime(2024, 4, 10, 0, 0, 0), { Planet::Sun }, {}, AspectSettings());
	settings.AspectEvents = false;
	settings.SignIngresses = true;
	settings.HouseIngresses = true;
	auto result = Analysis::Run(calc, natal, settings);

	auto signs = Of(result, AnalysisEventKind::SignIngress);
	REQUIRE(signs.size() == 1);
	CHECK(signs[0].Index == 0);		// Aries, at 03:06 UT on 20 March
	CHECK(signs[0].Time.Julian() == Approx(DateTime(2024, 3, 20, 3, 6, 0).Julian()).margin(0.005));
	CHECK(Diff(signs[0].Longitude, 0) < 1e-5);

	auto houses = Of(result, AnalysisEventKind::HouseIngress);
	REQUIRE_FALSE(houses.empty());
	for (auto const& event : houses) {
		// the Sun is in that house just after and in the one before just before
		auto after = calc.CalcPlanet(Planet::Sun, DateTime(event.Time.Julian() + 0.001, true), 1, false).Longitude;
		auto before = calc.CalcPlanet(Planet::Sun, DateTime(event.Time.Julian() - 0.001, true), 1, false).Longitude;
		CHECK(DerivedCharts::HouseOf(natal.Houses(), after) == event.Index);
		CHECK(DerivedCharts::HouseOf(natal.Houses(), before) == (event.Index + 10) % 12 + 1);
		CHECK(Diff(event.Longitude, natal.Houses().Cusps[event.Index - 1].Value) < 1e-5);
	}
}

TEST_CASE("A retrograde planet enters the previous sign going back", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun }, DateTime(2000, 1, 1, 12, 0, 0));
	// Mercury turned retrograde at the end of Aries in April 2024... its sign changes are what they are: each one enters
	// the sign it is going into
	auto settings = Transits(DateTime(2024, 1, 1, 0, 0, 0), DateTime(2024, 12, 31, 0, 0, 0), { Planet::Mercury }, {}, AspectSettings());
	settings.AspectEvents = false;
	settings.SignIngresses = true;
	auto result = Analysis::Run(calc, natal, settings);
	auto signs = Of(result, AnalysisEventKind::SignIngress);
	REQUIRE(signs.size() >= 4);
	for (auto const& event : signs) {
		auto after = calc.CalcPlanet(Planet::Mercury, DateTime(event.Time.Julian() + 0.001, true), 1, true);
		CHECK(static_cast<int>(after.Longitude.Sign()) == event.Index);
		CHECK(event.Retrograde == (after.Speed < 0));
	}
}

TEST_CASE("Stations", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun }, DateTime(2000, 1, 1, 12, 0, 0));
	auto settings = Transits(DateTime(2024, 3, 25, 0, 0, 0), DateTime(2024, 5, 1, 0, 0, 0), { Planet::Mercury }, {}, AspectSettings());
	settings.AspectEvents = false;
	settings.Stations = true;
	auto result = Analysis::Run(calc, natal, settings);
	REQUIRE(result.Events.size() == 2);
	// turned retrograde on 1 April 2024 and direct on 25 April
	CHECK(result.Events[0].Kind == AnalysisEventKind::StationRetrograde);
	CHECK(result.Events[0].Time.Julian() == Approx(DateTime(2024, 4, 1, 12, 0, 0).Julian()).margin(1.0));
	CHECK(result.Events[0].Retrograde);
	CHECK(result.Events[1].Kind == AnalysisEventKind::StationDirect);
	CHECK(result.Events[1].Time.Julian() == Approx(DateTime(2024, 4, 25, 12, 0, 0).Julian()).margin(1.0));
	CHECK_FALSE(result.Events[1].Retrograde);
	for (auto const& event : result.Events)
		CHECK(std::abs(calc.CalcPlanet(Planet::Mercury, event.Time).Speed) < 0.001);
}

TEST_CASE("The angles of the birth chart are targets", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun }, DateTime(2000, 1, 1, 12, 0, 0));
	auto settings = Transits(DateTime(2024, 1, 1, 0, 0, 0), DateTime(2025, 1, 1, 0, 0, 0), { Planet::Sun }, {}, ConjunctionOnly(1));
	settings.NatalAngles = true;
	auto result = Analysis::Run(calc, natal, settings);

	auto exact = Of(result, AnalysisEventKind::Exact);
	REQUIRE(exact.size() == 2);		// the Ascendant and the Midheaven, once each
	for (auto const& event : exact) {
		REQUIRE(event.TargetKind != AnalysisTarget::Planet);
		double angle = event.TargetKind == AnalysisTarget::Ascendant ? natal.Houses().Asc.Value : natal.Houses().MC.Value;
		CHECK(Diff(SunAt(calc, event.Time), angle) < 1e-5);
	}

	settings.NatalAngles = false;
	CHECK(Analysis::Run(calc, natal, settings).Events.empty());
}

TEST_CASE("Progressed planets to natal ones", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun, Planet::Moon, Planet::Mars, Planet::Saturn }, DateTime(1980, 6, 15, 8, 30, 0));
	AnalysisSettings settings;
	settings.Type = AnalysisType::ProgressedToNatal;
	settings.From = DateTime(1990, 1, 1, 0, 0, 0);
	settings.To = DateTime(2020, 1, 1, 0, 0, 0);
	settings.Movers = { Planet::Moon };
	settings.Targets = { Planet::Sun, Planet::Mars, Planet::Saturn };
	settings.NatalAngles = false;
	auto result = Analysis::Run(calc, natal, settings);

	auto exact = Of(result, AnalysisEventKind::Exact);
	REQUIRE(exact.size() >= 10);		// the progressed Moon goes round the zodiac in 27 years
	for (auto const& event : exact) {
		CHECK(event.Mover == Planet::Moon);
		auto progressed = DerivedCharts::Progress(calc, natal, event.Time);
		double moon = LongitudeIn(progressed, Planet::Moon);
		CHECK(Diff(event.Longitude, moon) < 1e-3);
		CHECK(Diff(moon, LongitudeIn(natal, event.Target)) == Approx(AspectCalculator::GetAspectAngle(event.Aspect)).margin(2e-3));
	}
	// every aspect the Moon enters it also leaves, in order
	int inside = 0;
	for (auto const& event : result.Events) {
		if (event.Kind == AnalysisEventKind::EnterOrb || event.Kind == AnalysisEventKind::InOrbAtStart)
			inside++;
		if (event.Kind == AnalysisEventKind::LeaveOrb)
			inside--;
		CHECK(inside >= 0);
	}
}

TEST_CASE("Solar arc directions to natal planets", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun, Planet::Moon, Planet::Venus }, DateTime(1975, 3, 3, 14, 0, 0));
	for (auto key : { ArcKey::Actual, ArcKey::Naibod }) {
		AnalysisSettings settings;
		settings.Type = AnalysisType::SolarArcToNatal;
		settings.Key = key;
		settings.From = DateTime(1975, 3, 3, 14, 0, 0);
		settings.To = DateTime(2045, 3, 3, 14, 0, 0);
		settings.Movers = { Planet::Sun, Planet::Venus };
		settings.Targets = { Planet::Moon };
		settings.NatalAngles = false;
		auto result = Analysis::Run(calc, natal, settings);
		auto exact = Of(result, AnalysisEventKind::Exact);
		REQUIRE_FALSE(exact.empty());
		for (auto const& event : exact) {
			ProgressionOptions options;
			options.Method = ProgressionMethod::SolarArc;
			options.Key = key;
			auto directed = DerivedCharts::Progress(calc, natal, event.Time, options);
			double mover = LongitudeIn(directed, event.Mover);
			CHECK(Diff(event.Longitude, mover) < 1e-3);
			CHECK(Diff(mover, LongitudeIn(natal, Planet::Moon)) == Approx(AspectCalculator::GetAspectAngle(event.Aspect)).margin(2e-3));
			CHECK_FALSE(event.Retrograde);
		}
	}
}

TEST_CASE("Transits to progressed planets", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun, Planet::Moon, Planet::Jupiter }, DateTime(1985, 11, 20, 3, 15, 0));
	AnalysisSettings settings;
	settings.Type = AnalysisType::TransitsToProgressed;
	settings.From = DateTime(2020, 1, 1, 0, 0, 0);
	settings.To = DateTime(2023, 1, 1, 0, 0, 0);
	settings.Movers = { Planet::Jupiter };
	settings.Targets = { Planet::Sun, Planet::Moon };
	auto result = Analysis::Run(calc, natal, settings);

	auto exact = Of(result, AnalysisEventKind::Exact);
	REQUIRE_FALSE(exact.empty());
	for (auto const& event : exact) {
		double transit = calc.CalcPlanet(Planet::Jupiter, event.Time, 1, false).Longitude.Value;
		auto progressed = DerivedCharts::Progress(calc, natal, event.Time);
		double target = LongitudeIn(progressed, event.Target);
		CHECK(Diff(transit, target) == Approx(AspectCalculator::GetAspectAngle(event.Aspect)).margin(2e-3));
	}
}

TEST_CASE("Progressed planets to each other, every pair once", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun, Planet::Moon, Planet::Mercury, Planet::Venus, Planet::Mars }, DateTime(1990, 2, 10, 20, 0, 0));
	AnalysisSettings settings;
	settings.Type = AnalysisType::ProgressedToProgressed;
	settings.From = DateTime(1990, 2, 10, 20, 0, 0);
	settings.To = DateTime(2030, 2, 10, 20, 0, 0);
	settings.Movers = { Planet::Moon, Planet::Mercury, Planet::Venus, Planet::Sun, Planet::Mars };
	settings.Targets = settings.Movers;
	auto result = Analysis::Run(calc, natal, settings);

	auto exact = Of(result, AnalysisEventKind::Exact);
	REQUIRE(exact.size() > 20);
	for (auto const& event : exact) {
		CHECK(event.Mover != event.Target);
		CHECK(event.Mover < event.Target);		// (each pair once, whichever way round)
		auto progressed = DerivedCharts::Progress(calc, natal, event.Time);
		CHECK(Diff(LongitudeIn(progressed, event.Mover), LongitudeIn(progressed, event.Target)) ==
			Approx(AspectCalculator::GetAspectAngle(event.Aspect)).margin(3e-3));
	}
}

TEST_CASE("Events come in time order with the passes counted", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun, Planet::Moon, Planet::Mars, Planet::Saturn }, DateTime(1980, 6, 15, 8, 30, 0));
	auto settings = Transits(DateTime(2023, 1, 1, 0, 0, 0), DateTime(2024, 1, 1, 0, 0, 0), { Planet::Sun, Planet::Mars, Planet::Jupiter, Planet::Saturn },
		{ Planet::Sun, Planet::Moon, Planet::Mars, Planet::Saturn }, AspectSettings());
	settings.HouseIngresses = true;
	settings.SignIngresses = true;
	settings.Stations = true;
	auto result = Analysis::Run(calc, natal, settings);
	REQUIRE(result.Events.size() > 50);
	CHECK(std::is_sorted(result.Events.begin(), result.Events.end(), [](auto const& a, auto const& b) { return a.Time.Julian() < b.Time.Julian(); }));
	for (auto const& event : result.Events) {
		if (event.Aspect != AspectType::None) {
			CHECK(event.Pass >= 1);
			CHECK(event.Pass <= 3);
		}
	}
}

TEST_CASE("An analysis can be cancelled and reports its progress", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun, Planet::Moon }, DateTime(2000, 1, 1, 12, 0, 0));
	auto settings = Transits(DateTime(2020, 1, 1, 0, 0, 0), DateTime(2030, 1, 1, 0, 0, 0), { Planet::Sun, Planet::Mars }, { Planet::Sun, Planet::Moon }, AspectSettings());

	double last = -1;
	int calls = 0;
	auto watch = [&](double fraction) {
		calls++;
		CHECK(fraction >= last);
		CHECK(fraction >= 0);
		CHECK(fraction <= 1.0000001);
		last = fraction;
		return true;
	};
	auto done = Analysis::Run(calc, natal, settings, watch);
	CHECK_FALSE(done.Cancelled);
	CHECK(calls > 2);
	CHECK(last == Approx(1));

	auto cancelled = Analysis::Run(calc, natal, settings, [](double) { return false; });
	CHECK(cancelled.Cancelled);
	CHECK(cancelled.Events.empty());
}

TEST_CASE("An empty range or nothing to watch gives nothing", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun }, DateTime(2000, 1, 1, 12, 0, 0));
	auto backwards = Transits(DateTime(2024, 1, 2, 0, 0, 0), DateTime(2024, 1, 1, 0, 0, 0), { Planet::Sun }, { Planet::Sun }, AspectSettings());
	CHECK(Analysis::Run(calc, natal, backwards).Events.empty());
	auto nothing = Transits(DateTime(2024, 1, 1, 0, 0, 0), DateTime(2024, 6, 1, 0, 0, 0), {}, { Planet::Sun }, AspectSettings());
	CHECK(Analysis::Run(calc, natal, nothing).Events.empty());
	// only the aspects that are switched on are looked for
	auto none = Transits(DateTime(2024, 1, 1, 0, 0, 0), DateTime(2024, 6, 1, 0, 0, 0), { Planet::Sun }, { Planet::Sun }, AspectSettings());
	none.Aspects.AspectEnabled.fill(false);
	CHECK(Analysis::Run(calc, natal, none).Events.empty());
}

TEST_CASE("Several analyses run together, their events merged and tagged", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun, Planet::Moon, Planet::Mercury, Planet::Venus, Planet::Mars }, DateTime(1990, 2, 10, 20, 0, 0));
	AnalysisSettings settings;
	settings.Types = { AnalysisType::TransitsToNatal, AnalysisType::ProgressedToNatal, AnalysisType::TransitsToProgressed };
	settings.Type = settings.Types[0];
	settings.From = DateTime(2026, 1, 1, 0, 0, 0);
	settings.To = DateTime(2027, 1, 1, 0, 0, 0);
	settings.Movers = { Planet::Sun, Planet::Mars, Planet::Jupiter, Planet::Moon };
	settings.Targets = { Planet::Sun, Planet::Moon, Planet::Venus };
	settings.SignIngresses = true;
	settings.Stations = true;
	settings.HouseIngresses = true;

	auto together = Analysis::RunAll(calc, natal, settings);
	REQUIRE_FALSE(together.Cancelled);
	CHECK(std::is_sorted(together.Events.begin(), together.Events.end(), [](auto const& a, auto const& b) { return a.Time.Julian() < b.Time.Julian(); }));

	// each analysis on its own finds the same events for what is its own
	size_t transits = 0, progressed = 0, toProgressed = 0;
	for (auto const& event : together.Events) {
		transits += event.Type == AnalysisType::TransitsToNatal;
		progressed += event.Type == AnalysisType::ProgressedToNatal;
		toProgressed += event.Type == AnalysisType::TransitsToProgressed;
	}
	auto alone = [&](AnalysisType type) {
		auto one = settings;
		one.Type = type;
		one.Types.clear();
		return Analysis::Run(calc, natal, one).Events;
	};
	CHECK(transits == alone(AnalysisType::TransitsToNatal).size());
	CHECK(progressed == alone(AnalysisType::ProgressedToNatal).size());
	CHECK(toProgressed > 0);
	CHECK(toProgressed < alone(AnalysisType::TransitsToProgressed).size());		// (its sign ingresses and stations are the first analysis's)
	for (auto const& event : Of(together, AnalysisEventKind::SignIngress))
		CHECK(event.Type != AnalysisType::TransitsToProgressed);
	for (auto const& event : Of(together, AnalysisEventKind::StationRetrograde))
		CHECK(event.Type != AnalysisType::TransitsToProgressed);
	for (auto const& event : Of(together, AnalysisEventKind::HouseIngress))
		CHECK(event.Type != AnalysisType::TransitsToProgressed);

	// a single type gives the same as Run, tagged
	auto single = settings;
	single.Types = { AnalysisType::SolarArcToNatal };
	auto solar = Analysis::RunAll(calc, natal, single);
	for (auto const& event : solar.Events)
		CHECK(event.Type == AnalysisType::SolarArcToNatal);
	single.Types.clear();
	single.Type = AnalysisType::SolarArcToNatal;
	CHECK(solar.Events.size() == Analysis::Run(calc, natal, single).Events.size());
	CHECK(single.TypeList() == std::vector<AnalysisType>{ AnalysisType::SolarArcToNatal });
}

TEST_CASE("Running several analyses reports progress across them and can be cancelled", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun, Planet::Moon }, DateTime(2000, 1, 1, 12, 0, 0));
	AnalysisSettings settings;
	settings.Types = { AnalysisType::TransitsToNatal, AnalysisType::ProgressedToNatal };
	settings.From = DateTime(2020, 1, 1, 0, 0, 0);
	settings.To = DateTime(2030, 1, 1, 0, 0, 0);
	settings.Movers = { Planet::Sun, Planet::Mars };
	settings.Targets = { Planet::Sun, Planet::Moon };

	double last = -1;
	auto watch = [&](double fraction) {
		CHECK(fraction >= last);
		CHECK(fraction <= 1.0000001);
		last = fraction;
		return true;
	};
	Analysis::RunAll(calc, natal, settings, watch);
	CHECK(last == Approx(1));

	int calls = 0;
	auto cancelled = Analysis::RunAll(calc, natal, settings, [&](double) { return ++calls < 3; });
	CHECK(cancelled.Cancelled);
	CHECK(cancelled.Events.empty());
}

TEST_CASE("The event that ends a stay within an orb has the whole stay", "[Analysis]") {
	AstroCalculator calc;
	auto natal = Natal({ Planet::Sun }, DateTime(2000, 1, 1, 12, 0, 0));
	auto result = Analysis::Run(calc, natal, Transits(DateTime(2000, 12, 25, 0, 0, 0), DateTime(2001, 1, 8, 0, 0, 0), { Planet::Sun }, { Planet::Sun }, ConjunctionOnly(1)));
	REQUIRE(result.Events.size() == 3);
	CHECK_FALSE(result.Events[0].Window);
	CHECK_FALSE(result.Events[1].Window);
	REQUIRE(result.Events[2].Window);
	auto const& stay = *result.Events[2].Window;
	CHECK(stay.HasEnter);
	CHECK(stay.HasLeave);
	CHECK(stay.Enter.Julian() == result.Events[0].Time.Julian());
	REQUIRE(stay.Exacts.size() == 1);
	CHECK(stay.Exacts[0].Julian() == result.Events[1].Time.Julian());
	CHECK(stay.Leave.Julian() == result.Events[2].Time.Julian());

	// a range that begins inside the orb: no entry; one that ends inside it: no leaving
	double exact = result.Events[1].Time.Julian();
	auto starting = Analysis::Run(calc, natal, Transits(DateTime(exact - 0.2, true), DateTime(exact + 3, true), { Planet::Sun }, { Planet::Sun }, ConjunctionOnly(1)));
	REQUIRE(starting.Events.size() == 3);
	REQUIRE(starting.Events[2].Window);
	CHECK_FALSE(starting.Events[2].Window->HasEnter);
	CHECK(starting.Events[2].Window->HasLeave);
	CHECK(starting.Events[2].Window->Exacts.size() == 1);

	auto ending = Analysis::Run(calc, natal, Transits(DateTime(exact - 3, true), DateTime(exact + 0.2, true), { Planet::Sun }, { Planet::Sun }, ConjunctionOnly(1)));
	REQUIRE(ending.Events.size() == 3);
	REQUIRE(ending.Events[2].Kind == AnalysisEventKind::InOrbAtEnd);
	REQUIRE(ending.Events[2].Window);
	CHECK(ending.Events[2].Window->HasEnter);
	CHECK_FALSE(ending.Events[2].Window->HasLeave);
}

TEST_CASE("Each pass of a retrograde planet is a stay of its own", "[Analysis]") {
	AstroCalculator calc;
	auto retro = calc.CalcPlanetStation(Planet::Mars, DateTime(2022, 10, 1, 0, 0, 0));
	auto direct = calc.CalcPlanetStation(Planet::Mars, DateTime(retro.Time.Julian() + 5, true));
	auto natal = Natal({ Planet::Sun }, DateTime(2000, 1, 1, 12, 0, 0));
	natal.AllPlanets()[0].Longitude = AstroPoint(AstroPoint::MidPoint(retro.Position, direct.Position).Value);
	auto result = Analysis::Run(calc, natal, Transits(DateTime(retro.Time.Julian() - 120, true), DateTime(direct.Time.Julian() + 120, true), { Planet::Mars }, { Planet::Sun }, ConjunctionOnly(1)));

	auto leaves = Of(result, AnalysisEventKind::LeaveOrb);
	REQUIRE(leaves.size() == 3);
	double previousLeave = 0;
	for (auto const& leave : leaves) {
		REQUIRE(leave.Window);
		auto const& stay = *leave.Window;
		CHECK(stay.HasEnter);
		REQUIRE(stay.Exacts.size() == 1);
		CHECK(stay.Enter.Julian() < stay.Exacts[0].Julian());
		CHECK(stay.Exacts[0].Julian() < stay.Leave.Julian());
		CHECK(stay.Enter.Julian() > previousLeave);
		previousLeave = stay.Leave.Julian();
	}
}

TEST_CASE("Analysis settings can be kept as text and read back", "[Analysis]") {
	AnalysisSettings settings;
	settings.Types = { AnalysisType::TransitsToNatal, AnalysisType::SolarArcToNatal };
	settings.Type = settings.Types[0];
	settings.From = DateTime(2026, 1, 1, 0, 0, 0);
	settings.To = DateTime(2027, 6, 1, 0, 0, 0);
	settings.Movers = { Planet::Sun, Planet::Mars, Planet::Chiron };
	settings.Targets = { Planet::Moon, Planet::Venus };
	settings.NatalAngles = false;
	settings.AspectEvents = true;
	settings.Aspects.AspectEnabled.fill(false);
	settings.Aspects.AspectEnabled[static_cast<int>(AspectType::Conjunction)] = true;
	settings.Aspects.AspectEnabled[static_cast<int>(AspectType::Quintile)] = true;
	settings.HouseIngresses = true;
	settings.SignIngresses = false;
	settings.Stations = true;

	auto text = settings.ToText();
	AnalysisSettings read;
	read.From = DateTime(2030, 3, 3, 0, 0, 0);
	read.FromText(text);
	CHECK(read.TypeList() == settings.TypeList());
	CHECK(read.Type == AnalysisType::TransitsToNatal);
	CHECK(read.Movers == settings.Movers);
	CHECK(read.Targets == settings.Targets);
	CHECK_FALSE(read.NatalAngles);
	CHECK(read.AspectEvents);
	CHECK(read.Aspects.AspectEnabled == settings.Aspects.AspectEnabled);
	CHECK(read.HouseIngresses);
	CHECK_FALSE(read.SignIngresses);
	CHECK(read.Stations);
	// the range comes back as a length, from wherever it starts now
	CHECK(read.To.Julian() - read.From.Julian() == Approx(settings.To.Julian() - settings.From.Julian()));
	CHECK(read.ToText() == text);

	// "major only" is kept as the aspects that are on
	AnalysisSettings majors;
	majors.Aspects.MajorOnly = true;
	AnalysisSettings readMajors;
	readMajors.FromText(majors.ToText());
	CHECK_FALSE(readMajors.Aspects.MajorOnly);
	for (int i = 0; i < AspectSettings::AspectTypeCount; i++)
		CHECK(readMajors.Aspects.AspectEnabled[i] == (i <= static_cast<int>(AspectType::Opposition)));
}

TEST_CASE("Analysis settings text that is damaged or empty leaves the rest alone", "[Analysis]") {
	AnalysisSettings settings;
	settings.Movers = { Planet::Sun };
	settings.Type = AnalysisType::TransitsToNatal;
	settings.Types = { AnalysisType::ProgressedToNatal };
	settings.FromText(L"");
	CHECK(settings.Movers == std::vector<Planet>{ Planet::Sun });
	settings.FromText(L"nonsense;types=;movers=1,x,99,-3,2;days=abc;=;;angles");
	CHECK(settings.TypeList() == std::vector<AnalysisType>{ AnalysisType::TransitsToNatal });		// (an empty list falls back to Type, which is what it was)
	CHECK(settings.Movers == std::vector<Planet>{ Planet::Moon, Planet::Mercury });
}

TEST_CASE("Analysis events as text and back", "[Analysis][File]") {
	// a real analysis: transits and progressions over a year, with everything on that leaves a trace
	AstroCalculator calc;
	ChartData natal;
	natal.AddPlanets(std::vector<Planet>{ Planet::Sun, Planet::Moon, Planet::Mercury, Planet::Venus, Planet::Mars, Planet::Jupiter, Planet::Saturn });
	natal.Info().Time = DateTime(1980, 5, 17, 9, 40, 0);
	natal.Info().Latitude = 40.7;
	natal.Info().Longitude = -74;
	natal.SetHouseSystem(HouseSystem::Placidus);
	calc.Calculate(natal);

	AnalysisSettings settings;
	settings.Types = { AnalysisType::TransitsToNatal, AnalysisType::ProgressedToNatal };
	settings.Type = AnalysisType::TransitsToNatal;
	settings.From = DateTime(2024, 1, 1, 0, 0, 0);
	settings.To = DateTime(2025, 1, 1, 0, 0, 0);
	settings.Movers = { Planet::Sun, Planet::Mars, Planet::Jupiter, Planet::Saturn };
	settings.Targets = { Planet::Sun, Planet::Moon, Planet::Venus, Planet::Saturn };
	settings.HouseIngresses = true;
	settings.SignIngresses = true;
	settings.Stations = true;
	auto result = Analysis::RunAll(calc, natal, settings);
	REQUIRE(result.Events.size() > 50);

	auto text = Analysis::EventsToText(result.Events);
	std::vector<AnalysisEvent> back;
	std::string error;
	REQUIRE(Analysis::EventsFromText(text, back, error));
	INFO(error);
	REQUIRE(back.size() == result.Events.size());

	int windows = 0, exacts = 0;
	for (size_t i = 0; i < back.size(); i++) {
		auto const& a = result.Events[i];
		auto const& b = back[i];
		CAPTURE(i);
		CHECK(b.Type == a.Type);
		CHECK(std::fabs(b.Time.Julian() - a.Time.Julian()) < 1e-8);
		CHECK(b.Kind == a.Kind);
		CHECK(b.Mover == a.Mover);
		CHECK(b.TargetKind == a.TargetKind);
		CHECK(b.Target == a.Target);
		CHECK(b.Aspect == a.Aspect);
		CHECK(b.Orb == a.Orb);
		CHECK(b.Pass == a.Pass);
		CHECK(b.Retrograde == a.Retrograde);
		CHECK(b.Index == a.Index);
		CHECK(std::fabs(b.Longitude - a.Longitude) < 1e-8);
		REQUIRE((b.Window != nullptr) == (a.Window != nullptr));
		if (a.Window) {
			windows++;
			CHECK(b.Window->HasEnter == a.Window->HasEnter);
			CHECK(b.Window->HasLeave == a.Window->HasLeave);
			CHECK(std::fabs(b.Window->Enter.Julian() - a.Window->Enter.Julian()) < 1e-8);
			CHECK(std::fabs(b.Window->Leave.Julian() - a.Window->Leave.Julian()) < 1e-8);
			REQUIRE(b.Window->Exacts.size() == a.Window->Exacts.size());
			for (size_t j = 0; j < a.Window->Exacts.size(); j++) {
				exacts++;
				CHECK(std::fabs(b.Window->Exacts[j].Julian() - a.Window->Exacts[j].Julian()) < 1e-8);
			}
		}
	}
	CHECK(windows > 0);
	CHECK(exacts > 0);
	// and the text is the same when written again
	CHECK(Analysis::EventsToText(back) == text);
}

TEST_CASE("Analysis events text: empty, comments, and what is wrong", "[Analysis][File]") {
	std::vector<AnalysisEvent> events{ AnalysisEvent{} };
	std::string error;
	REQUIRE(Analysis::EventsFromText("", events, error));
	CHECK(events.empty());
	REQUIRE(Analysis::EventsFromText("; a comment\n\n# another\r\n", events, error));
	CHECK(events.empty());
	CHECK(Analysis::EventsToText({}).empty());

	auto good = std::string("0,2460000.500000000,1,0,0,1,1,8,1,0,0,123.450000000\n");
	REQUIRE(Analysis::EventsFromText(good, events, error));
	REQUIRE(events.size() == 1);
	CHECK(events[0].Kind == AnalysisEventKind::Exact);
	CHECK(events[0].Aspect == AspectType::Sextile);
	CHECK(events[0].Longitude == Approx(123.45));
	CHECK(events[0].Window == nullptr);

	// a bad line changes nothing and is named
	events.clear();
	auto bad = [&](std::string const& text, std::string const& mentions) {
		std::vector<AnalysisEvent> kept{ AnalysisEvent{} };
		std::string message;
		CHECK_FALSE(Analysis::EventsFromText(text, kept, message));
		CHECK(kept.size() == 1);
		CHECK(message.find(mentions) != std::string::npos);
	};
	bad(good + "0,2460000.5,1,0,0,1,1,8,1,0,0\n", "line 2");					// too few fields
	bad("9,2460000.5,1,0,0,1,1,8,1,0,0,1\n", "type");
	bad("0,soon,1,0,0,1,1,8,1,0,0,1\n", "time");
	bad("0,2460000.5,99,0,0,1,1,8,1,0,0,1\n", "kind");
	bad("0,2460000.5,1,99,0,1,1,8,1,0,0,1\n", "mover");
	bad("0,2460000.5,1,0,7,1,1,8,1,0,0,1\n", "target");
	bad("0,2460000.5,1,0,0,1,40,8,1,0,0,1\n", "aspect");
	bad("0,2460000.5,1,0,0,1,1,x,1,0,0,1\n", "number");
	bad("0,2460000.5,1,0,0,1,1,8,1,0,0,1|1,2,3\n", "stay");
	bad("0,2460000.5,1,0,0,1,1,8,1,0,0,1|1,2460000.5,1,2460001.5,2,2460000.7\n", "stay");		// says two exacts, has one
}
