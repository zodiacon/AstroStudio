#pragma once


struct CairoColor {
	CairoColor(double r, double g, double b, double a = 1.0);
	CairoColor() = default;
	CairoColor WithAlpha(double a) const;

	double R, G, B, A;
};

