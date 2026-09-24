#include "TestCommon.h"
#include "AstroCalculator.h"
#include <cmath>

namespace {
	double At(double jd) {
		return jd;
	}

	double Jd(int y, int m, int d, int hour = 0, int minute = 0) {
		return DateTime(y, m, d, hour, minute, 0).Julian();
	}

	double Wrap180(double angle) {
		angle = fmod(angle, 360);
		if (angle > 180)
			angle -= 360;
		else if (angle <= -180)
			angle += 360;
		return angle;
	}

	double Lon(AstroCalculator const& calc, Planet planet, double jd) {
		return calc.CalcPlanet(planet, DateTime(jd, true), 1, false).Longitude.Value;
	}
}

TEST_CASE("Eclipses of 2024", "[Eclipses]") {
	AstroCalculator calc;
	auto eclipses = calc.CalcEclipses(DateTime(2024, 1, 1), DateTime(2025, 1, 1));

	// 25 Mar penumbral lunar 07:12 UT, 8 Apr total solar 18:17, 18 Sep partial lunar 02:44, 2 Oct annular solar 18:46
	REQUIRE(eclipses.size() == 4);

	CHECK_FALSE(eclipses[0].Solar);
	CHECK(eclipses[0].Kind == EclipseKind::Penumbral);
	CHECK(eclipses[0].Maximum.Julian() == Approx(Jd(2024, 3, 25, 7, 12)).margin(0.02));

	CHECK(eclipses[1].Solar);
	CHECK(eclipses[1].Kind == EclipseKind::Total);
	CHECK(eclipses[1].Maximum.Julian() == Approx(Jd(2024, 4, 8, 18, 17)).margin(0.02));

	CHECK_FALSE(eclipses[2].Solar);
	CHECK(eclipses[2].Kind == EclipseKind::Partial);
	CHECK(eclipses[2].Maximum.Julian() == Approx(Jd(2024, 9, 18, 2, 44)).margin(0.02));

	CHECK(eclipses[3].Solar);
	CHECK(eclipses[3].Kind == EclipseKind::Annular);
	CHECK(eclipses[3].Maximum.Julian() == Approx(Jd(2024, 10, 2, 18, 46)).margin(0.02));
}

TEST_CASE("Other kinds of eclipse", "[Eclipses]") {
	AstroCalculator calc;

	// 20 April 2023: a hybrid solar eclipse, 04:16 UT
	auto hybrid = calc.CalcEclipses(DateTime(2023, 4, 1), DateTime(2023, 4, 30));
	REQUIRE(hybrid.size() == 1);
	CHECK(hybrid[0].Solar);
	CHECK(hybrid[0].Kind == EclipseKind::Hybrid);
	CHECK(hybrid[0].Maximum.Julian() == Approx(Jd(2023, 4, 20, 4, 16)).margin(0.02));

	// 8 November 2022: a total lunar eclipse, about 11:00 UT
	auto total = calc.CalcEclipses(DateTime(2022, 11, 1), DateTime(2022, 11, 15));
	REQUIRE(total.size() == 1);
	CHECK_FALSE(total[0].Solar);
	CHECK(total[0].Kind == EclipseKind::Total);
	CHECK(total[0].Maximum.Julian() == Approx(Jd(2022, 11, 8, 11, 0)).margin(0.02));
}

TEST_CASE("A range without eclipses", "[Eclipses]") {
	AstroCalculator calc;
	// between the April and the September eclipse of 2024
	CHECK(calc.CalcEclipses(DateTime(2024, 5, 1), DateTime(2024, 8, 31)).empty());
	CHECK(calc.CalcEclipses(DateTime(2024, 5, 1), DateTime(2024, 5, 1)).empty());
}

TEST_CASE("Eclipses come in time order and only from the range", "[Eclipses]") {
	AstroCalculator calc;
	auto from = DateTime(2020, 1, 1), to = DateTime(2030, 1, 1);
	auto eclipses = calc.CalcEclipses(from, to);
	// a bit over four a year
	CHECK(eclipses.size() > 35);
	CHECK(eclipses.size() < 50);
	for (size_t i = 0; i < eclipses.size(); i++) {
		CHECK(eclipses[i].Maximum.Julian() >= from.Julian());
		CHECK(eclipses[i].Maximum.Julian() < to.Julian());
		if (i > 0)
			CHECK(eclipses[i].Maximum.Julian() >= eclipses[i - 1].Maximum.Julian());
	}
}

TEST_CASE("Every eclipse is at a new or full moon", "[Eclipses]") {
	AstroCalculator calc;
	for (auto const& eclipse : calc.CalcEclipses(DateTime(2024, 1, 1), DateTime(2027, 1, 1))) {
		double sun = Lon(calc, Planet::Sun, eclipse.Maximum.Julian());
		double moon = Lon(calc, Planet::Moon, eclipse.Maximum.Julian());
		double elongation = fabs(Wrap180(moon - sun));
		INFO("eclipse at JD " << eclipse.Maximum.Julian());
		if (eclipse.Solar)
			CHECK(elongation < 2);
		else
			CHECK(elongation > 178);
	}
}

