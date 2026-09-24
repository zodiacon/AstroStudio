#include "pch.h"
#include "AstroPoint.h"

AstroPoint::AstroPoint(double value, AstroPointFlags flags) : Value(value), Flags(flags) {
	Normalize();
}

AstroPoint& AstroPoint::operator=(double value) {
	Value = value;
	Flags = AstroPointFlags::None;
	return Normalize();
}

double AstroPoint::DegreeInSign() const {
	return Value - int(Value / 30) * 30;
}

double AstroPoint::Minutes() const {
	return (DegreeInSign() - (int)DegreeInSign()) * 60;
}

double AstroPoint::Seconds() const {
	return (Minutes() - (int)Minutes()) * 60;
}

AstroPoint AstroPoint::NextSign() const {
	return Sign() == ZodiacSign::Pisces ? 0 : ((int)Sign() + 1) * 30;
}

ZodiacSign AstroPoint::Sign() const {
	return ZodiacSign((int)(Value / 30));
}

double AstroPoint::Diff(AstroPoint const& p1, AstroPoint const& p2) {
	double angle = fabs(p1 - p2);
	if (angle > 180)
		angle = 360 - angle;
	return angle;
}

bool AstroPoint::IsBetween(AstroPoint const& start, AstroPoint const& end) const {
	// the range runs forward from start to end, and may pass 0 Aries (then end is the smaller number)
	double start1 = start, end1 = end;
	if (end1 < start1)
		end1 += 360;
	auto degree = Value;
	if (degree < start1)
		degree += 360;
	return degree <= end1;
}

AstroPoint AstroPoint::ZeroSign() const {
	return AstroPoint((int)Sign() * 30);
}

AstroPoint AstroPoint::Opposite() const {
	return AstroPoint(Value - 180).Normalize();
}

AstroPoint& AstroPoint::Normalize() {
	if (Value < 0 || Value >= 360) {
		Value = fmod(Value, 360);
		if (Value < 0)
			Value += 360;
		if (Value >= 360)		// a tiny negative value rounds up to 360 when 360 is added
			Value = 0;
	}
	return *this;
}

AstroPoint AstroPoint::Normalize() const {
	auto p = *this;
	return p.Normalize();
}

AstroPoint AstroPoint::MidPoint(AstroPoint const& p1, AstroPoint const& p2) {
	// on the shorter arc between the two: the midpoint of 350 and 10 is 0, not 180
	double delta = fmod(p2.Value - p1.Value, 360);
	if (delta > 180)
		delta -= 360;
	else if (delta < -180)
		delta += 360;
	return AstroPoint(p1.Value + delta / 2).Normalize();
}
