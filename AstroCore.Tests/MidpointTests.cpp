#include "TestCommon.h"
#include "Midpoints.h"
#include <algorithm>
#include <cmath>

namespace {
	PlanetPosition At(Planet planet, double longitude) {
		return PlanetPosition{ .Longitude = AstroPoint(longitude), .Speed = 0, .Latitude = 0, .LatitudeSpeed = 0, .Planet = planet };
	}

	ChartPoint Body(Planet planet, double longitude, double speed = 0) {
		return ChartPoint{ PointKind::Planet, planet, AstroPoint(longitude), speed };
	}

	// the planets up to Pluto for J2000 (noon UT, 1 Jan 2000) with houses for Greenwich
	ChartData J2000() {
		ChartData chart;
		chart.AddPlanets(std::vector<Planet>{ Planet::Sun, Planet::Moon, Planet::Mercury, Planet::Venus, Planet::Mars, Planet::Jupiter,
			Planet::Saturn, Planet::Uranus, Planet::Neptune, Planet::Pluto });
		chart.Info().Time = DateTime(2000, 1, 1, 12, 0, 0);
		chart.Info().Latitude = 51.5;
		chart.Info().Longitude = 0;
		chart.SetHouseSystem(HouseSystem::Placidus);
		AstroCalculator calc;
		calc.Calculate(chart);
		return chart;
	}
}

TEST_CASE("A midpoint is half way on the shorter arc", "[Midpoints]") {
	auto midpoints = Midpoints::Calculate(std::vector{ Body(Planet::Sun, 10, 1.0), Body(Planet::Moon, 50, 13.0) });
	REQUIRE(midpoints.size() == 1);
	auto const& m = midpoints[0];
	CHECK(m.Longitude.Value == Approx(30));
	CHECK(m.Arc == Approx(40));
	CHECK(m.Speed == Approx(7.0));
	CHECK(m.Opposite().Value == Approx(210));
	CHECK(m.A.Body == Planet::Sun);
	CHECK(m.B.Body == Planet::Moon);

	SECTION("through 0 Aries") {
		auto wrap = Midpoints::Calculate(std::vector{ Body(Planet::Sun, 350), Body(Planet::Moon, 10) });
		CHECK(wrap[0].Longitude.Value == Approx(0).margin(1e-9));
		CHECK(wrap[0].Arc == Approx(20));
		auto wide = Midpoints::Calculate(std::vector{ Body(Planet::Sun, 300), Body(Planet::Moon, 100) });
		// 160 degrees apart the short way, through 0: the midpoint is at 20, not at 200
		CHECK(wide[0].Longitude.Value == Approx(20));
		CHECK(wide[0].Arc == Approx(160));
	}
	SECTION("the order of the two makes no difference") {
		auto reverse = Midpoints::Calculate(std::vector{ Body(Planet::Moon, 50), Body(Planet::Sun, 10) });
		CHECK(reverse[0].Longitude.Value == Approx(30));
	}
}

TEST_CASE("Midpoints of a set of points: every pair once, by longitude", "[Midpoints]") {
	std::vector points{ Body(Planet::Sun, 100), Body(Planet::Moon, 20), Body(Planet::Mercury, 250), Body(Planet::Venus, 95) };
	auto midpoints = Midpoints::Calculate(points);
	CHECK(midpoints.size() == 6);		// 4 * 3 / 2
	CHECK(std::ranges::is_sorted(midpoints, {}, [](auto const& m) { return m.Longitude.Value; }));
	for (auto const& m : midpoints) {
		CHECK_FALSE(m.A.SameAs(m.B));
		// half the arc from each end
		CHECK(AstroPoint::Diff(m.Longitude, m.A.Longitude) == Approx(m.Arc / 2).margin(1e-9));
		CHECK(AstroPoint::Diff(m.Longitude, m.B.Longitude) == Approx(m.Arc / 2).margin(1e-9));
	}
	// Sun and Venus: 95 and 100
	auto it = std::ranges::find_if(midpoints, [](auto const& m) { return m.A.Body == Planet::Sun && m.B.Body == Planet::Venus; });
	REQUIRE(it != midpoints.end());
	CHECK(it->Longitude.Value == Approx(97.5));

	CHECK(Midpoints::Calculate(std::vector<ChartPoint>{}).empty());
	CHECK(Midpoints::Calculate(std::vector{ Body(Planet::Sun, 1) }).empty());
}

