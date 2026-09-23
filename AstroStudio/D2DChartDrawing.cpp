#include "pch.h"
#include "D2DChartDrawing.h"
#include <cmath>
#include <numbers>
#include "PlanetSpacer.h"
#include "DefaultFont.h"
#include "resource.h"

namespace {
	constexpr double Radians(double degrees) {
		return degrees * std::numbers::pi / 180;
	}

	const D2D1_COLOR_F Black = D2D1::ColorF(D2D1::ColorF::Black);
	const D2D1_COLOR_F Gray = D2D1::ColorF(D2D1::ColorF::Gray);
	const D2D1_COLOR_F Blue = D2D1::ColorF(D2D1::ColorF::Blue);

	// point on a circle, angle in degrees measured from the positive X axis; Y grows downward, so
	// increasing angles run clockwise on screen
	D2D1_POINT_2F PointOnCircle(D2D1_POINT_2F const& center, float radius, double degrees) {
		auto rad = Radians(degrees);
		return D2D1::Point2F(center.x + radius * (float)std::cos(rad), center.y + radius * (float)std::sin(rad));
	}
}

HRESULT D2DChartDrawing::Draw(ID2D1RenderTarget* rt, float size) {
	if (rt == nullptr || m_data == nullptr)
		return E_POINTER;

	auto hr = D2DResources::Get().Ensure();
	if (FAILED(hr))
		return hr;

	D2D1_MATRIX_3X2_F oldTransform;
	rt->GetTransform(&oldTransform);

	rt->SetTransform(D2D1::Matrix3x2F::Scale(size / 1000.0f, size / 1000.0f));
	// Clear honors the clip, so only the chart square is cleared and the caller can fill the area around it
	rt->PushAxisAlignedClip(D2D1::RectF(0, 0, 1000, 1000), D2D1_ANTIALIAS_MODE_ALIASED);
	rt->Clear(m_params.BackColor);
	hr = DrawChart(rt);
	rt->PopAxisAlignedClip();
	rt->SetTransform(oldTransform);
	return hr;
}

