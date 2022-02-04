#include "pch.h"
#include "CairoSurface.h"
#include "CairoColor.h"
#include "CairoFont.h"
#include "CairoCtx.h"
#include <cmath>

CairoCtx::CairoCtx(CairoSurface& surface) {
	m_cairo = cairo_create(surface);
}

CairoCtx::CairoCtx(CairoCtx&& other) : m_cairo(other.m_cairo) {
	other.m_cairo = nullptr;
}

CairoCtx& CairoCtx::operator=(CairoCtx&& other) {
	if (&other != this) {
		Destroy();
		m_cairo = other.m_cairo;
		other.m_cairo = nullptr;
	}
	return *this;
}

void CairoCtx::Destroy() {
	if (m_cairo) {
		cairo_destroy(m_cairo);
		m_cairo = nullptr;
	}
}

CairoCtx::~CairoCtx() {
	Destroy();
}

CairoCtx::operator bool() const {
	return m_cairo != nullptr;
}

CairoStatus CairoCtx::Status() const {
	return static_cast<CairoStatus>(cairo_status(m_cairo));
}

CairoCtx& CairoCtx::LineWidth(double width) {
	cairo_set_line_width(m_cairo, width);
	return *this;
}

double CairoCtx::LineWidth() const {
	return cairo_get_line_width(m_cairo);
}

CairoCtx& CairoCtx::Operator(CairoOperator op) {
	cairo_set_operator(m_cairo, static_cast<cairo_operator_t>(op));
	return *this;
}

CairoCtx& CairoCtx::SourceColor(double r, double g, double b, double a) {
	cairo_set_source_rgba(m_cairo, r, g, b, a);
	return *this;
}

CairoCtx& CairoCtx::SourceColor(CairoColor const& color) {
	return SourceColor(color.R, color.G, color.B, color.A);
}

CairoCtx& CairoCtx::Rectangle(double x, double y, double width, double height) {
	cairo_rectangle(m_cairo, x, y, width, height);
	return *this;
}

CairoCtx& CairoCtx::Rectangle(CairoRect const& r) {
	return Rectangle(r.X, r.Y, r.Width, r.Height);
}

CairoCtx& CairoCtx::Translate(double dx, double dy) {
	cairo_translate(m_cairo, dx, dy);
	return *this;
}

CairoCtx& CairoCtx::Transform(CairoMatrix const& m) {
	cairo_transform(m_cairo, m);
	return *this;
}

CairoCtx& CairoCtx::MoveTo(double x, double y, bool rel) {
	if (rel)
		cairo_rel_move_to(m_cairo, x, y);
	else
		cairo_move_to(m_cairo, x, y);
	return *this;
}

CairoCtx& CairoCtx::MoveTo(CairoPoint const& pt, bool rel) {
	return MoveTo(pt.X, pt.Y, rel);
}

CairoCtx& CairoCtx::LineTo(double x, double y, bool rel) {
	if (rel)
		cairo_rel_line_to(m_cairo, x, y);
	else
		cairo_line_to(m_cairo, x, y);
	return *this;
}

CairoCtx& CairoCtx::LineTo(CairoPoint const& pt, bool rel) {
	return LineTo(pt.X, pt.Y, rel);
}

CairoCtx& CairoCtx::Scale(double sx, double sy) {
	cairo_scale(m_cairo, sx, sy);
	return *this;
}

CairoPath CairoCtx::CopyPath(bool flat) {
	return CairoPath(flat ? cairo_copy_path_flat(m_cairo) : cairo_copy_path(m_cairo));
}

CairoCtx& CairoCtx::NewPath() {
	cairo_new_path(m_cairo);
	return *this;
}

CairoCtx& CairoCtx::NewSubPath() {
	cairo_new_sub_path(m_cairo);
	return *this;
}

CairoCtx& CairoCtx::ClosePath() {
	cairo_close_path(m_cairo);
	return *this;
}

CairoCtx& CairoCtx::AppendPath(CairoPath const& path) {
	cairo_append_path(m_cairo, path);
	return *this;
}

CairoCtx& CairoCtx::Clip(bool preserve) {
	preserve ? cairo_clip_preserve(m_cairo) : cairo_clip(m_cairo);
	return *this;
}

CairoCtx& CairoCtx::ResetClip() {
	cairo_reset_clip(m_cairo);
	return *this;
}

CairoRect CairoCtx::ClipExtents() const {
	CairoRect r;
	double x, y;
	cairo_clip_extents(m_cairo, &r.X, &r.Y, &x, &y);
	r.Width = x - r.X;
	r.Height = y - r.Y;
	return r;
}

bool CairoCtx::IsInClip(double x, double y) const {
	return cairo_in_clip(m_cairo, x, y);
}

CairoCtx& CairoCtx::CurveTo(double x1, double y1, double x2, double y2, double x3, double y3, bool rel) {
	if(rel)
		cairo_rel_curve_to(m_cairo, x1, y1, x2, y2, x3, y3);
	else
		cairo_curve_to(m_cairo, x1, y1, x2, y2, x3, y3);
	return *this;
}

