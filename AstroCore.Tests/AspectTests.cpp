#include "TestCommon.h"
#include "Aspects.h"

namespace {
	PlanetPosition At(Planet planet, double longitude, double speed = 0) {
		return PlanetPosition{ .Longitude = AstroPoint(longitude), .Speed = speed, .Latitude = 0, .LatitudeSpeed = 0, .Planet = planet };
	}
}

TEST_CASE("Aspect angles", "[Aspects]") {
	CHECK(AspectCalculator::GetAspectAngle(AspectType::Conjunction) == Approx(0));
	CHECK(AspectCalculator::GetAspectAngle(AspectType::Sextile) == Approx(60));
	CHECK(AspectCalculator::GetAspectAngle(AspectType::Square) == Approx(90));
	CHECK(AspectCalculator::GetAspectAngle(AspectType::Trine) == Approx(120));
	CHECK(AspectCalculator::GetAspectAngle(AspectType::Opposition) == Approx(180));
	CHECK(AspectCalculator::GetAspectAngle(AspectType::SemiSextile) == Approx(30));
	CHECK(AspectCalculator::GetAspectAngle(AspectType::SemiSquare) == Approx(45));
	CHECK(AspectCalculator::GetAspectAngle(AspectType::Quintile) == Approx(72));
	CHECK(AspectCalculator::GetAspectAngle(AspectType::BiQuintile) == Approx(144));
	CHECK(AspectCalculator::GetAspectAngle(AspectType::Septile) == Approx(360.0 / 7));
	CHECK(AspectCalculator::GetAspectAngle(AspectType::BiSeptile) == Approx(720.0 / 7));
	CHECK(AspectCalculator::GetAspectAngle(AspectType::Quincunx) == Approx(150));
	CHECK(AspectCalculator::GetAspectAngle(AspectType::SesquiQuadrate) == Approx(135));
	CHECK(AspectCalculator::GetAspectAngle(AspectType::Novile) == Approx(40));
	CHECK(AspectCalculator::GetAspectAngle(AspectType::BiNovile) == Approx(80));
}

TEST_CASE("The five major aspects", "[Aspects]") {
	AspectCalculator calc;
	struct { double second; AspectType type; } cases[] = {
		{ 2, AspectType::Conjunction },
		{ 60, AspectType::Sextile },
		{ 90, AspectType::Square },
		{ 120, AspectType::Trine },
		{ 180, AspectType::Opposition },
	};
	for (auto& c : cases) {
		auto aspect = calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, c.second));
		INFO("angle " << c.second);
		CHECK(aspect.Type == c.type);
	}
}

TEST_CASE("The angle and the orb of an aspect", "[Aspects]") {
	AspectCalculator calc;
	auto aspect = calc.CalcAspect(At(Planet::Mars, 10), At(Planet::Jupiter, 133));
	CHECK(aspect.Type == AspectType::Trine);
	CHECK(aspect.Angle == Approx(123));
	CHECK(aspect.Orb == Approx(3));
	CHECK(aspect.IsMajor());
	CHECK(aspect.IsSoft());
	CHECK_FALSE(aspect.IsHard());
}

TEST_CASE("Aspects are found the short way round zero Aries", "[Aspects]") {
	AspectCalculator calc;
	// 355 and 5 are 10 degrees apart, 358 and 2 only 4
	CHECK(calc.CalcAspect(At(Planet::Mars, 358), At(Planet::Jupiter, 2)).Type == AspectType::Conjunction);
	// 350 and 110 are 120 degrees apart across zero
	CHECK(calc.CalcAspect(At(Planet::Mars, 350), At(Planet::Jupiter, 110)).Type == AspectType::Trine);
	// the order of the two planets doesn't matter
	CHECK(calc.CalcAspect(At(Planet::Jupiter, 110), At(Planet::Mars, 350)).Type == AspectType::Trine);
}

TEST_CASE("The orb limit", "[Aspects]") {
	AspectCalculator calc;		// major aspects: 8 degrees
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 128)).Type == AspectType::Trine);
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 112)).Type == AspectType::Trine);
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 128.5)).Type == AspectType::None);
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 111.5)).Type == AspectType::None);

	AspectSettings tight;
	tight.MajorAspectOrb = 3;
	AspectCalculator tightCalc(tight);
	CHECK(tightCalc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 122.5)).Type == AspectType::Trine);
	CHECK(tightCalc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 124)).Type == AspectType::None);
}

TEST_CASE("No aspect", "[Aspects]") {
	AspectCalculator calc;
	// 100 degrees is more than 8 from both 90 and 120, and far from every minor aspect
	auto aspect = calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 100));
	CHECK(aspect.Type == AspectType::None);
	CHECK(aspect.Angle == Approx(100));
}

TEST_CASE("Minor aspects and their orb", "[Aspects]") {
	AspectCalculator calc;		// minor aspects: 2 degrees
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 31)).Type == AspectType::SemiSextile);
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 45)).Type == AspectType::SemiSquare);
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 150)).Type == AspectType::Quincunx);
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 135)).Type == AspectType::SesquiQuadrate);
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 72)).Type == AspectType::Quintile);
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 144)).Type == AspectType::BiQuintile);
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 40)).Type == AspectType::Novile);
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 80)).Type == AspectType::BiNovile);
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 34)).Type == AspectType::None);

	auto semi = calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 45));
	CHECK_FALSE(semi.IsMajor());
	CHECK(semi.IsHard());
}

