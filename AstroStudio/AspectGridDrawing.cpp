#include "pch.h"
#include "AspectGridDrawing.h"
#include "Helpers.h"
#include "DefaultFont.h"
#include "resource.h"
#include <map>
#include <cmath>

using namespace Gdiplus;

bool AspectGridDrawing::Draw(Gdiplus::Graphics& g) {
	if (m_data == nullptr)
		return false;

	auto const& planets = m_data->AllPlanets();
	int n = (int)planets.size();
	int cell = m_params.CellSize;

	g.Clear(m_params.BackColor);
	g.SetSmoothingMode(SmoothingModeAntiAlias);
	g.SetTextRenderingHint(TextRenderingHintAntiAlias);

	Pen gridPen(m_params.GridLineColor, 1);
	SolidBrush blackBrush(m_params.AspectColor);
	StringFormat format;
	format.SetAlignment(StringAlignmentCenter);
	format.SetLineAlignment(StringAlignmentCenter);
	Font font(&Helpers::GetAstroFontFamily(IDR_FONT), cell * 0.375f);
	Font infoFont(FontFamily::GenericSansSerif(), cell * 0.22f);

	//
	// aspect lookup keyed by unordered planet pair, so it doesn't depend on
	// AspectCalculator::Calculate's iteration order
	//
	std::map<std::pair<Planet, Planet>, AspectData const*> lookup;
	if (m_aspects) {
		for (auto const& a : *m_aspects) {
			auto p1 = a.Planet1.Planet, p2 = a.Planet2.Planet;
			if (p1 > p2)
				std::swap(p1, p2);
			lookup[{p1, p2}] = &a;
		}
	}

	int size = n + 1;
	for (int row = 0; row < size; row++) {
		for (int col = 0; col < size; col++) {
			RectF rc((float)(col * cell), (float)(row * cell), (float)cell, (float)cell);
			g.DrawRectangle(&gridPen, rc);

			if (row == 0 && col == 0)
				continue;

			PointF center((float)(col * cell) + cell / 2.0f, (float)(row * cell) + cell / 2.0f);

			if (row == 0) {
				auto glyph = DefaultFont::Get().GetPlanetGlyphAsString(planets[col - 1].Planet);
				g.DrawString(glyph, 1, &font, center, &format, &blackBrush);
				continue;
			}
			if (col == 0) {
				auto glyph = DefaultFont::Get().GetPlanetGlyphAsString(planets[row - 1].Planet);
				g.DrawString(glyph, 1, &font, center, &format, &blackBrush);
				continue;
			}
			if (row <= col)
				continue;	// at/above the diagonal: each pair is shown once, in the lower triangle

			auto p1 = planets[row - 1].Planet, p2 = planets[col - 1].Planet;
			auto key = p1 < p2 ? std::make_pair(p1, p2) : std::make_pair(p2, p1);
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

			SolidBrush brush(color);
			auto glyph = DefaultFont::Get().GetAspectGlyphAsString(aspect.Type);
			PointF glyphCenter(center.X, center.Y - cell * 0.12f);
			g.DrawString(glyph, 1, &font, glyphCenter, &format, &brush);

			CString info;
			info.Format(L"%d%c%c", (int)std::lround(aspect.Orb), 0xb0, aspect.Applying ? L'A' : L'S');
			PointF infoCenter(center.X, center.Y + cell * 0.32f);
			g.DrawString(info, info.GetLength(), &infoFont, infoCenter, &format, &blackBrush);
		}
	}

	return true;
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

int AspectGridDrawing::GridSize() const {
	int n = m_data ? (int)m_data->AllPlanets().size() : 0;
	return (n + 1) * m_params.CellSize;
}