CairoRect CairoCtx::PathExtents() const {
	CairoRect r;
	double x2, y2;
	cairo_path_extents(m_cairo, &r.X, &r.Y, &x2, &y2);
	r.Width = x2 - r.X;
	r.Height = y2 - r.Y;
	return r;
}

CairoCtx& CairoCtx::ShowText(char const* utf8text) {
	cairo_show_text(m_cairo, utf8text);
	return *this;
}

CairoCtx& CairoCtx::FontFace(CairoFont const& font) {
	cairo_set_font_face(m_cairo, font);
	return *this;
}

CairoCtx& CairoCtx::FontSize(double size) {
	cairo_set_font_size(m_cairo, size);
	return *this;
}

CairoMatrix CairoCtx::Matrix() const {
	CairoMatrix m;
	cairo_get_matrix(m_cairo, m);
	return m;
}

CairoCtx& CairoCtx::Matrix(CairoMatrix const& m) {
	cairo_set_matrix(m_cairo, m);
	return *this;
}

CairoCtx& CairoCtx::IdentityMatrix() {
	cairo_identity_matrix(m_cairo);
	return *this;
}

CairoCtx& CairoCtx::UserToDevice(double& x, double& y) {
	cairo_user_to_device(m_cairo, &x, &y);
	return *this;
}

CairoCtx& CairoCtx::UserToDevice(CairoPoint& p) {
	cairo_user_to_device(m_cairo, &p.X, &p.Y);
	return *this;
}

CairoCtx& CairoCtx::DeviceToUser(double& x, double& y) {
	cairo_device_to_user(m_cairo, &x, &y);
	return *this;
}

CairoCtx& CairoCtx::DeviceToUser(CairoPoint& p) {
	cairo_device_to_user(m_cairo, &p.X, &p.Y);
	return *this;
}

CairoCtx& CairoCtx::DeviceToUserDistance(double& x, double& y) {
	cairo_device_to_user_distance(m_cairo, &x, &y);
	return *this;
}

CairoCtx& CairoCtx::DeviceToUserDistance(CairoPoint& p) {
	cairo_device_to_user_distance(m_cairo, &p.X, &p.Y);
	return *this;
}

CairoTextExtents CairoCtx::TextExtents(char const* text) {
	CairoTextExtents ext;
	cairo_text_extents(m_cairo, text, reinterpret_cast<cairo_text_extents_t*>(&ext));
	return ext;
}

CairoCtx& CairoCtx::UserToDeviceDistance(double& x, double& y) {
	cairo_user_to_device_distance(m_cairo, &x, &y);
	return *this;
}

CairoCtx& CairoCtx::UserToDeviceDistance(CairoPoint& p) {
	cairo_user_to_device_distance(m_cairo, &p.X, &p.Y);
	return *this;
}

CairoCtx& CairoCtx::Stroke(bool preserve) {
	preserve ? cairo_stroke_preserve(m_cairo) : cairo_stroke(m_cairo);
	return *this;
}

CairoRect CairoCtx::StrokeExtents() const {
	CairoRect r;
	double x, y;
	cairo_stroke_extents(m_cairo, &r.X, &r.Y, &x, &y);
	r.Width = x - r.X;
	r.Height = y - r.Y;
	return r;
}

CairoCtx& CairoCtx::Fill(bool preserve) {
	preserve ? cairo_fill_preserve(m_cairo) : cairo_fill(m_cairo);
	return *this;
}

CairoRect CairoCtx::FillExtents() const {
	CairoRect r;
	double x, y;
	cairo_fill_extents(m_cairo, &r.X, &r.Y, &x, &y);
	r.Width = x - r.X;
	r.Height = y - r.Y;
	return r;
}

bool CairoCtx::IsInFill(double x, double y) const {
	return cairo_in_fill(m_cairo, x, y);
}

CairoCtx& CairoCtx::Paint(double alpha) {
	cairo_paint_with_alpha(m_cairo, alpha);
	return *this;
}

CairoCtx& CairoCtx::Arc(double cx, double cy, double radius, double startAngle, double endAngle, bool negative) {
	if(negative)
		cairo_arc_negative(m_cairo, cx, cy, radius, startAngle, endAngle);
	else
		cairo_arc(m_cairo, cx, cy, radius, startAngle, endAngle);
	return *this;
}

CairoCtx& CairoCtx::Circle(double cx, double cy, double radius) {
	return Arc(cx, cy, radius, 0, Rad(360));
}

CairoCtx& CairoCtx::Ellipse(double cx, double cy, double rx, double ry) {
	auto m = Matrix();
	Translate(cx, cy).Scale(1, ry / rx).Translate(-cx, -cy).Circle(cx, cy, rx).Scale(1, rx / ry);
	return Matrix(m);
}

CairoCtx::operator cairo_t* () const {
	return m_cairo;
}

