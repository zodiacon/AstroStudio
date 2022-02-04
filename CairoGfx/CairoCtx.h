#pragma once

#include <vector>
#include "CairoPath.h"
#include "CairoMatrix.h"
#include "CairoPattern.h"
#include "CairoTypes.h"

class CairoSurface;
struct CairoColor;
class CairoFont;

typedef struct _cairo cairo_t;

enum class CairoOperator {
	Clear,
	
	Source,
	Over,
	In,
	Out,
	Atop,
	
	Dest,
	DestOver,
	DestIn,
	DestOut,
	DestAtop,
	
	Xor,
	Add,
	Saturate,
	
	Multiply,
	Screen,
	Overlay,
	Darken,
	Lighten,
	ColorDodge,
	ColorBurn,
	HardLight,
	SoftLight,
	Difference,
	Exclusion,
	HslHue,
	HslSaturation,
	HslColor,
	HslLuminosity
};

enum class CairoFillRule {
	Winding,
	EvenOdd
};

enum class CairoAntialias {
	Default,

	/* method */
	None,
	Gray,
	Subpixel,

	/* hints */
	Fast,
	Good,
	Best
};

class CairoCtx final {
public:
	CairoCtx(CairoSurface& surface);
	CairoCtx(CairoCtx const&) = delete;
	CairoCtx& operator=(const CairoCtx&) = delete;
	CairoCtx(CairoCtx&&);
	CairoCtx& operator=(CairoCtx&& other);

	void Destroy();
	~CairoCtx();

	operator bool() const;

	CairoStatus Status() const;
	CairoCtx& LineWidth(double width);
	double LineWidth() const;
	CairoCtx& Operator(CairoOperator op);
	CairoOperator Operator() const;
	CairoCtx& SourceColor(double r, double g, double b, double a = 1.0);
	CairoCtx& SourceColor(CairoColor const& color);
	CairoCtx& Rectangle(double x, double y, double width, double height);
	CairoCtx& Rectangle(CairoRect const& r);
	CairoCtx& RoundedRectangle(double x, double y, double width, double height, double cornerRadius = 0);
	CairoCtx& RoundedRectangle(CairoRect const& r, double cornerRadius = 0);
	CairoCtx& RoundedRectangle(CairoRect const& r, CairoColor const& stroke, CairoColor const& fill, double lineWidth, double cornerRadius = 0);

	CairoCtx& Translate(double dx, double dy);
	CairoCtx& Transform(CairoMatrix const& m);
	CairoCtx& MoveTo(double x, double y, bool rel = false);
	CairoCtx& MoveTo(CairoPoint const& pt, bool rel = false);
	CairoCtx& LineTo(double x, double y, bool rel = false);
	CairoCtx& LineTo(CairoPoint const& pt, bool rel = false);
	CairoCtx& Scale(double sx, double sy);
	CairoCtx& Rotate(double angle);
	CairoPath CopyPath(bool flat = false);
	CairoCtx& NewPath();
	CairoCtx& NewSubPath();
	CairoCtx& ClosePath();
	CairoCtx& AppendPath(CairoPath const& path);
	CairoCtx& Clip(bool preserve = false);
	CairoCtx& ResetClip();
	CairoRect ClipExtents() const;
	bool IsInClip(double x, double y) const;
	CairoCtx& CurveTo(double x1, double y1, double x2, double y2, double x3, double y3, bool rel = false);
	CairoRect PathExtents() const;
	CairoCtx& ShowText(char const* utf8text);
	CairoCtx& FontFace(CairoFont const& font);
	CairoCtx& FontSize(double size);
	CairoCtx& Antialias(CairoAntialias antialias);
	CairoAntialias Antialias() const;
	CairoCtx& Dash(double const* dashes, int count, double offset);
	CairoCtx& Dash(std::vector<double> const& dashes, double offset);
	CairoCtx& Dash(double value, double offset);
	CairoCtx& DashDisable();
	CairoCtx& FillRule(CairoFillRule rule);
	CairoFillRule FillRule() const;
	CairoCtx& Tolerance(double tolerance);
	double Tolerance() const;

	CairoMatrix Matrix() const;
	CairoCtx& Matrix(CairoMatrix const& m);
	CairoCtx& IdentityMatrix();
	CairoCtx& UserToDevice(double& x, double& y);
	CairoCtx& UserToDevice(CairoPoint& p);
	CairoCtx& DeviceToUser(double& x, double& y);
	CairoCtx& DeviceToUser(CairoPoint& p);
	CairoCtx& UserToDeviceDistance(double& x, double& y);
	CairoCtx& UserToDeviceDistance(CairoPoint& p);
	CairoCtx& DeviceToUserDistance(double& x, double& y);
	CairoCtx& DeviceToUserDistance(CairoPoint& p);
	CairoTextExtents TextExtents(char const* text);

	CairoCtx& Stroke(bool preserve = false);
	CairoRect StrokeExtents() const;
	CairoCtx& Fill(bool preserve = false);
	CairoRect FillExtents() const;
	bool IsInFill(double x, double y) const;
	CairoCtx& Paint(double alpha = 1.0);
	CairoCtx& Clear(CairoColor const& color);
	CairoCtx& Mask(CairoPattern const& pat);
	CairoCtx& Mask(CairoSurface const& surface, double x, double y);
	CairoCtx& Arc(double cx, double cy, double radius, double startAngle, double endAngle, bool negative = false);
	CairoCtx& Circle(double cx, double cy, double radius);
	CairoCtx& Ellipse(double cx, double cy, double rx, double ry);
	CairoCtx& Source(CairoPattern const& pattern);
	CairoCtx& Source(CairoSurface const& surface, double x, double y);
	CairoPattern Source() const;

	CairoCtx& Save();
	CairoCtx& Restore();
	CairoCtx& PushGroup();
	CairoCtx& PushGroup(CairoContent content);
	CairoPattern PopGroup();
	CairoCtx& PopGroupToSource();
	CairoSurface Target() const;
	CairoCtx& CopyPage();
	CairoCtx& ShowPage();

	operator cairo_t* () const;

private:
	cairo_t* m_cairo{ nullptr };
};