TEST_CASE("Midpoints of a chart", "[Midpoints]") {
	auto chart = J2000();

	SECTION("planets only") {
		auto points = Midpoints::Points(chart);
		REQUIRE(points.size() == 10);
		CHECK(points[0].Body == Planet::Sun);
		CHECK(points[0].Longitude.Value == Approx(chart.GetPlanet(0).Longitude.Value));
		CHECK(points[1].Speed == Approx(chart.GetPlanet(1).Speed));
		auto midpoints = Midpoints::Calculate(chart);
		CHECK(midpoints.size() == 45);
		// the Sun (280.4) and the Moon (223.3) at J2000: their midpoint is on the shorter arc
		auto sunMoon = std::ranges::find_if(midpoints, [](auto const& m) { return m.A.Body == Planet::Sun && m.B.Body == Planet::Moon; });
		REQUIRE(sunMoon != midpoints.end());
		CHECK(sunMoon->Longitude.Value == Approx(AstroPoint::MidPoint(chart.GetPlanet(0).Longitude, chart.GetPlanet(1).Longitude).Value));
	}
	SECTION("with the angles") {
		MidpointOptions options;
		options.Angles = true;
		auto points = Midpoints::Points(chart, options);
		REQUIRE(points.size() == 12);
		CHECK(points[10].Kind == PointKind::Ascendant);
		CHECK(points[10].Longitude.Value == Approx(chart.Houses().Asc.Value));
		CHECK(points[11].Kind == PointKind::Midheaven);
		CHECK(points[11].Longitude.Value == Approx(chart.Houses().MC.Value));
		CHECK(points[11].Speed == 0);
		CHECK(Midpoints::Calculate(chart, options).size() == 66);
	}
	SECTION("only some bodies") {
		MidpointOptions options;
		options.Only = { Planet::Sun, Planet::Moon, Planet::Saturn };
		options.Angles = true;
		auto points = Midpoints::Points(chart, options);
		REQUIRE(points.size() == 5);
		CHECK(points[2].Body == Planet::Saturn);
		auto midpoints = Midpoints::Calculate(chart, options);
		CHECK(midpoints.size() == 10);
		// the Ascendant/Midheaven midpoint is there, with the angles told apart from the planets
		auto angles = std::ranges::count_if(midpoints, [](auto const& m) {
			return m.A.Kind == PointKind::Ascendant && m.B.Kind == PointKind::Midheaven;
		});
		CHECK(angles == 1);
	}
}

TEST_CASE("Bodies and angles can be left out", "[Midpoints]") {
	auto chart = J2000();
	MidpointOptions options;
	options.Angles = true;

	SECTION("bodies") {
		options.Except = { Planet::Sun, Planet::Pluto };
		auto points = Midpoints::Points(chart, options);
		REQUIRE(points.size() == 10);		// 8 planets and the two angles
		for (auto const& point : points)
			CHECK_FALSE((point.Kind == PointKind::Planet && (point.Body == Planet::Sun || point.Body == Planet::Pluto)));
		CHECK(Midpoints::Calculate(chart, options).size() == 45);
		// what is left out of a list of the ones that take part is left out of it
		options.Only = { Planet::Sun, Planet::Moon, Planet::Mars };
		points = Midpoints::Points(chart, options);
		REQUIRE(points.size() == 4);
		CHECK(points[0].Body == Planet::Moon);
		CHECK(points[1].Body == Planet::Mars);
	}
	SECTION("one of the angles") {
		options.Midheaven = false;
		auto points = Midpoints::Points(chart, options);
		REQUIRE(points.size() == 11);
		CHECK(points.back().Kind == PointKind::Ascendant);
		options.Midheaven = true;
		options.Ascendant = false;
		points = Midpoints::Points(chart, options);
		REQUIRE(points.size() == 11);
		CHECK(points.back().Kind == PointKind::Midheaven);
		// with the angles off altogether, which of them is wanted doesn't matter
		options.Angles = false;
		options.Ascendant = true;
		CHECK(Midpoints::Points(chart, options).size() == 10);
	}
	SECTION("every body left out") {
		for (int i = 0; i < static_cast<int>(Planet::NumPlanets); i++)
			options.Except.push_back(static_cast<Planet>(i));
		auto points = Midpoints::Points(chart, options);
		REQUIRE(points.size() == 2);		// (only the angles are left)
		CHECK(Midpoints::Calculate(chart, options).size() == 1);
	}
}

