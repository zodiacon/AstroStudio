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

namespace {
	// how much the chart shrinks to make room for the transit band around it, and the band's edges
	constexpr float BiWheelScale = 0.87f;
	constexpr float TransitInnerRadius = 434, TransitOuterRadius = 496;
	constexpr float TransitDotRadius = 441, TransitGlyphRadius = 468;
}

D2D1_POINT_2F D2DChartDrawing::Map(D2D1_POINT_2F const& pt) const {
	auto center = D2DChartDrawing::Center;
	return D2D1::Point2F(center.x + m_scale * (pt.x - center.x), center.y + m_scale * (pt.y - center.y));
}

HRESULT D2DChartDrawing::DrawChart(ID2D1RenderTarget* rt) {
	m_planetSpots.clear();
	m_aspectLines.clear();
	m_transitSpots.clear();
	m_transitLines.clear();
	bool withTransits = m_transits != nullptr;
	m_scale = withTransits ? BiWheelScale : 1.0f;

	if (withTransits) {
		if (auto hr = DrawTransitBand(rt); FAILED(hr))
			return hr;
	}

	// the chart itself, smaller when there is a band to make room for
	D2D1_MATRIX_3X2_F transform;
	rt->GetTransform(&transform);
	if (withTransits)
		rt->SetTransform(D2D1::Matrix3x2F::Scale(m_scale, m_scale, D2DChartDrawing::Center) * transform);
	auto hr = DrawNatal(rt);
	rt->SetTransform(transform);
	if (FAILED(hr) || !withTransits)
		return hr;
	return DrawTransits(rt);
}