TEST_CASE("Void of course periods are consistent", "[VoidOfCourse]") {
	AstroCalculator calc;
	const double from = Jd(2026, 9, 1), to = Jd(2026, 10, 1);
	auto periods = calc.CalcVoidOfCourse(DateTime(from, true), DateTime(to, true));

	// the Moon changes sign about every two and a half days
	REQUIRE(periods.size() >= 11);
	CHECK(periods.size() <= 15);
	CHECK(periods.front().End.Julian() > from);
	CHECK(periods.front().Start.Julian() < to);
	CHECK(periods.back().End.Julian() >= to);

	for (size_t i = 0; i < periods.size(); i++) {
		auto const& p = periods[i];
		INFO("period " << i << " ends JD " << p.End.Julian());
		CHECK(p.Start.Julian() <= p.End.Julian());
		if (i > 0) {
			// one sign after the other, the next starting when the last one ends or later
			CHECK(p.Start.Julian() >= periods[i - 1].End.Julian() - 1e-6);
			CHECK(p.Sign == static_cast<ZodiacSign>((static_cast<int>(periods[i - 1].Sign) + 1) % 12));
		}
		// it ends when the Moon reaches the start of a sign
		double end = Lon(calc, Planet::Moon, p.End.Julian());
		double inSign = fmod(end, 30);
		CHECK(std::min(inSign, 30 - inSign) < 0.01);
		// and lasts less than the Moon's stay in a sign
		CHECK(p.End.Julian() - p.Start.Julian() < 3.5);
	}
}

TEST_CASE("The last aspect is exact where the void starts", "[VoidOfCourse]") {
	AstroCalculator calc;
	auto periods = calc.CalcVoidOfCourse(DateTime(2026, 9, 1), DateTime(2026, 11, 1));
	int checked = 0;
	for (auto const& p : periods) {
		if (p.WholeSign)
			continue;
		double jd = p.Start.Julian();
		double angle = AstroPoint::Diff(Lon(calc, Planet::Moon, jd), Lon(calc, p.LastPlanet, jd));
		INFO("void starts JD " << jd << " with planet " << (int)p.LastPlanet);
		CHECK(fabs(angle - p.LastAngle) < 0.02);
		CHECK((p.LastAngle == 0 || p.LastAngle == 60 || p.LastAngle == 90 || p.LastAngle == 120 || p.LastAngle == 180));
		checked++;
	}
	CHECK(checked > 15);
}

TEST_CASE("The Moon makes no aspect during a void", "[VoidOfCourse]") {
	AstroCalculator calc;
	static constexpr Planet bodies[] = { Planet::Sun, Planet::Mercury, Planet::Venus, Planet::Mars, Planet::Jupiter, Planet::Saturn,
		Planet::Uranus, Planet::Neptune, Planet::Pluto };
	static constexpr double angles[] = { 0, 60, -60, 90, -90, 120, -120, 180 };

	auto periods = calc.CalcVoidOfCourse(DateTime(2026, 9, 1), DateTime(2026, 9, 20));
	for (auto const& p : periods) {
		// from a minute after the void starts to a minute before it ends, on a fine grid
		const double step = 1.0 / 48;
		for (double t = p.Start.Julian() + 1.0 / 1440; t + step < p.End.Julian() - 1.0 / 1440; t += step) {
			double moon0 = Lon(calc, Planet::Moon, t), moon1 = Lon(calc, Planet::Moon, t + step);
			for (auto body : bodies) {
				double planet0 = Lon(calc, body, t), planet1 = Lon(calc, body, t + step);
				for (double angle : angles) {
					double before = Wrap180(moon0 - planet0 - angle), after = Wrap180(moon1 - planet1 - angle);
					bool crossed = before < 0 && after >= 0 && after - before < 90;
					INFO("JD " << t << " planet " << (int)body << " angle " << angle);
					REQUIRE_FALSE(crossed);
				}
			}
		}
	}
}

TEST_CASE("Only aspects to inner planets can be used", "[VoidOfCourse]") {
	AstroCalculator calc;
	// leaving out Uranus, Neptune and Pluto can only make voids start earlier and last longer
	auto all = calc.CalcVoidOfCourse(DateTime(2026, 9, 1), DateTime(2026, 10, 1), true);
	auto inner = calc.CalcVoidOfCourse(DateTime(2026, 9, 1), DateTime(2026, 10, 1), false);
	REQUIRE(all.size() == inner.size());
	for (size_t i = 0; i < all.size(); i++) {
		CHECK(inner[i].End.Julian() == Approx(all[i].End.Julian()).margin(1e-6));
		CHECK(inner[i].Start.Julian() <= all[i].Start.Julian() + 1e-6);
	}
}