TEST_CASE("Major aspects only", "[Aspects]") {
	AspectSettings settings;
	settings.MajorOnly = true;
	AspectCalculator calc(settings);
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 45)).Type == AspectType::None);
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 150)).Type == AspectType::None);
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 90)).Type == AspectType::Square);
}

TEST_CASE("Extra orb for the conjunction", "[Aspects]") {
	AspectSettings settings;
	settings.ConjunctionOrbAdd = 2;		// 10 degrees for a conjunction
	AspectCalculator calc(settings);
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 9)).Type == AspectType::Conjunction);
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 11)).Type == AspectType::None);
	// only the conjunction gets it
	CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Jupiter, 129)).Type == AspectType::None);
}

TEST_CASE("Extra orb for the Sun and the Moon", "[Aspects]") {
	AspectSettings settings;
	settings.SunPlanetOrbAdd = 2;
	settings.MoonPlanetOrbAdd = 1;
	settings.SunMoonOrbAdd = 3;
	AspectCalculator calc(settings);

	SECTION("Sun with a planet") {
		CHECK(calc.CalcAspect(At(Planet::Sun, 0), At(Planet::Mars, 9.5)).Type == AspectType::Conjunction);
		CHECK(calc.CalcAspect(At(Planet::Mars, 0), At(Planet::Mars, 9.5)).Type == AspectType::None);
	}
	SECTION("Moon with a planet") {
		CHECK(calc.CalcAspect(At(Planet::Moon, 0), At(Planet::Mars, 8.5)).Type == AspectType::Conjunction);
		CHECK(calc.CalcAspect(At(Planet::Moon, 0), At(Planet::Mars, 9.5)).Type == AspectType::None);
	}
	SECTION("Sun with the Moon") {
		CHECK(calc.CalcAspect(At(Planet::Sun, 0), At(Planet::Moon, 10.5)).Type == AspectType::Conjunction);
		CHECK(calc.CalcAspect(At(Planet::Sun, 0), At(Planet::Moon, 11.5)).Type == AspectType::None);
	}
}

TEST_CASE("Applying and separating", "[Aspects]") {
	// Mars at 0 and Jupiter at 118: two degrees short of an exact trine
	auto mars = At(Planet::Mars, 0, 0);
	auto jupiter = At(Planet::Jupiter, 118, 0);

	SECTION("the gap closes") {
		mars.Speed = -1;		// moving backwards, away from Jupiter: the angle grows towards 120
		CHECK(AspectCalculator::IsApplying(mars, jupiter, 120, 120));
		AspectCalculator calc;
		CHECK(calc.CalcAspect(mars, jupiter).Applying);
	}
	SECTION("the gap widens") {
		mars.Speed = 1;
		CHECK_FALSE(AspectCalculator::IsApplying(mars, jupiter, 120, 120));
		AspectCalculator calc;
		CHECK_FALSE(calc.CalcAspect(mars, jupiter).Applying);
	}
	SECTION("past the exact angle") {
		auto past = At(Planet::Jupiter, 122, 0);
		mars.Speed = -1;
		CHECK_FALSE(AspectCalculator::IsApplying(mars, past, 120, 120));
		mars.Speed = 1;
		CHECK(AspectCalculator::IsApplying(mars, past, 120, 120));
	}
}

TEST_CASE("All aspects among a set of planets", "[Aspects]") {
	AspectCalculator calc;

	SECTION("a grand trine") {
		auto aspects = calc.Calculate({ At(Planet::Sun, 0), At(Planet::Moon, 120), At(Planet::Mars, 240) });
		REQUIRE(aspects.size() == 3);
		for (auto& a : aspects)
			CHECK(a.Type == AspectType::Trine);
	}
	SECTION("nothing in common") {
		CHECK(calc.Calculate({ At(Planet::Mars, 0), At(Planet::Jupiter, 100) }).empty());
	}
	SECTION("one planet or none") {
		CHECK(calc.Calculate({}).empty());
		CHECK(calc.Calculate({ At(Planet::Mars, 0) }).empty());
	}
	SECTION("each pair once") {
		// four planets at the same spot: 6 pairs
		auto aspects = calc.Calculate({ At(Planet::Mars, 10), At(Planet::Jupiter, 10), At(Planet::Saturn, 10), At(Planet::Uranus, 10) });
		CHECK(aspects.size() == 6);
	}
	SECTION("the planets are kept in the result") {
		auto aspects = calc.Calculate({ At(Planet::Mars, 0), At(Planet::Jupiter, 90) });
		REQUIRE(aspects.size() == 1);
		CHECK(aspects[0].Type == AspectType::Square);
		CHECK(aspects[0].Planet1.Planet == Planet::Mars);
		CHECK(aspects[0].Planet2.Planet == Planet::Jupiter);
	}
}

TEST_CASE("Hard and soft aspects", "[Aspects]") {
	AspectData a{};
	a.Type = AspectType::Trine;
	CHECK(a.IsSoft());
	CHECK_FALSE(a.IsHard());
	a.Type = AspectType::Sextile;
	CHECK(a.IsSoft());
	a.Type = AspectType::Square;
	CHECK(a.IsHard());
	CHECK_FALSE(a.IsSoft());
	a.Type = AspectType::Opposition;
	CHECK(a.IsHard());
	a.Type = AspectType::Conjunction;
	CHECK_FALSE(a.IsHard());
	CHECK_FALSE(a.IsSoft());
	CHECK(a.IsMajor());
	a.Type = AspectType::Quincunx;
	CHECK_FALSE(a.IsMajor());
}
