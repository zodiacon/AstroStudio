#pragma once

#include <vector>
#include "CairoTypes.h"

typedef struct _cairo_region cairo_region_t;

enum class CairoRegionOverlap {
	In,		/* completely inside region */
	Out,	/* completely outside region */
	Part	/* partly inside region */
};

class CairoRegion final {
public:
	CairoRegion();
	CairoRegion(CairoRectI const& r);
	CairoRegion(std::vector<CairoRectI> const& v);
	CairoRegion(CairoRectI const* rects, int count);

	CairoRegion(CairoRegion const&) = delete;
	CairoRegion& operator=(CairoRegion const&) = delete;
	CairoRegion(CairoRegion&& other);
	CairoRegion& operator=(CairoRegion&& other);

	operator const cairo_region_t* () const;

	void Destroy();
	~CairoRegion();

	CairoRegion& Translate(int dx, int dy);
	CairoRegion& Union(CairoRegion const& rgn);
	CairoRegion& Union(CairoRectI const& r);
	CairoRegion& Subtract(CairoRegion const& rgn);
	CairoRegion& Subtract(CairoRectI const& r);
	CairoRegion& Intersect(CairoRegion const& rgn);
	CairoRegion& Intersect(CairoRectI const& r);
	CairoRegion& Xor(CairoRegion const& rgn);
	CairoRegion& Xor(CairoRectI const& r);

	CairoRegion Copy() const;
	CairoStatus Status() const;
	bool IsEmpty() const;
	bool ContainsPoint(int x, int y) const;
	CairoRegionOverlap ContainsRectangle(CairoRectI const& r) const;
	bool IsEqual(CairoRegion const& other);
	static bool IsEqual(CairoRegion const& rgn1, CairoRegion const& rgn2);

private:
	explicit CairoRegion(cairo_region_t* rgn);

	cairo_region_t* m_rgn;
};

