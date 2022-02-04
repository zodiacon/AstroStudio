#include "pch.h"
#include "AstroPoint.h"

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

bool AstroPoint::IsBetween(AstroPoint const& start, AstroPoint const& end) {
	double start1 = start, end1 = end;
	if (end1 < start1)
		end1 += 360;
	auto degree = Value;
	if (fabs(start1 - degree) > 180)
		degree += 360;
	return start1 <= Value && Value <= end1;
}

AstroPoint AstroPoint::ZeroSign() const {
	return AstroPoint((int)Sign() * 30);
}

AstroPoint AstroPoint::Opposite() const {
	return AstroPoint(Value + 180).Normalize();
}

AstroPoint& AstroPoint::Normalize() {
	if (Value < 0)
		Value += 360 * (1 - int(Value) / 360);
	else if (Value >= 360)
		Value -= 360 * (int(Value) / 360);
	return *this;
}
