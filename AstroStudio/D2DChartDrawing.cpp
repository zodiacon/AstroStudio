#include "pch.h"
#include "D2DChartDrawing.h"
#include <cmath>
#include <numbers>
#include "PlanetSpacer.h"
#include "DefaultFont.h"
#include "resource.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace {
	// ChartDrawing sizes its fonts in points (17 and 20); these are the same sizes in DIPs at 96 DPI
	constexpr float GlyphFontSize = 17 * 96.0f / 72;
	constexpr float PlanetFontSize = 20 * 96.0f / 72;

	constexpr double Radians(double degrees) {
		return degrees * std::numbers::pi / 180;
	}

	D2D1_COLOR_F ToColorF(Gdiplus::Color const& color) {
		return D2D1::ColorF(color.GetR() / 255.0f, color.GetG() / 255.0f, color.GetB() / 255.0f, color.GetA() / 255.0f);
	}

	// point on a circle, angle in degrees measured clockwise from the positive X axis (GDI+ pie convention)
	D2D1_POINT_2F PointOnCircle(D2D1_POINT_2F const& center, float radius, double degrees) {
		auto rad = Radians(degrees);
		return D2D1::Point2F(center.x + radius * (float)std::cos(rad), center.y + radius * (float)std::sin(rad));
	}
}

D2DChartDrawing::~D2DChartDrawing() {
	if (m_dwFactory && m_fontLoader)
		m_dwFactory->UnregisterFontFileLoader(m_fontLoader);
}

HRESULT D2DChartDrawing::Draw(ID2D1RenderTarget* rt, float size) {
	if (rt == nullptr || m_data == nullptr)
		return E_POINTER;

	auto hr = EnsureTextResources();
	if (FAILED(hr))
		return hr;

	D2D1_MATRIX_3X2_F oldTransform;
	rt->GetTransform(&oldTransform);

	rt->SetTransform(D2D1::Matrix3x2F::Scale(size / 1000.0f, size / 1000.0f));
	// Clear honors the clip, so only the chart square is cleared and the caller can fill the area around it
	rt->PushAxisAlignedClip(D2D1::RectF(0, 0, 1000, 1000), D2D1_ANTIALIAS_MODE_ALIASED);
	rt->Clear(ToColorF(m_params.BackColor));
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
	auto use = [&](Gdiplus::Color const& color) {
		brush->SetColor(ToColorF(color));
		return brush.p;
	};
	auto drawGlyph = [&](WCHAR glyph, IDWriteTextFormat* format, D2D1_POINT_2F const& pt) {
		constexpr float half = 30;
		rt->DrawText(&glyph, 1, format, D2D1::RectF(pt.x - half, pt.y - half, pt.x + half, pt.y + half),
			use(Gdiplus::Color::Black), D2D1_DRAW_TEXT_OPTIONS_NONE);
	};

	auto const& houses = m_data->Houses();
	D2D1_POINT_2F center = D2D1::Point2F(500, 500);

	//
	// draw ASC/MC line
	//
	rt->DrawLine(D2D1::Point2F(5, 500), D2D1::Point2F(995, 500), use(Gdiplus::Color::Black), 2);

	//
	// draw MC/IC line
	//
	rt->DrawLine(PointByAngle(center, 495, houses.MC), PointByAngle(center, 495, houses.MC.Opposite()), use(Gdiplus::Color::Black), 2);

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
		rt->DrawGeometry(geometry, use(Gdiplus::Color::Black), 1);

		drawGlyph(DefaultFont::Get().GetSignGlyph((ZodiacSign)i), m_glyphFormat, PointOnCircle(center, glyphRadius, angle + 15));
		angle -= 30;
	}

	//
	// draw houses
	//
	if (m_params.DrawHouseLines) {
		for (int i = 0; i < 12; i++) {
			if (i % 3 == 0)
				continue;
			rt->DrawLine(center, PointByAngle(center, innerRadius, houses.Cusps[i]), use(Gdiplus::Color::Gray), 1);
		}
	}

	//
	// draw planets
	//
	float r = 420;
	PlanetSpacer spacer(m_data->AllPlanets());
	spacer.Space();

	for (auto& pp : spacer.NewPositions())
		drawGlyph(DefaultFont::Get().GetPlanetGlyph(pp.Planet), m_planetFormat, PointByAngle(center, r, pp.Longitude));

	r = 395;
	for (auto& pp : m_data->AllPlanets()) {
		auto pt = PointByAngle(center, r, pp.Longitude);
		rt->FillEllipse(D2D1::Ellipse(pt, 3, 3), use(Gdiplus::Color::Blue));
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
			drawGlyph(DefaultFont::Get().GetAspectGlyph(aspect.Type), m_glyphFormat,
				D2D1::Point2F((pt1.x + pt2.x) / 2, (pt1.y + pt2.y) / 2));
		}
	}

	return S_OK;
}

