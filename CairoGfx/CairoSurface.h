#pragma once

#include "CairoTypes.h"

typedef struct _cairo_surface cairo_surface_t;

enum class CairoSurfaceType {
	Image,
	Pdf,
	Ps,
	Xlib,
	Xcb,
	Glitz,
	Quartz,
	Win32,
	Beos,
	Directfb,
	Svg,
	Os2,
	Win32_printing,
	Quartz_image,
	Script,
	Qt,
	Recording,
	Vg,
	Gl,
	Drm,
	Tee,
	Xml,
	Skia,
	Subsurface,
	Cogl
};

class CairoMappedSurface;

class CairoSurface {
public:
	CairoSurface() = default;
	explicit CairoSurface(HDC hdc);
	CairoSurface(HDC hdc, CairoFormat format);

	CairoSurface(CairoSurface const&) = delete;
	CairoSurface& operator=(CairoSurface const&) = delete;
	CairoSurface(CairoSurface&& other);
	CairoSurface& operator=(CairoSurface&& other);

	static CairoSurface CreateImage(CairoFormat format, int width, int height);
	static CairoSurface CreatePrinting(HDC hdc);
	static CairoSurface CreateWithDib(CairoFormat format, int width, int height);
	static CairoSurface CreateWithDdb(HDC hdc, CairoFormat format, int width, int height);
	static CairoSurface CreateFromPng(char const* filename);
	static CairoSurface CreateForImage(unsigned char* data, CairoFormat format, int width, int height, int stride);
	static CairoSurface CreateForRectangle(CairoSurface const& target, double x, double y, double width, double height);
	static CairoSurface CreateForRectangle(CairoSurface const& target, CairoRect const& r);
	static CairoSurface CreateSimilar(CairoSurface const& other, CairoContent content, int width, int height);
	static CairoSurface CreateSimilarImage(CairoSurface const& other, CairoFormat format, int width, int height);
	static CairoSurface CreateSvg(char const* filename, double widthInPoints, double heightInPoints);

	~CairoSurface();

	operator cairo_surface_t* ();
	operator const cairo_surface_t* () const;
	operator bool() const;

	CairoSurface& Flush();
	CairoSurface& Finish();
	CairoSurface& CopyPage();
	CairoSurface& ShowPage();
	CairoSurface& FallbackResolution(double pixelsPerInchX, double pixelsPerInchY);
	CairoSurface& DeviceScale(double xScale, double yScale);
	CairoSurface& DeviceOffset(double x, double y);
	CairoSurface& MarkDirty();
	CairoSurface& MarkDirty(CairoRectI const& r);
	CairoMappedSurface MapToImage(CairoRectI const& extents);

	HDC GetDC() const;
	bool HasShowTextGlyphs() const;
	CairoStatus WriteToPng(char const* filename);
	CairoStatus Status() const;
	int Width() const;
	int Height() const;
	int Stride() const;
	CairoSurfaceType Type() const;
	CairoContent Content() const;
	CairoFormat Format() const;
	unsigned char* Data() const;
	CairoSurface Image() const;

protected:
	explicit CairoSurface(cairo_surface_t* surface);
	virtual void Destroy();

	friend class CairoCtx;
	friend class CairoPattern;

	cairo_surface_t* m_surface{ nullptr };
};

class CairoMappedSurface final : CairoSurface {
	friend class CairoCtx;
protected:
	void Destroy() override;

private:
	CairoMappedSurface(cairo_surface_t* surface, cairo_surface_t* image);

	cairo_surface_t* m_image;
};
