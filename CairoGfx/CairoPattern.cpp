#include "pch.h"
#include "CairoPattern.h"
#include "CairoColor.h"
#include "CairoMatrix.h"

CairoPattern::CairoPattern(CairoSurface& surface) {
	m_pattern = cairo_pattern_create_for_surface(surface);
}

CairoPattern::CairoPattern(cairo_pattern_t* pattern) : m_pattern(pattern) {
}

CairoPattern::CairoPattern(CairoColor const& color) {
	m_pattern = cairo_pattern_create_rgba(color.R, color.G, color.B, color.A);
}

CairoPattern::CairoPattern(double r, double g, double b, double a) {
	m_pattern = cairo_pattern_create_rgba(r, g, b, a);
}

CairoPattern CairoPattern::CreateLinear(double x0, double y0, double x1, double y1) {
	return CairoPattern(cairo_pattern_create_linear(x0, y0, x1, y1));
}

CairoPattern CairoPattern::CreateLinear(CairoPoint const& p0, CairoPoint const& p1) {
	return CreateLinear(p0.X, p0.Y, p1.X, p1.Y);
}

CairoPattern CairoPattern::CreateRadial(double cx0, double cy0, double radius0, double cx1, double cy1, double radius1) {
	return CairoPattern(cairo_pattern_create_radial(cx0, cy0, radius0, cx1, cy1, radius1));
}

CairoPattern CairoPattern::CreateRadial(CairoPoint const& p0, double radius0, CairoPoint const& p1, double radius1) {
	return CreateRadial(p0.X, p0.Y, radius0, p1.X, p1.Y, radius1);
}

CairoPattern CairoPattern::CreateMesh() {
	return CairoPattern(cairo_pattern_create_mesh());
}

CairoPattern::~CairoPattern() {
	assert(m_pattern);
	cairo_pattern_destroy(m_pattern);
}

CairoPattern& CairoPattern::AddColorStop(double offset, CairoColor const& color) {
	return AddColorStop(offset, color.R, color.G, color.B, color.A);
}

CairoPattern& CairoPattern::AddColorStop(double offset, double r, double g, double b, double a) {
	cairo_pattern_add_color_stop_rgba(m_pattern, offset, r, g, b, a);
	return *this;
}

CairoPattern& CairoPattern::Matrix(CairoMatrix const& matrix) {
	cairo_pattern_set_matrix(m_pattern, matrix);
	return *this;
}

CairoPattern& CairoPattern::Filter(CairoFilter filter) {
	cairo_pattern_set_filter(m_pattern, static_cast<cairo_filter_t>(filter));
	return *this;
}

CairoPattern::operator cairo_pattern_t* () const {
	return m_pattern;
}

CairoSurface CairoPattern::Surface() const {
	cairo_surface_t* surface;
	cairo_pattern_get_surface(m_pattern, &surface);
	cairo_surface_reference(surface);
	return CairoSurface(surface);
}

CairoStatus CairoPattern::Status() const {
	return static_cast<CairoStatus>(cairo_pattern_status(m_pattern));
}

CairoPatternType CairoPattern::Type() const {
	return static_cast<CairoPatternType>(cairo_pattern_get_type(m_pattern));
}
