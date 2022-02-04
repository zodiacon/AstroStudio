#include "pch.h"
#include "CairoRegion.h"

CairoRegion::CairoRegion() {
	m_rgn = cairo_region_create();
}

CairoRegion::CairoRegion(CairoRectI const& r) {
	m_rgn = cairo_region_create_rectangle(reinterpret_cast<const cairo_rectangle_int_t*>(&r));
}

CairoRegion::CairoRegion(std::vector<CairoRectI> const& rects) {
	m_rgn = cairo_region_create_rectangles(reinterpret_cast<const cairo_rectangle_int_t*>(rects.data()), (int)rects.size());
}

CairoRegion::CairoRegion(CairoRectI const* rects, int count) {
	m_rgn = cairo_region_create_rectangles(reinterpret_cast<const cairo_rectangle_int_t*>(rects), count);
}

CairoRegion::CairoRegion(CairoRegion&& other) : m_rgn(other.m_rgn) {
	other.m_rgn = nullptr;
}

CairoRegion& CairoRegion::operator=(CairoRegion&& other) {
	if (&other != this) {
		Destroy();
		m_rgn = other.m_rgn;
		other.m_rgn = nullptr;
	}
	return *this;
}

CairoRegion::operator const cairo_region_t* () const {
	return m_rgn;
}

void CairoRegion::Destroy() {
	if (m_rgn) {
		cairo_region_destroy(m_rgn);
		m_rgn = nullptr;
	}
}

CairoRegion::~CairoRegion() {
	Destroy();
}

CairoRegion& CairoRegion::Translate(int dx, int dy) {
	cairo_region_translate(m_rgn, dx, dy);
	return *this;
}

CairoRegion& CairoRegion::Union(CairoRegion const& rgn) {
	cairo_region_union(m_rgn, rgn);
	return *this;
}

CairoRegion& CairoRegion::Union(CairoRectI const& rect) {
	cairo_region_union_rectangle(m_rgn, reinterpret_cast<const cairo_rectangle_int_t*>(&rect));
	return *this;
}

CairoRegion& CairoRegion::Subtract(CairoRegion const& rgn) {
	cairo_region_subtract(m_rgn, rgn);
	return *this;
}

CairoRegion& CairoRegion::Subtract(CairoRectI const& rect) {
	cairo_region_subtract_rectangle(m_rgn, reinterpret_cast<const cairo_rectangle_int_t*>(&rect));
	return *this;
}

CairoRegion& CairoRegion::Intersect(CairoRegion const& rgn) {
	cairo_region_intersect(m_rgn, rgn);
	return *this;
}

CairoRegion& CairoRegion::Intersect(CairoRectI const& rect) {
	cairo_region_intersect_rectangle(m_rgn, reinterpret_cast<const cairo_rectangle_int_t*>(&rect));
	return *this;
}

CairoRegion::CairoRegion(cairo_region_t* rgn) : m_rgn(rgn) {
}

CairoRegion& CairoRegion::Xor(CairoRegion const& rgn) {
	cairo_region_xor(m_rgn, rgn);
	return *this;
}

CairoRegion& CairoRegion::Xor(CairoRectI const& r) {
	cairo_region_xor_rectangle(m_rgn, reinterpret_cast<const cairo_rectangle_int_t*>(&r));
	return *this;
}

CairoRegion CairoRegion::Copy() const {
	return CairoRegion(cairo_region_copy(m_rgn));
}

CairoStatus CairoRegion::Status() const {
	return static_cast<CairoStatus>(cairo_region_status(m_rgn));
}

bool CairoRegion::IsEmpty() const {
	return (bool)cairo_region_is_empty(m_rgn);
}

bool CairoRegion::ContainsPoint(int x, int y) const {
	return cairo_region_contains_point(m_rgn, x, y);
}

CairoRegionOverlap CairoRegion::ContainsRectangle(CairoRectI const& rect) const {
	return static_cast<CairoRegionOverlap>(cairo_region_contains_rectangle(m_rgn, reinterpret_cast<const cairo_rectangle_int_t*>(&rect)));
}

bool CairoRegion::IsEqual(CairoRegion const& other) {
	return cairo_region_equal(m_rgn, other);
}

bool CairoRegion::IsEqual(CairoRegion const& rgn1, CairoRegion const& rgn2) {
	return cairo_region_equal(rgn1, rgn2);
}