HRESULT D2DChartDrawing::DrawChart(ID2D1RenderTarget* rt) {
	CComPtr<ID2D1Factory> factory;
	rt->GetFactory(&factory);

	CComPtr<ID2D1SolidColorBrush> brush;
	auto hr = rt->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &brush);
	if (FAILED(hr))
		return hr;

	// one brush serves the whole drawing; each use just switches its color
	auto use = [&](D2D1_COLOR_F const& color) {
		brush->SetColor(color);
		return brush.p;
	};
	auto drawGlyph = [&](WCHAR glyph, IDWriteTextFormat* format, D2D1_POINT_2F const& pt) {
		constexpr float half = 30;
		rt->DrawText(&glyph, 1, format, D2D1::RectF(pt.x - half, pt.y - half, pt.x + half, pt.y + half),
			use(Black), D2D1_DRAW_TEXT_OPTIONS_NONE);
	};

	auto& resources = D2DResources::Get();
	auto const& houses = m_data->Houses();
	D2D1_POINT_2F center = D2D1::Point2F(500, 500);

	//
	// draw ASC/MC line
	//
	rt->DrawLine(D2D1::Point2F(5, 500), D2D1::Point2F(995, 500), use(Black), 2);

	//
	// draw MC/IC line
	//
	rt->DrawLine(PointByAngle(center, 495, houses.MC), PointByAngle(center, 495, houses.MC.Opposite()), use(Black), 2);

	//
	// draw zodiac
	//
	float const outerRadius = 480;
	float const innerRadius = outerRadius - m_params.ZodiacBeltWidth;
	float const glyphRadius = (outerRadius + innerRadius) / 2;
	auto angle = (float)(houses.Asc.NextSign() + 120 + houses.Asc.DegreeInSign());

	for (int i = 0; i < 12; i++) {
		// each belt is a 30 degree annular sector
		CComPtr<ID2D1PathGeometry> geometry;
		hr = factory->CreatePathGeometry(&geometry);
		if (FAILED(hr))
			return hr;
		CComPtr<ID2D1GeometrySink> sink;
		hr = geometry->Open(&sink);
		if (FAILED(hr))
			return hr;
		sink->BeginFigure(PointOnCircle(center, outerRadius, angle), D2D1_FIGURE_BEGIN_FILLED);
		sink->AddArc(D2D1::ArcSegment(PointOnCircle(center, outerRadius, angle + 30), D2D1::SizeF(outerRadius, outerRadius), 0,
			D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
		sink->AddLine(PointOnCircle(center, innerRadius, angle + 30));
		sink->AddArc(D2D1::ArcSegment(PointOnCircle(center, innerRadius, angle), D2D1::SizeF(innerRadius, innerRadius), 0,
			D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
		sink->EndFigure(D2D1_FIGURE_END_CLOSED);
		hr = sink->Close();
		if (FAILED(hr))
			return hr;

		if (m_params.FillZodiacBelts)
			rt->FillGeometry(geometry, use(m_params.ElementColor[i % 4]));
		rt->DrawGeometry(geometry, use(Black), 1);

		drawGlyph(DefaultFont::Get().GetSignGlyph((ZodiacSign)i), resources.GlyphFormat(),PointOnCircle(center, glyphRadius, angle + 15));
		angle -= 30;
	}

	//
	// draw houses
	//
	if (m_params.DrawHouseLines) {
		for (int i = 0; i < 12; i++) {
			if (i % 3 == 0)
				continue;
			rt->DrawLine(center, PointByAngle(center, innerRadius, houses.Cusps[i]), use(Gray), 1);
		}
	}

	//
	// draw planets
	//
	float r = 420;
	PlanetSpacer spacer(m_data->AllPlanets());
	spacer.Space();

	for (auto& pp : spacer.NewPositions())
		drawGlyph(DefaultFont::Get().GetPlanetGlyph(pp.Planet), resources.PlanetFormat(),PointByAngle(center, r, pp.Longitude));

	r = 395;
	for (auto& pp : m_data->AllPlanets()) {
		auto pt = PointByAngle(center, r, pp.Longitude);
		rt->FillEllipse(D2D1::Ellipse(pt, 3, 3), use(Blue));
	}

	//
	// draw aspects
	//
	if (m_params.DrawAspects && m_aspects) {
		for (auto const& aspect : *m_aspects) {
			if (aspect.Type == AspectType::Conjunction)
				continue;

			if (!m_params.DrawVeryMinorAspects && (aspect.Type == AspectType::Septile || aspect.Type == AspectType::BiSeptile ||
				aspect.Type == AspectType::Quintile || aspect.Type == AspectType::BiQuintile))
				continue;

			if (!m_params.DrawNonStandardPlanetAspects && (aspect.Planet1.Planet > Planet::Pluto || aspect.Planet2.Planet > Planet::Pluto))
				continue;

			auto pt1 = PointByAngle(center, r, aspect.Planet1.Longitude);
			auto pt2 = PointByAngle(center, r, aspect.Planet2.Longitude);
			auto color(m_params.AspectColor);
			double width = m_params.MinorAspectWidth;
			if (aspect.IsMajor())
				width = m_params.MajorAspectWidth;
			else
				color = m_params.MinorAspectColor;
			if (aspect.IsSoft())
				color = m_params.SoftAspectColor;
			else if (aspect.IsHard())
				color = m_params.HardAspectColor;

			// the tighter the orb, the thicker the line
			auto maxWidth(width);
			width = 4 - aspect.Orb / 2;
			if (width > maxWidth)
				width = maxWidth;
			else if (width < .5f)
				width = .5f;
			rt->DrawLine(pt1, pt2, use(color), (float)width);
			drawGlyph(DefaultFont::Get().GetAspectGlyph(aspect.Type), resources.GlyphFormat(),
				D2D1::Point2F((pt1.x + pt2.x) / 2, (pt1.y + pt2.y) / 2));
		}
	}

	return S_OK;
}

D2DChartDrawing& D2DChartDrawing::DrawingParameters(ChartDrawingParameters const& params) {
	m_params = params;
	return *this;
}

ChartDrawingParameters const& D2DChartDrawing::DrawingParameters() const {
	return m_params;
}

ChartDrawingParameters& D2DChartDrawing::DrawingParameters() {
	return m_params;
}

D2DChartDrawing& D2DChartDrawing::Chart(ChartData* data) {
	m_data = data;
	return *this;
}

ChartData* D2DChartDrawing::Chart() const {
	return m_data;
}

D2DChartDrawing& D2DChartDrawing::Aspects(std::vector<AspectData>* aspects) {
	m_aspects = aspects;
	return *this;
}

D2D1_POINT_2F D2DChartDrawing::PointByAngle(D2D1_POINT_2F const& center, float radius, double angle) const {
	angle = Radians(angle - m_data->Houses().Asc + 180);
	return D2D1::Point2F(center.x + radius * (float)std::cos(angle), center.y - radius * (float)std::sin(angle));
}