HRESULT D2DChartDrawing::EnsureTextResources() {
	if (m_glyphFormat && m_planetFormat)
		return S_OK;

	HRESULT hr = S_OK;
	if (!m_dwFactory) {
		hr = ::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory5), reinterpret_cast<IUnknown**>(&m_dwFactory));
		if (FAILED(hr))
			return hr;
	}
	if (!m_fontCollection) {
		hr = LoadAstroFontCollection();
		if (FAILED(hr))
			return hr;
	}

	CComPtr<IDWriteFontFamily> family;
	hr = m_fontCollection->GetFontFamily(0, &family);
	if (FAILED(hr))
		return hr;
	CComPtr<IDWriteLocalizedStrings> names;
	hr = family->GetFamilyNames(&names);
	if (FAILED(hr))
		return hr;
	UINT32 length = 0;
	hr = names->GetStringLength(0, &length);
	if (FAILED(hr))
		return hr;
	std::wstring familyName(length, L'\0');
	hr = names->GetString(0, familyName.data(), length + 1);
	if (FAILED(hr))
		return hr;

	auto createFormat = [&](float size, IDWriteTextFormat** format) {
		hr = m_dwFactory->CreateTextFormat(familyName.c_str(), m_fontCollection, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", format);
		if (SUCCEEDED(hr))
			hr = (*format)->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
		if (SUCCEEDED(hr))
			hr = (*format)->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
		return hr;
	};

	CComPtr<IDWriteTextFormat> glyphFormat, planetFormat;
	hr = createFormat(GlyphFontSize, &glyphFormat);
	if (FAILED(hr))
		return hr;
	hr = createFormat(PlanetFontSize, &planetFormat);
	if (FAILED(hr))
		return hr;

	m_glyphFormat = glyphFormat;
	m_planetFormat = planetFormat;
	return S_OK;
}

// DirectWrite cannot see the font GDI registered with AddFontMemResourceEx, and neither can GDI+ (see
// Helpers::LoadAstroFont), so it gets its own collection built from the same embedded resource.
HRESULT D2DChartDrawing::LoadAstroFontCollection() {
	auto res = ::FindResource(nullptr, MAKEINTRESOURCE(IDR_FONT), L"TTF");
	if (!res)
		return HRESULT_FROM_WIN32(::GetLastError());
	auto hGlobal = ::LoadResource(nullptr, res);
	if (!hGlobal)
		return HRESULT_FROM_WIN32(::GetLastError());
	auto size = ::SizeofResource(nullptr, res);
	auto data = ::LockResource(hGlobal);
	if (!data)
		return E_FAIL;

	CComPtr<IDWriteInMemoryFontFileLoader> loader;
	auto hr = m_dwFactory->CreateInMemoryFontFileLoader(&loader);
	if (FAILED(hr))
		return hr;
	hr = m_dwFactory->RegisterFontFileLoader(loader);
	if (FAILED(hr))
		return hr;

	// the resource lives as long as the module, so the loader needs no owner object to keep the data alive
	CComPtr<IDWriteFontFile> file;
	CComPtr<IDWriteFontSetBuilder1> builder;
	CComPtr<IDWriteFontSet> fontSet;
	CComPtr<IDWriteFontCollection1> collection;
	hr = loader->CreateInMemoryFontFileReference(m_dwFactory, data, size, nullptr, &file);
	if (SUCCEEDED(hr))
		hr = m_dwFactory->CreateFontSetBuilder(&builder);
	if (SUCCEEDED(hr))
		hr = builder->AddFontFile(file);
	if (SUCCEEDED(hr))
		hr = builder->CreateFontSet(&fontSet);
	if (SUCCEEDED(hr))
		hr = m_dwFactory->CreateFontCollectionFromFontSet(fontSet, &collection);

	if (FAILED(hr)) {
		m_dwFactory->UnregisterFontFileLoader(loader);
		return hr;
	}
	m_fontLoader = loader;
	m_fontCollection = collection;
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