CairoCtx& CairoCtx::RoundedRectangle(double x, double y, double width, double height, double radius) {
	if (radius == 0)
		radius = height / 10;

	NewSubPath();
	Arc(x + width - radius, y + radius, radius, Rad(-90), 0);
	Arc(x + width - radius, y + height - radius, radius, 0, Rad(90));
	Arc(x + radius, y + height - radius, radius, Rad(90), Rad(180));
	Arc(x + radius, y + radius, radius, Rad(180), Rad(270));
	return ClosePath();
}

CairoCtx& CairoCtx::RoundedRectangle(CairoRect const& r, double cornerRadius) {
	return RoundedRectangle(r.X, r.Y, r.Width, r.Height, cornerRadius);
}

CairoCtx& CairoCtx::RoundedRectangle(CairoRect const& r, CairoColor const& stroke, CairoColor const& fill, double lineWidth, double cornerRadius) {
	RoundedRectangle(r, cornerRadius);
	return SourceColor(fill).Fill(true).LineWidth(lineWidth).SourceColor(stroke).Stroke();
}

CairoCtx& CairoCtx::Rotate(double angle) {
	cairo_rotate(m_cairo, angle);
	return *this;
}

CairoCtx& CairoCtx::Save() {
	cairo_save(m_cairo);
	return *this;
}

CairoCtx& CairoCtx::Restore() {
	cairo_restore(m_cairo);
	return *this;
}

CairoSurface CairoCtx::Target() const {
	auto surface = cairo_get_target(m_cairo);
	cairo_surface_reference(surface);
	return CairoSurface(surface);
}

CairoCtx& CairoCtx::CopyPage() {
	cairo_copy_page(m_cairo);
	return *this;
}

CairoCtx& CairoCtx::ShowPage() {
	cairo_show_page(m_cairo);
	return *this;
}

CairoCtx& CairoCtx::PushGroup() {
	cairo_push_group(m_cairo);
	return *this;
}

CairoPattern CairoCtx::PopGroup() {
	return CairoPattern(cairo_pop_group(m_cairo));
}

CairoCtx& CairoCtx::PushGroup(CairoContent content) {
	cairo_push_group_with_content(m_cairo, static_cast<cairo_content_t>(content));
	return *this;
}

CairoCtx& CairoCtx::PopGroupToSource() {
	cairo_pop_group_to_source(m_cairo);
	return *this;
}

CairoCtx& CairoCtx::Mask(CairoPattern const& pat) {
	cairo_mask(m_cairo, pat);
	return *this;
}

CairoCtx& CairoCtx::Mask(CairoSurface const& surface, double x, double y) {
	cairo_mask_surface(m_cairo, const_cast<CairoSurface&>(surface), x, y);
	return *this;
}

CairoCtx& CairoCtx::Clear(CairoColor const& color) {
	return SourceColor(color.WithAlpha(1)).Mask(CairoPattern(0, 0, 0, color.A));
}

CairoCtx& CairoCtx::Source(CairoPattern const& pattern) {
	cairo_set_source(m_cairo, pattern);
	return *this;
}

CairoCtx& CairoCtx::Source(CairoSurface const& surface, double x, double y) {
	cairo_set_source_surface(m_cairo, const_cast<CairoSurface&>(surface), x, y);
	return *this;
}

CairoPattern CairoCtx::Source() const {
	auto pat = cairo_get_source(m_cairo);
	cairo_pattern_reference(pat);
	return CairoPattern(pat);
}

CairoCtx& CairoCtx::Antialias(CairoAntialias antialias) {
	cairo_set_antialias(m_cairo, static_cast<cairo_antialias_t>(antialias));
	return *this;
}

CairoAntialias CairoCtx::Antialias() const {
	return static_cast<CairoAntialias>(cairo_get_antialias(m_cairo));
}

CairoCtx& CairoCtx::Dash(double const* dashes, int count, double offset) {
	cairo_set_dash(m_cairo, dashes, count, offset);
	return *this;
}

CairoCtx& CairoCtx::Dash(std::vector<double> const& dashes, double offset) {
	return Dash(dashes.data(), (int)dashes.size(), offset);
}

CairoCtx& CairoCtx::Dash(double value, double offset) {
	return Dash(&value, 1, offset);
}

CairoCtx& CairoCtx::DashDisable() {
	return Dash(nullptr, 0, 0);
}

CairoCtx& CairoCtx::FillRule(CairoFillRule rule) {
	cairo_set_fill_rule(m_cairo, static_cast<cairo_fill_rule_t>(rule));
	return *this;
}

CairoFillRule CairoCtx::FillRule() const {
	return static_cast<CairoFillRule>(cairo_get_fill_rule(m_cairo));
}

CairoCtx& CairoCtx::Tolerance(double tolerance) {
	cairo_set_tolerance(m_cairo, tolerance);
	return *this;
}

double CairoCtx::Tolerance() const {
	return cairo_get_tolerance(m_cairo);
}