HRESULT D2DChartDrawing::DrawNatal(ID2D1RenderTarget* rt) {
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
	// draw ASC/DSC line
	//
	rt->DrawLine(PointByAngle(center, 495, houses.Asc), PointByAngle(center, 495, houses.Asc.Opposite()), use(Black), 2);

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
	auto angle = (float)(houses.Asc.NextSign() + 120 + houses.Asc.DegreeInSign() - m_rotation);

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

	auto const& planets = m_data->AllPlanets();
	for (auto& pp : spacer.NewPositions()) {
		auto pt = PointByAngle(center, r, pp.Longitude);
		drawGlyph(DefaultFont::Get().GetPlanetGlyph(pp.Planet), resources.PlanetFormat(), pt);
		if (auto it = std::find_if(planets.begin(), planets.end(), [&](auto& p) { return p.Planet == pp.Planet; }); it != planets.end())
			m_planetSpots.push_back({ Map(pt), int(it - planets.begin()) });
		if (m_highlight && !m_highlight->Transit && m_highlight->Planet == pp.Planet)
			rt->DrawEllipse(D2D1::Ellipse(pt, 22, 22), use(D2D1::ColorF(D2D1::ColorF::DarkOrange)), 3);
	}

	r = 395;
	for (auto& pp : planets) {
		auto pt = PointByAngle(center, r, pp.Longitude);
		bool highlighted = m_highlight && !m_highlight->Transit && m_highlight->Planet == pp.Planet;
		rt->FillEllipse(D2D1::Ellipse(pt, highlighted ? 6.f : 3.f, highlighted ? 6.f : 3.f), use(highlighted ? D2D1::ColorF(D2D1::ColorF::DarkOrange) : Blue));
	}

	//
	// draw aspects
	//
	if (m_params.DrawAspects && m_aspects && !m_transits) {
		for (int index = 0; index < (int)m_aspects->size(); index++) {
			auto const& aspect = (*m_aspects)[index];
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
			auto middle = D2D1::Point2F((pt1.x + pt2.x) / 2, (pt1.y + pt2.y) / 2);
			m_aspectLines.push_back({ Map(pt1), Map(pt2), Map(middle), index });

			if (m_highlight) {
				bool involved = !m_highlight->Transit && (aspect.Planet1.Planet == m_highlight->Planet || aspect.Planet2.Planet == m_highlight->Planet);
				if (!involved) {
					// not the highlighted planet's: a faint line, no glyph
					color.a = 0.12f;
					rt->DrawLine(pt1, pt2, use(color), (float)width);
					continue;
				}
				width += 1;
			}
			rt->DrawLine(pt1, pt2, use(color), (float)width);
			drawGlyph(DefaultFont::Get().GetAspectGlyph(aspect.Type), resources.GlyphFormat(), middle);
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

D2DChartDrawing& D2DChartDrawing::Rotation(double degrees) {
	m_rotation = degrees;
	return *this;
}

double D2DChartDrawing::Rotation() const {
	return m_rotation;
}

D2DChartDrawing& D2DChartDrawing::Highlight(std::optional<ChartSelection> planet) {
	m_highlight = planet;
	return *this;
}

std::optional<ChartSelection> D2DChartDrawing::Highlight() const {
	return m_highlight;
}

D2DChartDrawing& D2DChartDrawing::Transits(ChartData* data) {
	m_transits = data;
	return *this;
}

D2DChartDrawing& D2DChartDrawing::TransitAspects(std::vector<AspectData>* aspects) {
	m_transitAspects = aspects;
	return *this;
}

D2DChartDrawing& D2DChartDrawing::TransitCaption(std::wstring caption) {
	m_transitCaption = std::move(caption);
	return *this;
}

HRESULT D2DChartDrawing::DrawTransitBand(ID2D1RenderTarget* rt) {
	CComPtr<ID2D1SolidColorBrush> brush;
	if (auto hr = rt->CreateSolidColorBrush(Black, &brush); FAILED(hr))
		return hr;
	auto center = D2DChartDrawing::Center;
	brush->SetColor(m_params.TransitBandColor);
	rt->FillEllipse(D2D1::Ellipse(center, TransitOuterRadius, TransitOuterRadius), brush);
	brush->SetColor(m_params.BackColor);
	rt->FillEllipse(D2D1::Ellipse(center, TransitInnerRadius, TransitInnerRadius), brush);
	brush->SetColor(Gray);
	rt->DrawEllipse(D2D1::Ellipse(center, TransitOuterRadius, TransitOuterRadius), brush, 1);
	rt->DrawEllipse(D2D1::Ellipse(center, TransitInnerRadius, TransitInnerRadius), brush, 1);
	return S_OK;
}

HRESULT D2DChartDrawing::DrawTransits(ID2D1RenderTarget* rt) {
	CComPtr<ID2D1SolidColorBrush> brush;
	if (auto hr = rt->CreateSolidColorBrush(Black, &brush); FAILED(hr))
		return hr;
	auto use = [&](D2D1_COLOR_F const& color) {
		brush->SetColor(color);
		return brush.p;
	};
	auto& resources = D2DResources::Get();
	auto center = D2DChartDrawing::Center;
	auto const& planets = m_transits->AllPlanets();
	auto const& natal = m_data->AllPlanets();

	bool fade = m_highlight.has_value();

	//
	// the transiting planets: a dot at their place on the edge of the chart, and the glyph out in the band
	//
	PlanetSpacer spacer(planets);
	spacer.Space();
	for (auto& pp : spacer.NewPositions()) {
		auto pt = PointByAngle(center, TransitGlyphRadius, pp.Longitude);
		constexpr float half = 30;
		WCHAR glyph = DefaultFont::Get().GetPlanetGlyph(pp.Planet);
		rt->DrawText(&glyph, 1, resources.PlanetFormat(), D2D1::RectF(pt.x - half, pt.y - half, pt.x + half, pt.y + half),
			use(m_params.TransitColor), D2D1_DRAW_TEXT_OPTIONS_NONE);
		if (auto it = std::find_if(planets.begin(), planets.end(), [&](auto& p) { return p.Planet == pp.Planet; }); it != planets.end())
			m_transitSpots.push_back({ pt, int(it - planets.begin()) });
		if (m_highlight && m_highlight->Transit && m_highlight->Planet == pp.Planet)
			rt->DrawEllipse(D2D1::Ellipse(pt, 22, 22), use(D2D1::ColorF(D2D1::ColorF::DarkOrange)), 3);
	}
	for (auto& pp : planets) {
		bool highlighted = m_highlight && m_highlight->Transit && m_highlight->Planet == pp.Planet;
		rt->FillEllipse(D2D1::Ellipse(PointByAngle(center, TransitDotRadius, pp.Longitude), highlighted ? 6.f : 3.5f, highlighted ? 6.f : 3.5f),
			use(highlighted ? D2D1::ColorF(D2D1::ColorF::DarkOrange) : m_params.TransitColor));
	}

	//
	// the aspects from the transiting planets to the chart's planets
	//
	if (m_params.DrawAspects && m_transitAspects) {
		float const natalDotRadius = 395 * m_scale;
		for (int index = 0; index < (int)m_transitAspects->size(); index++) {
			auto const& aspect = (*m_transitAspects)[index];
			if (aspect.Type == AspectType::Conjunction)
				continue;
			if (!m_params.DrawNonStandardPlanetAspects && (aspect.Planet1.Planet > Planet::Pluto || aspect.Planet2.Planet > Planet::Pluto))
				continue;

			auto pt1 = PointByAngle(center, TransitDotRadius, aspect.Planet1.Longitude);
			auto pt2 = PointByAngle(center, natalDotRadius, aspect.Planet2.Longitude);
			auto color(m_params.AspectColor);
			if (aspect.IsSoft())
				color = m_params.SoftAspectColor;
			else if (aspect.IsHard())
				color = m_params.HardAspectColor;
			else
				color = m_params.MinorAspectColor;

			// the tighter the orb, the thicker the line
			float width = std::clamp(4.0f - aspect.Orb / 2, 0.75f, 3.0f);
			auto middle = D2D1::Point2F((pt1.x + pt2.x) / 2, (pt1.y + pt2.y) / 2);
			m_transitLines.push_back({ pt1, pt2, middle, index });

			bool involved = !fade;
			if (m_highlight)
				involved = m_highlight->Transit ? aspect.Planet1.Planet == m_highlight->Planet : aspect.Planet2.Planet == m_highlight->Planet;
			if (!involved) {
				color.a = 0.12f;
				rt->DrawLine(pt1, pt2, use(color), width);
				continue;
			}
			if (fade)
				width += 1;
			rt->DrawLine(pt1, pt2, use(color), width);
			if (fade) {
				WCHAR glyph = DefaultFont::Get().GetAspectGlyph(aspect.Type);
				rt->DrawText(&glyph, 1, resources.GlyphFormat(), D2D1::RectF(middle.x - 30, middle.y - 30, middle.x + 30, middle.y + 30),
					use(Black), D2D1_DRAW_TEXT_OPTIONS_NONE);
			}
		}
	}

	//
	// when the transits are for
	//
	if (!m_transitCaption.empty()) {
		CComPtr<IDWriteTextFormat> format;
		if (SUCCEEDED(resources.CreateTextFormat(22, &format))) {
			format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
			format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_FAR);
			rt->DrawText(m_transitCaption.c_str(), (UINT32)m_transitCaption.size(), format, D2D1::RectF(10, 900, 990, 992), use(m_params.TransitColor));
		}
	}
	return S_OK;
}

ChartHit D2DChartDrawing::HitTest(D2D1_POINT_2F const& point) const {
	auto distance = [](D2D1_POINT_2F const& a, D2D1_POINT_2F const& b) {
		return std::hypot(a.x - b.x, a.y - b.y);
	};

	// a planet's glyph is about 40 units wide
	constexpr float planetRadius = 22;
	ChartHit hit;
	float best = planetRadius;
	for (auto const& spot : m_planetSpots)
		if (auto d = distance(point, spot.Point); d <= best) {
			best = d;
			hit = { ChartHit::Kind::Planet, spot.Index };
		}
	for (auto const& spot : m_transitSpots)
		if (auto d = distance(point, spot.Point); d <= best) {
			best = d;
			hit = { ChartHit::Kind::TransitPlanet, spot.Index };
		}
	if (hit.Type != ChartHit::Kind::None)
		return hit;

	// an aspect: on its line, or on the glyph in the middle of it
	constexpr float lineReach = 6, glyphReach = 16;
	best = lineReach + 1;
	auto testLines = [&](std::vector<AspectLine> const& lines, ChartHit::Kind kind) {
	for (auto const& line : lines) {
		float d;
		if (distance(point, line.Middle) <= glyphReach)
			d = 0;
		else {
			float dx = line.To.x - line.From.x, dy = line.To.y - line.From.y;
			float lengthSquared = dx * dx + dy * dy;
			float t = lengthSquared > 0 ? ((point.x - line.From.x) * dx + (point.y - line.From.y) * dy) / lengthSquared : 0;
			t = std::clamp(t, 0.f, 1.f);
			d = distance(point, D2D1::Point2F(line.From.x + t * dx, line.From.y + t * dy));
		}
		if (d <= lineReach && d < best) {
			best = d;
			hit = { kind, line.Index };
		}
	}
	};
	testLines(m_aspectLines, ChartHit::Kind::Aspect);
	testLines(m_transitLines, ChartHit::Kind::TransitAspect);
	return hit;
}

D2D1_POINT_2F D2DChartDrawing::PointByAngle(D2D1_POINT_2F const& center, float radius, double angle) const {
	angle = Radians(angle - m_data->Houses().Asc + 180 + m_rotation);
	return D2D1::Point2F(center.x + radius * (float)std::cos(angle), center.y - radius * (float)std::sin(angle));
}
