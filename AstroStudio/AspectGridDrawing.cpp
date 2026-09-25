#include "pch.h"
#include "AspectGridDrawing.h"
#include "DefaultFont.h"
#include <map>
#include <cmath>

HRESULT AspectGridDrawing::Draw(ID2D1RenderTarget* rt) {
	if (rt == nullptr || m_data == nullptr)
		return E_POINTER;

	auto hr = EnsureFormats();
	if (FAILED(hr))
		return hr;

	CComPtr<ID2D1SolidColorBrush> brush;
	hr = rt->CreateSolidColorBrush(m_params.AspectColor, &brush);
	if (FAILED(hr))
		return hr;

	// the planets along the top (the chart's) and down the side (the chart's, or the overlay's)
	auto const& planets = m_data->AllPlanets();
	auto const& rowPlanets = m_overlay ? m_overlay->Data.AllPlanets() : planets;
	int columns = (int)planets.size() + 1, rows = (int)rowPlanets.size() + 1;
	int cell = m_params.CellSize;

	rt->Clear(m_params.BackColor);

	// one brush serves the whole drawing; each use just switches its color
	auto use = [&](D2D1_COLOR_F const& color) {
		brush->SetColor(color);
		return brush.p;
	};
	// draws text centered on a point
	auto drawText = [&](PCWSTR text, UINT32 length, IDWriteTextFormat* format, D2D1_POINT_2F const& center, D2D1_COLOR_F const& color) {
		float half = cell / 2.0f;
		rt->DrawText(text, length, format, D2D1::RectF(center.x - half, center.y - half, center.x + half, center.y + half),
			use(color), D2D1_DRAW_TEXT_OPTIONS_NONE);
	};

	//
	// aspect lookup keyed by unordered planet pair, so it doesn't depend on
	// AspectCalculator::Calculate's iteration order
	//
	std::map<std::pair<Planet, Planet>, AspectData const*> lookup;
	if (m_overlay) {
		// the overlay's planet is Planet1 and the chart's Planet2; the same planet on both sides is a pair like any other, so
		// the order is kept
		for (auto const& a : m_overlay->Aspects)
			lookup[{a.Planet1.Planet, a.Planet2.Planet}] = &a;
	}
	else if (m_aspects) {
		for (auto const& a : *m_aspects) {
			auto p1 = a.Planet1.Planet, p2 = a.Planet2.Planet;
			if (p1 > p2)
				std::swap(p1, p2);
			lookup[{p1, p2}] = &a;
		}
	}

	// a 1px line is centered on the path it strokes, so lines sit at half-pixel positions to cover
	// exactly one pixel row/column; this also keeps the outer border inside the target
	const float margin = 0.5f;

	for (int row = 0; row < rows; row++) {
		for (int col = 0; col < columns; col++) {
			D2D1_RECT_F rc = D2D1::RectF(margin + col * cell, margin + row * cell, margin + (col + 1) * cell, margin + (row + 1) * cell);
			rt->DrawRectangle(rc, use(m_params.GridLineColor), 1);

			if (row == 0 && col == 0) {
				// with an overlay the corner says what the rows are
				if (m_overlay && !m_overlay->Label.empty())
					drawText(m_overlay->Label.c_str(), (UINT32)m_overlay->Label.size(), m_labelFormat,
						D2D1::Point2F(margin + cell / 2.0f, margin + cell / 2.0f), m_params.OverlayColor);
				continue;
			}

			D2D1_POINT_2F center = D2D1::Point2F(margin + col * cell + cell / 2.0f, margin + row * cell + cell / 2.0f);

			if (row == 0 || col == 0) {
				bool side = col == 0;		// the planets down the side may be the overlay's, in its colour
				WCHAR glyph = DefaultFont::Get().GetPlanetGlyph((side ? rowPlanets[row - 1] : planets[col - 1]).Planet);
				drawText(&glyph, 1, m_glyphFormat, center, side && m_overlay ? m_params.OverlayColor : m_params.AspectColor);
				continue;
			}
			if (!m_overlay && row == col)
				continue;	// no self-aspect; the mirrored pair is drawn on both sides of the diagonal

			auto p1 = rowPlanets[row - 1].Planet, p2 = planets[col - 1].Planet;
			auto key = m_overlay || p1 < p2 ? std::make_pair(p1, p2) : std::make_pair(p2, p1);
			auto it = lookup.find(key);
			if (it == lookup.end())
				continue;

			auto& aspect = *it->second;
			auto color(m_params.AspectColor);
			if (!aspect.IsMajor())
				color = m_params.MinorAspectColor;
			if (aspect.IsSoft())
				color = m_params.SoftAspectColor;
			else if (aspect.IsHard())
				color = m_params.HardAspectColor;

			WCHAR glyph = DefaultFont::Get().GetAspectGlyph(aspect.Type);
			drawText(&glyph, 1, m_glyphFormat, D2D1::Point2F(center.x, center.y - cell * 0.12f), color);

			CString info;
			info.Format(L"%d%c%c", (int)std::lround(aspect.Orb), 0xb0, aspect.Applying ? L'A' : L'S');
			drawText(info, info.GetLength(), m_infoFormat, D2D1::Point2F(center.x, center.y + cell * 0.32f), m_params.AspectColor);
		}
	}

	return S_OK;
}