TEST_CASE("Points are the same point by kind and body", "[Midpoints]") {
	CHECK(Body(Planet::Sun, 10).SameAs(Body(Planet::Sun, 200)));
	CHECK_FALSE(Body(Planet::Sun, 10).SameAs(Body(Planet::Moon, 10)));
	ChartPoint asc{ PointKind::Ascendant, Planet::Sun, AstroPoint(10), 0 };
	ChartPoint mc{ PointKind::Midheaven, Planet::Moon, AstroPoint(10), 0 };
	CHECK_FALSE(asc.SameAs(Body(Planet::Sun, 10)));	// the angle is not the Sun
	CHECK_FALSE(asc.SameAs(mc));
	CHECK(mc.SameAs(ChartPoint{ PointKind::Midheaven, Planet::Sun, AstroPoint(99), 0 }));	// (the body means nothing for an angle)
}

TEST_CASE("Midpoints between two charts", "[Midpoints]") {
	std::vector a{ Body(Planet::Sun, 10), Body(Planet::Moon, 100) };
	std::vector b{ Body(Planet::Sun, 50), Body(Planet::Venus, 340), Body(Planet::Mars, 200) };
	auto midpoints = Midpoints::CalcBetween(a, b);
	CHECK(midpoints.size() == 6);		// each with each, the same planet in both included
	CHECK(std::ranges::is_sorted(midpoints, {}, [](auto const& m) { return m.Longitude.Value; }));
	for (auto const& m : midpoints) {
		CHECK((m.A.Longitude.Value == 10 || m.A.Longitude.Value == 100));
		CHECK((m.B.Longitude.Value == 50 || m.B.Longitude.Value == 340 || m.B.Longitude.Value == 200));
	}
	auto sunSun = std::ranges::find_if(midpoints, [](auto const& m) { return m.A.Body == Planet::Sun && m.B.Body == Planet::Sun; });
	REQUIRE(sunSun != midpoints.end());
	CHECK(sunSun->Longitude.Value == Approx(30));
	// Sun 10 with Venus 340: through 0
	auto sunVenus = std::ranges::find_if(midpoints, [](auto const& m) { return m.A.Body == Planet::Sun && m.B.Body == Planet::Venus; });
	CHECK(sunVenus->Longitude.Value == Approx(355));

	CHECK(Midpoints::CalcBetween(a, {}).empty());
}

TEST_CASE("The 90 degree dial", "[Midpoints]") {
	CHECK(Midpoints::OnDial(0) == Approx(0));
	CHECK(Midpoints::OnDial(100) == Approx(10));
	CHECK(Midpoints::OnDial(190) == Approx(10));		// opposition and square fall together
	CHECK(Midpoints::OnDial(359) == Approx(89));
	CHECK(Midpoints::OnDial(-10) == Approx(80));
	CHECK(Midpoints::OnDial(100, 2) == Approx(100));	// the 180 degree dial: conjunction and opposition
	CHECK(Midpoints::OnDial(280, 2) == Approx(100));
	CHECK(Midpoints::OnDial(100, 1) == Approx(100));
	CHECK(Midpoints::OnDial(100, 0) == Approx(100));		// (nonsense divisions mean the whole circle)
}

