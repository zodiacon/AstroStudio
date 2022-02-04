#include "pch.h"
#include "CairoColor.h"

CairoColor::CairoColor(double r, double g, double b, double a) : R(r), G(g), B(b), A(a) {
}

CairoColor CairoColor::WithAlpha(double a) const {
	return CairoColor(R, G, B, a);
}

