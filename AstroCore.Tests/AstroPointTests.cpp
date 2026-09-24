#include "TestCommon.h"
#include "AstroPoint.h"

TEST_CASE("A longitude is normalized into 0..360", "[AstroPoint]") {
	CHECK(AstroPoint(10).Value == Approx(10));
	CHECK(AstroPoint(370).Value == Approx(10));
	CHECK(AstroPoint(720.5).Value == Approx(0.5));
	CHECK(AstroPoint(360).Value == Approx(0));
	CHECK(AstroPoint(-10).Value == Approx(350));
	CHECK(AstroPoint(-370).Value == Approx(350));
	CHECK(AstroPoint(-0.5).Value == Approx(359.5));
}

TEST_CASE("Whole turns below zero normalize to zero", "[AstroPoint]") {
	CHECK(AstroPoint(-360).Value == Approx(0));
	CHECK(AstroPoint(-720).Value == Approx(0));
	// a tiny negative number must not come out as 360
	CHECK(AstroPoint(-1e-15).Value < 360);
	CHECK(AstroPoint(-1e-15).Value >= 0);
}

TEST_CASE("Assigning a number resets the flags", "[AstroPoint]") {
	AstroPoint p(100, AstroPointFlags::Retro);
	CHECK((p.Flags & AstroPointFlags::Retro) == AstroPointFlags::Retro);
	p = 200;
	CHECK(p.Flags == AstroPointFlags::None);
	CHECK(p.Value == Approx(200));
}

TEST_CASE("Converts to a double", "[AstroPoint]") {
	double v = AstroPoint(123.5);
	CHECK(v == Approx(123.5));
}

TEST_CASE("The sign of a longitude", "[AstroPoint]") {
	CHECK(AstroPoint(0).Sign() == ZodiacSign::Aries);
	CHECK(AstroPoint(29.999).Sign() == ZodiacSign::Aries);
	CHECK(AstroPoint(30).Sign() == ZodiacSign::Taurus);
	CHECK(AstroPoint(90).Sign() == ZodiacSign::Cancer);
	CHECK(AstroPoint(180).Sign() == ZodiacSign::Libra);
	CHECK(AstroPoint(270).Sign() == ZodiacSign::Capricorn);
	CHECK(AstroPoint(359.999).Sign() == ZodiacSign::Pisces);
}

TEST_CASE("Degrees, minutes and seconds within the sign", "[AstroPoint]") {
	AstroPoint p(45.5 + 30.0 / 3600);	// 15 degrees 30 minutes 30 seconds into Taurus
	CHECK(p.DegreeInSign() == Approx(15.508333).epsilon(1e-6));
	CHECK(p.Minutes() == Approx(30.5).epsilon(1e-4));
	CHECK(p.Seconds() == Approx(30).margin(0.05));

	CHECK(AstroPoint(0).DegreeInSign() == Approx(0));
	CHECK(AstroPoint(29.5).DegreeInSign() == Approx(29.5));
	CHECK(AstroPoint(330).DegreeInSign() == Approx(0));
}

TEST_CASE("Next sign and start of sign", "[AstroPoint]") {
	CHECK(AstroPoint(15).NextSign().Value == Approx(30));
	CHECK(AstroPoint(30).NextSign().Value == Approx(60));
	CHECK(AstroPoint(350).NextSign().Value == Approx(0));		// Pisces -> Aries
	CHECK(AstroPoint(45).ZeroSign().Value == Approx(30));
	CHECK(AstroPoint(29.9).ZeroSign().Value == Approx(0));
	CHECK(AstroPoint(359).ZeroSign().Value == Approx(330));
}

TEST_CASE("The opposite point", "[AstroPoint]") {
	CHECK(AstroPoint(10).Opposite().Value == Approx(190));
	CHECK(AstroPoint(190).Opposite().Value == Approx(10));
	CHECK(AstroPoint(0).Opposite().Value == Approx(180));
	CHECK(AstroPoint(180).Opposite().Value == Approx(0));
	CHECK(AstroPoint(359).Opposite().Value == Approx(179));
}

TEST_CASE("The angle between two points is the shorter way round", "[AstroPoint]") {
	CHECK(AstroPoint::Diff(10, 50) == Approx(40));
	CHECK(AstroPoint::Diff(50, 10) == Approx(40));
	CHECK(AstroPoint::Diff(350, 10) == Approx(20));
	CHECK(AstroPoint::Diff(10, 350) == Approx(20));
	CHECK(AstroPoint::Diff(0, 180) == Approx(180));
	CHECK(AstroPoint::Diff(90, 270) == Approx(180));
	CHECK(AstroPoint::Diff(100, 100) == Approx(0));
	CHECK(AstroPoint::Diff(1, 359) == Approx(2));
}

TEST_CASE("Midpoint", "[AstroPoint]") {
	CHECK(AstroPoint::MidPoint(10, 30).Value == Approx(20));
	CHECK(AstroPoint::MidPoint(0, 90).Value == Approx(45));
	CHECK(AstroPoint::MidPoint(100, 100).Value == Approx(100));
}

TEST_CASE("Midpoint across zero Aries", "[AstroPoint]") {
	// the midpoint is on the shorter arc between the two points
	CHECK(AstroPoint::MidPoint(10, 350).Value == Approx(0).margin(1e-9));
	CHECK(AstroPoint::MidPoint(300, 20).Value == Approx(340).margin(1e-9));
	CHECK(AstroPoint::MidPoint(350, 10).Value == Approx(0).margin(1e-9));
	CHECK(AstroPoint::MidPoint(340, 20).Value == Approx(0).margin(1e-9));
}

TEST_CASE("IsBetween", "[AstroPoint]") {
	CHECK(AstroPoint(15).IsBetween(10, 20));
	CHECK_FALSE(AstroPoint(25).IsBetween(10, 20));
	CHECK_FALSE(AstroPoint(5).IsBetween(10, 20));
	CHECK(AstroPoint(10).IsBetween(10, 20));
	CHECK(AstroPoint(20).IsBetween(10, 20));

	// a range that passes zero Aries: its upper end is a smaller number
	CHECK(AstroPoint(355).IsBetween(350, 20));
	CHECK_FALSE(AstroPoint(180).IsBetween(350, 20));
}

TEST_CASE("IsBetween after zero Aries", "[AstroPoint]") {
	CHECK(AstroPoint(5).IsBetween(350, 20));
	CHECK(AstroPoint(0).IsBetween(350, 20));
	CHECK(AstroPoint(20).IsBetween(350, 20));
	CHECK_FALSE(AstroPoint(25).IsBetween(350, 20));
	CHECK_FALSE(AstroPoint(349).IsBetween(350, 20));
}
