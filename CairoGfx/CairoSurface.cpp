#include "pch.h"
#include "CairoSurface.h"

CairoSurface::CairoSurface(HDC hdc) {
	m_surface = cairo_win32_surface_create(hdc);
}

CairoSurface::CairoSurface(cairo_surface_t* surface) {
	m_surface = surface;
}

CairoSurface::CairoSurface(HDC hdc, CairoFormat format) {
	m_surface = cairo_win32_surface_create_with_format(hdc, static_cast<cairo_format_t>(format));
}

CairoSurface::CairoSurface(CairoSurface&& other) : m_surface(other.m_surface) {
	other.m_surface = nullptr;
}

CairoSurface& CairoSurface::operator=(CairoSurface&& other) {
	if (&other != this) {
		Destroy();
		m_surface = other.m_surface;
		other.m_surface = nullptr;
	}
	return *this;
}

CairoSurface CairoSurface::CreateImage(CairoFormat format, int width, int height) {
	return CairoSurface(cairo_image_surface_create(static_cast<cairo_format_t>(format), width, height));
}

CairoSurface CairoSurface::CreatePrinting(HDC hdc) {
	return CairoSurface(cairo_win32_printing_surface_create(hdc));
}

CairoSurface CairoSurface::CreateWithDib(CairoFormat format, int width, int height) {
	return CairoSurface(cairo_win32_surface_create_with_dib(static_cast<cairo_format_t>(format), width, height));
}

CairoSurface CairoSurface::CreateWithDdb(HDC hdc, CairoFormat format, int width, int height) {
	return CairoSurface(cairo_win32_surface_create_with_ddb(hdc, static_cast<cairo_format_t>(format), width, height));
}

CairoSurface CairoSurface::CreateFromPng(char const* filename) {
	return CairoSurface(cairo_image_surface_create_from_png(filename));
}

CairoSurface CairoSurface::CreateForImage(unsigned char* data, CairoFormat format, int width, int height, int stride) {
	return CairoSurface(cairo_image_surface_create_for_data(data, static_cast<cairo_format_t>(format), width, height, stride));
}

CairoSurface CairoSurface::CreateForRectangle(CairoSurface const& target, double x, double y, double width, double height) {
	return CairoSurface(cairo_surface_create_for_rectangle(const_cast<CairoSurface&>(target), x, y, width, height));
}

CairoSurface CairoSurface::CreateForRectangle(CairoSurface const& target, CairoRect const& r) {
	return CreateForRectangle(target, r.X, r.Y, r.Width, r.Height);
}

CairoSurface CairoSurface::CreateSimilar(CairoSurface const& other, CairoContent content, int width, int height) {
	return CairoSurface(cairo_surface_create_similar(const_cast<CairoSurface&>(other), static_cast<cairo_content_t>(content), width, height));
}

CairoSurface CairoSurface::CreateSimilarImage(CairoSurface const& other, CairoFormat format, int width, int height) {
	return CairoSurface(cairo_surface_create_similar_image(const_cast<CairoSurface&>(other), static_cast<cairo_format_t>(format), width, height));
}

CairoSurface CairoSurface::CreateSvg(char const* filename, double widthInPoints, double heightInPoints) {
	return CairoSurface(cairo_svg_surface_create(filename, widthInPoints, heightInPoints));
}

CairoSurface& CairoSurface::Flush() {
	cairo_surface_flush(m_surface);
	return *this;
}

CairoSurface& CairoSurface::Finish() {
	cairo_surface_finish(m_surface);
	return *this;
}

CairoSurface& CairoSurface::CopyPage() {
	cairo_surface_copy_page(m_surface);
	return *this;
}

CairoSurface& CairoSurface::ShowPage() {
	cairo_surface_show_page(m_surface);
	return *this;
}

CairoSurface& CairoSurface::FallbackResolution(double pixelsPerInchX, double pixelsPerInchY) {
	cairo_surface_set_fallback_resolution(m_surface, pixelsPerInchX, pixelsPerInchY);
	return *this;
}

CairoSurface& CairoSurface::DeviceScale(double xScale, double yScale) {
	cairo_surface_set_device_scale(m_surface, xScale, yScale);
	return *this;
}

CairoSurface& CairoSurface::DeviceOffset(double x, double y) {
	cairo_surface_set_device_offset(m_surface, x, y);
	return *this;
}

CairoSurface& CairoSurface::MarkDirty() {
	cairo_surface_mark_dirty(m_surface);
	return *this;
}

CairoSurface& CairoSurface::MarkDirty(CairoRectI const& r) {
	cairo_surface_mark_dirty_rectangle(m_surface, r.X, r.Y, r.Width, r.Height);
	return *this;
}

HDC CairoSurface::GetDC() const {
	return cairo_win32_surface_get_dc(m_surface);
}

bool CairoSurface::HasShowTextGlyphs() const {
	return cairo_surface_has_show_text_glyphs(m_surface);
}

CairoStatus CairoSurface::WriteToPng(char const* filename) {
	return static_cast<CairoStatus>(cairo_surface_write_to_png(m_surface, filename));
}

CairoStatus CairoSurface::Status() const {
	return static_cast<CairoStatus>(cairo_surface_status(m_surface));
}

int CairoSurface::Width() const {
	return cairo_image_surface_get_width(m_surface);
}

int CairoSurface::Height() const {
	return cairo_image_surface_get_height(m_surface);
}

int CairoSurface::Stride() const {
	return cairo_image_surface_get_stride(m_surface);
}

CairoSurfaceType CairoSurface::Type() const {
	return static_cast<CairoSurfaceType>(cairo_surface_get_type(m_surface));
}

CairoContent CairoSurface::Content() const {
	return static_cast<CairoContent>(cairo_surface_get_content(m_surface));
}

CairoFormat CairoSurface::Format() const {
	return static_cast<CairoFormat>(cairo_image_surface_get_format(m_surface));
}

unsigned char* CairoSurface::Data() const {
	return cairo_image_surface_get_data(m_surface);
}

CairoSurface CairoSurface::Image() const {
	auto image = cairo_win32_surface_get_image(m_surface);
	if (image) {
		cairo_surface_reference(image);
	}
	return CairoSurface(image);
}

void CairoSurface::Destroy() {
	if (m_surface) {
		cairo_surface_destroy(m_surface);
		m_surface = nullptr;
	}
}

CairoSurface::~CairoSurface() {
	Destroy();
}

CairoSurface::operator cairo_surface_t* () {
	return m_surface;
}

CairoSurface::operator const cairo_surface_t* () const {
	return m_surface;
}

CairoSurface::operator bool() const {
	return m_surface != nullptr;
}

void CairoMappedSurface::Destroy() {
	if (m_surface) {
		cairo_surface_unmap_image(m_surface, m_image);
		m_surface = nullptr;
	}
}
