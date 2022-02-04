#pragma once

#include "CairoTypes.h"

typedef struct _cairo_matrix cairo_matrix_t;

class CairoMatrix final {
public:
	CairoMatrix() = default;
	CairoMatrix& Identity();
	CairoMatrix& Scale(double dx, double sy);
	CairoMatrix& InitScale(double sx, double sy);
	CairoMatrix& Translate(double dx, double dy);
	CairoMatrix& InitTranslate(double dx, double sy);
	CairoMatrix& Rotate(double angle);
	CairoMatrix& InitRotate(double angle);

	CairoMatrix Multiply(CairoMatrix const& mat) const;
	CairoMatrix& Multiply(CairoMatrix const& mat);
	CairoPoint& Transform(CairoPoint& pt);

	operator const cairo_matrix_t* () const;
	operator cairo_matrix_t* ();

	double xx; double yx;
	double xy; double yy;
	double x0; double y0;
};