TEST_CASE("How far a point is from a midpoint", "[Midpoints]") {
	int angle = -1;
	CHECK(Midpoints::ContactOrb(30, 30, ContactKind::Axis, &angle) == Approx(0));
	CHECK(angle == 0);
	CHECK(Midpoints::ContactOrb(31.2, 30, ContactKind::Axis, &angle) == Approx(1.2));
	CHECK(angle == 0);
	CHECK(Midpoints::ContactOrb(209, 30, ContactKind::Axis, &angle) == Approx(1));
	CHECK(angle == 180);
	// 45 degrees away is nothing on the axis, but is exact on the dial
	CHECK(Midpoints::ContactOrb(75, 30, ContactKind::Axis) == Approx(45));
	CHECK(Midpoints::ContactOrb(75, 30, ContactKind::Dial90, &angle) == Approx(0));
	CHECK(angle == 45);
	CHECK(Midpoints::ContactOrb(121, 30, ContactKind::Dial90, &angle) == Approx(1));
	CHECK(angle == 90);
	CHECK(Midpoints::ContactOrb(164, 30, ContactKind::Dial90, &angle) == Approx(1));
	CHECK(angle == 135);
	// the 45 degree dial counts the same angles
	CHECK(Midpoints::ContactOrb(75, 30, ContactKind::Dial45, &angle) == Approx(0));
	CHECK(angle == 45);
	CHECK(Midpoints::ContactOrb(121, 30, ContactKind::Dial45, &angle) == Approx(1));
	CHECK(angle == 90);
	// through 0 Aries, and on either side
	CHECK(Midpoints::ContactOrb(359, 1, ContactKind::Axis, &angle) == Approx(2));
	CHECK(angle == 0);
	CHECK(Midpoints::ContactOrb(1, 359, ContactKind::Axis) == Approx(2));
	CHECK(Midpoints::ContactOrb(-1, 179, ContactKind::Axis, &angle) == Approx(0));		// (a negative longitude is 359)
	CHECK(angle == 180);
}

