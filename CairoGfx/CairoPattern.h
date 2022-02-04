#pragma once

#include "CairoTypes.h"
#include "CairoSurface.h"

struct CairoColor;
class CairoMatrix;

typedef struct _cairo_pattern cairo_pattern_t;

enum class CairoPatternType {
	Solid,
	Surface,
	Linear,
	Radial,
	Mesh,
	RasterSource
};

enum class CairoFilter {
	Fast,
	Good,
	Best,
	Nearest,
	Bilinear,
	Gaussian
};

class CairoPattern final {
public:
	explicit CairoPattern(CairoSurface& surface);
	explicit CairoPattern(cairo_pattern_t* pattern);
	CairoPattern(CairoColor const& color);
	CairoPattern(double r, double g, double b, double a = 1.0);
	static CairoPattern CreateLinear(double x0, double y0, double x1, double y1);
	static CairoPattern CreateLinear(CairoPoint const& p0, CairoPoint const& p1);
	static CairoPattern CreateRadial(double cx0, double cy0, double radius0, double cx1, double cy1, double radius1);
	static CairoPattern CreateRadial(CairoPoint const& p0, double radius0, CairoPoint const& p1, double radius1);
	static CairoPattern CreateMesh();

	~CairoPattern();

	CairoPattern& AddColorStop(double offset, CairoColor const& color);
	CairoPattern& AddColorStop(double offset, double r, double g, double b, double a = 1.0);
	CairoPattern& Matrix(CairoMatrix const& matrix);
	CairoPattern& Filter(CairoFilter filter);

	operator cairo_pattern_t* () const;

	CairoSurface Surface() const;
	CairoStatus Status() const;
	CairoPatternType Type() const;

private:
	cairo_pattern_t* m_pattern;
};