HRESULT AspectGridDrawing::EnsureFormats() {
	auto& resources = D2DResources::Get();
	auto hr = resources.Ensure();
	if (FAILED(hr))
		return hr;
	if (m_glyphFormat && m_infoFormat && m_labelFormat && m_formatCellSize == m_params.CellSize && m_formatFamily == resources.TextFontFamily())
		return S_OK;

	// 0.375 and 0.22 of the cell size in points, in DIPs at 96 DPI
	CComPtr<IDWriteTextFormat> glyphFormat, infoFormat, labelFormat;
	hr = resources.CreateGlyphFormat(m_params.CellSize * 0.375f * 96 / 72, &glyphFormat);
	if (FAILED(hr))
		return hr;
	hr = resources.CreateTextFormat(m_params.CellSize * 0.22f * 96 / 72, &infoFormat);
	if (FAILED(hr))
		return hr;

	hr = resources.CreateTextFormat(m_params.CellSize * 0.15f * 96 / 72, &labelFormat);
	if (FAILED(hr))
		return hr;

	m_glyphFormat = glyphFormat;
	m_infoFormat = infoFormat;
	m_labelFormat = labelFormat;
	m_formatCellSize = m_params.CellSize;
	m_formatFamily = resources.TextFontFamily();
	return S_OK;
}

AspectGridDrawing& AspectGridDrawing::DrawingParameters(AspectGridDrawingParameters const& params) {
	m_params = params;
	return *this;
}

AspectGridDrawingParameters const& AspectGridDrawing::DrawingParameters() const {
	return m_params;
}

AspectGridDrawingParameters& AspectGridDrawing::DrawingParameters() {
	return m_params;
}

AspectGridDrawing& AspectGridDrawing::Chart(ChartData* data) {
	m_data = data;
	return *this;
}

ChartData* AspectGridDrawing::Chart() const {
	return m_data;
}

AspectGridDrawing& AspectGridDrawing::Aspects(std::vector<AspectData>* aspects) {
	m_aspects = aspects;
	return *this;
}

AspectGridDrawing& AspectGridDrawing::Overlay(ChartOverlay const* overlay) {
	m_overlay = overlay;
	return *this;
}

SIZE AspectGridDrawing::GridSize() const {
	int columns = m_data ? (int)m_data->AllPlanets().size() : 0;
	int rows = m_overlay ? (int)m_overlay->Data.AllPlanets().size() : columns;
	return { (columns + 1) * m_params.CellSize + 1, (rows + 1) * m_params.CellSize + 1 };	// +1 for the last grid line
}