TEST_CASE("Planets on midpoints", "[Midpoints]") {
	// Sun 10 and Moon 50: the midpoint is 30 (and 210)
	auto midpoints = Midpoints::Calculate(std::vector{ Body(Planet::Sun, 10), Body(Planet::Moon, 50) });
	auto contactsOf = [&](double longitude, ContactOptions const& options = {}) {
		return Midpoints::Contacts(midpoints, Body(Planet::Mars, longitude), options);
	};

	SECTION("on it, opposite it, near it, too far") {
		auto exact = contactsOf(30);
		REQUIRE(exact.size() == 1);
		CHECK(exact[0].Midpoint == 0);
		CHECK(exact[0].Point.Body == Planet::Mars);
		CHECK(exact[0].Angle == 0);
		CHECK(exact[0].Orb == Approx(0));

		auto opposite = contactsOf(211);
		REQUIRE(opposite.size() == 1);
		CHECK(opposite[0].Angle == 180);
		CHECK(opposite[0].Orb == Approx(1));

		CHECK(contactsOf(31.4).size() == 1);
		CHECK(contactsOf(31.6).empty());
		CHECK(contactsOf(120).empty());
		CHECK(contactsOf(75).empty());		// 45 away counts only on the dial
	}
	SECTION("the orb is the caller's") {
		ContactOptions tight;
		tight.Orb = 0.5;
		CHECK(contactsOf(30.4, tight).size() == 1);
		CHECK(contactsOf(30.6, tight).empty());
		ContactOptions wide;
		wide.Orb = 5;
		CHECK(contactsOf(34, wide).size() == 1);
	}
	SECTION("the 90 degree dial") {
		ContactOptions dial;
		dial.Kind = ContactKind::Dial90;
		for (auto [longitude, angle] : std::vector<std::pair<double, int>>{ { 30, 0 }, { 75, 45 }, { 120, 90 }, { 165, 135 }, { 210, 180 }, { 255, 135 } }) {
			INFO("longitude " << longitude);
			auto contacts = contactsOf(longitude, dial);
			REQUIRE(contacts.size() == 1);
			CHECK(contacts[0].Angle == angle);
			CHECK(contacts[0].Orb == Approx(0).margin(1e-9));
		}
		CHECK(contactsOf(100, dial).empty());
	}
	SECTION("the two ends are not contacts of their own midpoint") {
		// on the dial each end of a 90 degree arc would be at 45 from the midpoint
		auto quarter = Midpoints::Calculate(std::vector{ Body(Planet::Sun, 0), Body(Planet::Moon, 90) });
		ContactOptions dial;
		dial.Kind = ContactKind::Dial90;
		CHECK(Midpoints::Contacts(quarter, std::vector{ Body(Planet::Sun, 0), Body(Planet::Moon, 90) }, dial).empty());
		dial.ExcludeMembers = false;
		CHECK(Midpoints::Contacts(quarter, std::vector{ Body(Planet::Sun, 0), Body(Planet::Moon, 90) }, dial).size() == 2);
	}
	SECTION("tightest first") {
		midpoints = Midpoints::Calculate(std::vector{ Body(Planet::Sun, 10), Body(Planet::Moon, 50), Body(Planet::Mercury, 12), Body(Planet::Venus, 52) });
		// Sun/Venus and Moon/Mercury are both at 31, Sun/Moon at 30 and Mercury/Venus at 32
		std::vector points{ Body(Planet::Mars, 31.0) };
		auto contacts = Midpoints::Contacts(midpoints, points, ContactOptions{ .Orb = 1.5 });
		REQUIRE(contacts.size() == 4);
		for (size_t i = 1; i < contacts.size(); i++)
			CHECK(contacts[i - 1].Orb <= contacts[i].Orb);
		CHECK(contacts[0].Orb == Approx(0).margin(1e-9));
		CHECK(contacts[1].Orb == Approx(0).margin(1e-9));
		CHECK(contacts[2].Orb == Approx(1.0));
		CHECK(contacts[3].Orb == Approx(1.0));
	}
	SECTION("several points at once, an angle among them") {
		std::vector points{ Body(Planet::Mars, 30), ChartPoint{ PointKind::Ascendant, Planet::Sun, AstroPoint(210.5), 0 }, Body(Planet::Jupiter, 100) };
		auto contacts = Midpoints::Contacts(midpoints, points);
		REQUIRE(contacts.size() == 2);
		CHECK(contacts[0].Point.Body == Planet::Mars);
		CHECK(contacts[1].Point.Kind == PointKind::Ascendant);
		CHECK(contacts[1].Angle == 180);
	}
}

TEST_CASE("A midpoint tree on the 45 degree dial", "[Midpoints]") {
	CHECK(Midpoints::DialDivisions(ContactKind::Dial45) == 8);
	CHECK(Midpoints::DialDivisions(ContactKind::Dial90) == 4);
	CHECK(Midpoints::DialDivisions(ContactKind::Axis) == 4);

	// the Sun and the Moon have their midpoint at 30; Mars is on it, Venus 45 degrees from it and Jupiter 90 degrees
	auto midpoints = Midpoints::Calculate(std::vector{ Body(Planet::Sun, 10), Body(Planet::Moon, 50) });
	std::vector points{ Body(Planet::Mars, 30), Body(Planet::Venus, 75), Body(Planet::Jupiter, 120) };
	ContactOptions options;
	options.Kind = ContactKind::Dial90;
	auto tree90 = Midpoints::Tree(midpoints, points, options);
	REQUIRE(tree90.size() == 3);
	// on the 90 degree dial the semi-square is somewhere else
	auto dialOf = [](std::vector<MidpointBranch> const& tree, Planet planet) {
		auto it = std::ranges::find_if(tree, [&](auto const& b) { return b.Point.Body == planet; });
		REQUIRE(it != tree.end());
		return it->Dial;
	};
	CHECK(dialOf(tree90, Planet::Mars) == Approx(30));
	CHECK(dialOf(tree90, Planet::Venus) == Approx(75));
	CHECK(dialOf(tree90, Planet::Jupiter) == Approx(30));
	// on the 45 degree dial they all stand together
	options.Kind = ContactKind::Dial45;
	auto tree45 = Midpoints::Tree(midpoints, points, options);
	REQUIRE(tree45.size() == 3);
	for (auto planet : { Planet::Mars, Planet::Venus, Planet::Jupiter })
		CHECK(dialOf(tree45, planet) == Approx(30));
	// (and they are the same contacts)
	for (size_t i = 0; i < 3; i++)
		CHECK(Midpoints::Contacts(midpoints, points[i], options).size() == Midpoints::Contacts(midpoints, points[i], ContactOptions{ .Kind = ContactKind::Dial90 }).size());
}

