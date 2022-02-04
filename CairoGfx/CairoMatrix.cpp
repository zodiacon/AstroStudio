#include "pch.h"
#include "CairoMatrix.h"
#include "CairoTypes.h"

static_assert(sizeof(CairoMatrix) == sizeof(cairo_matrix_t));

CairoMatrix& CairoMatrix::Identity() {
	cairo_matrix_init_identity(reinterpret_cast<cairo_matrix_t*>(this));
	return *this;
}

CairoMatrix& CairoMatrix::Scale(double sx, double sy) {
	cairo_matrix_scale(reinterpret_cast<cairo_matrix_t*>(this), sx, sy);
	return *this;
}

CairoMatrix& CairoMatrix::Translate(double dx, double dy) {
	cairo_matrix_translate(reinterpret_cast<cairo_matrix_t*>(this), dx, dy);
	return *this;
}

CairoMatrix& CairoMatrix::InitScale(double sx, double sy) {
	cairo_matrix_init_scale(reinterpret_cast<cairo_matrix_t*>(this), sx, sy);
	return *this;
}

CairoMatrix& CairoMatrix::InitTranslate(double dx, double dy) {
	cairo_matrix_init_translate(reinterpret_cast<cairo_matrix_t*>(this), dx, dy);
	return *this;
}

CairoMatrix& CairoMatrix::Rotate(double angle) {
	cairo_matrix_rotate(reinterpret_cast<cairo_matrix_t*>(this), angle);
	return *this;
}

CairoMatrix& CairoMatrix::InitRotate(double angle) {
	cairo_matrix_init_rotate(reinterpret_cast<cairo_matrix_t*>(this), angle);
	return *this;
}

CairoMatrix CairoMatrix::Multiply(CairoMatrix const& mat) const {
	CairoMatrix result;
	cairo_matrix_multiply(result, *this, mat);
	return result;
}

CairoMatrix& CairoMatrix::Multiply(CairoMatrix const& mat) {
	cairo_matrix_multiply(*this, *this, mat);
	return *this;
}

CairoPoint& CairoMatrix::Transform(CairoPoint& pt) {
	cairo_matrix_transform_point(*this, &pt.X, &pt.Y);
	return pt;
}

CairoMatrix::operator const cairo_matrix_t* () const {
	return reinterpret_cast<cairo_matrix_t*>(const_cast<CairoMatrix*>(this));
}

CairoMatrix::operator cairo_matrix_t* () {
	return reinterpret_cast<cairo_matrix_t*>(const_cast<CairoMatrix*>(this));
}