TEST_CASE("A chart's midpoint tree", "[Midpoints]") {
	auto chart = J2000();
	auto points = Midpoints::Points(chart);
	auto midpoints = Midpoints::Calculate(points);

	// every contact is really within the orb, and no planet is on the midpoint of a pair it belongs to
	ContactOptions options;
	options.Kind = ContactKind::Dial90;
	options.Orb = 2;
	auto contacts = Midpoints::Contacts(midpoints, points, options);
	for (auto const& c : contacts) {
		auto const& m = midpoints[c.Midpoint];
		CHECK(c.Orb <= 2 + 1e-9);
		CHECK_FALSE(c.Point.SameAs(m.A));
		CHECK_FALSE(c.Point.SameAs(m.B));
		// the contact's orb is the distance from the dial angle that it names
		CHECK(std::fabs(AstroPoint::Diff(c.Point.Longitude, m.Longitude) - c.Angle) == Approx(c.Orb).margin(1e-9));
	}
	// the whole list is what the points give one at a time
	size_t total = 0;
	for (auto const& point : points)
		total += Midpoints::Contacts(midpoints, point, options).size();
	CHECK(total == contacts.size());
	// (45 midpoints and 8 other planets each on a dial that is 4 degrees in 45 covered: there are always some)
	CHECK_FALSE(contacts.empty());
}

TEST_CASE("A midpoint tree", "[Midpoints]") {
	// Sun 10, Moon 50 (midpoint 30), Mercury 15, Venus 45 (midpoint 30 too); Mars at 30 sits on both; Jupiter at 75 is 45 from them
	std::vector points{ Body(Planet::Sun, 10), Body(Planet::Moon, 50), Body(Planet::Mercury, 15), Body(Planet::Venus, 45),
		Body(Planet::Mars, 30), Body(Planet::Jupiter, 75.5) };
	auto midpoints = Midpoints::Calculate(points);
	ContactOptions options;
	options.Kind = ContactKind::Dial90;
	options.Orb = 1;
	auto tree = Midpoints::Tree(midpoints, points, options);

	auto branchOf = [&](Planet planet) -> MidpointBranch const* {
		auto it = std::ranges::find_if(tree, [&](auto const& b) { return b.Point.Body == planet; });
		return it == tree.end() ? nullptr : &*it;
	};
	SECTION("a branch for each point with a midpoint on it") {
		auto mars = branchOf(Planet::Mars);
		REQUIRE(mars != nullptr);
		// Sun/Moon and Mercury/Venus both at 30, exactly
		int exact = 0;
		for (auto const& contact : mars->Contacts) {
			CHECK(contact.Point.Body == Planet::Mars);
			CHECK(contact.Orb <= 1 + 1e-9);
			auto const& m = midpoints[contact.Midpoint];
			if (contact.Angle == 0 && contact.Orb < 1e-9) {
				exact++;
				CHECK(m.Longitude.Value == Approx(30));
			}
		}
		CHECK(exact == 2);
		CHECK(mars->Dial == Approx(30));
	}
	SECTION("the branches follow the dial, and the contacts are the tightest first") {
		REQUIRE(tree.size() >= 2);
		for (size_t i = 1; i < tree.size(); i++)
			CHECK(tree[i - 1].Dial <= tree[i].Dial);
		for (auto const& branch : tree)
			for (size_t i = 1; i < branch.Contacts.size(); i++)
				CHECK(branch.Contacts[i - 1].Orb <= branch.Contacts[i].Orb);
	}
	SECTION("a point is not on a midpoint it is an end of") {
		for (auto const& branch : tree)
			for (auto const& contact : branch.Contacts) {
				auto const& m = midpoints[contact.Midpoint];
				CHECK_FALSE(branch.Point.SameAs(m.A));
				CHECK_FALSE(branch.Point.SameAs(m.B));
			}
	}
	SECTION("the tree holds the contacts of the whole chart") {
		size_t total = 0;
		for (auto const& branch : tree)
			total += branch.Contacts.size();
		CHECK(total == Midpoints::Contacts(midpoints, points, options).size());
	}
	SECTION("points with nothing on them are left out") {
		ContactOptions tight = options;
		tight.Orb = 0.01;
		auto strict = Midpoints::Tree(midpoints, points, tight);
		CHECK(strict.size() < tree.size());
		for (auto const& branch : strict)
			CHECK_FALSE(branch.Contacts.empty());
		CHECK(Midpoints::Tree({}, points, options).empty());
		CHECK(Midpoints::Tree(midpoints, {}, options).empty());
	}
}

TEST_CASE("Midpoints between a chart and the planets around it", "[Midpoints]") {
	// the chart: Sun 100, Moon 50; the overlay: Sun 10, Mars 200
	ChartData chart, overlay;
	chart.AddPlanets({ At(Planet::Sun, 100), At(Planet::Moon, 50) });
	overlay.AddPlanets({ At(Planet::Sun, 10), At(Planet::Mars, 200) });
	auto own = Midpoints::Points(chart, {}, 0);
	auto around = Midpoints::Points(overlay, {}, 1);
	for (auto const& p : own)
		CHECK(p.Set == 0);
	for (auto const& p : around)
		CHECK(p.Set == 1);
	CHECK_FALSE(own[0].SameAs(around[0]));		// (the two Suns are not the same point)
	CHECK(own[0].SameAs(own[0]));

	// the overlay's Sun with the chart's Moon: 10 and 50, midpoint 30
	auto between = Midpoints::CalcBetween(around, own);
	REQUIRE(between.size() == 4);
	auto it = std::ranges::find_if(between, [](auto const& m) { return m.A.Body == Planet::Sun && m.B.Body == Planet::Moon; });
	REQUIRE(it != between.end());
	CHECK(it->Longitude.Value == Approx(30));
	CHECK(it->A.Set == 1);
	CHECK(it->B.Set == 0);

	// the points of both on it: the chart's Sun at 30 stands on it - its ends (the overlay's Sun and the chart's Moon) do not, though
	// the chart's Sun is a Sun and the overlay's Moon-less pair has a Sun as one end
	std::vector<ChartPoint> all = own;
	all.insert(all.end(), around.begin(), around.end());
	all[0].Longitude = AstroPoint(30);		// the chart's Sun on the midpoint
	auto contacts = Midpoints::Contacts(between, all);
	bool sunOnIt = false;
	for (auto const& c : contacts) {
		if (c.Midpoint != static_cast<size_t>(it - between.begin()))
			continue;
		CHECK_FALSE(c.Point.SameAs(it->A));
		CHECK_FALSE(c.Point.SameAs(it->B));
		if (c.Point.Set == 0 && c.Point.Body == Planet::Sun && c.Orb < 1e-9)
			sunOnIt = true;
	}
	CHECK(sunOnIt);		// (not excluded as an end: the end is the overlay's Sun)
}
