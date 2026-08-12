#include "pch.h"
#include "AspectGridDrawing.h"

#include "AstroFont.h"
#include "AstroHelpers.h"

#include <wx/graphics.h>
#include <cmath>
#include <map>

namespace {

	// See the note in ChartDrawing.cpp: the WTL fonts were Gdiplus::Font(...)
	// with the default UnitPoint.
	constexpr int PointsToPixels(double points) {
		return static_cast<int>(points * 96.0 / 72.0 + 0.5);
	}

} // namespace

bool AspectGridDrawing::Draw(wxDC& dc, wxPoint offset) {
	if (m_data == nullptr)
		return false;

	AstroHelpers::LoadAstroFont();

	auto const& planets = m_data->AllPlanets();
	auto n = static_cast<int>(planets.size());
	auto cell = m_params.CellSize;

	std::vector<AstroHelpers::GlyphRun> glyphs;
	auto glyphFont = AstroHelpers::GlyphFont(PointsToPixels(cell * 0.375));
	auto infoFont = wxFont(wxFontInfo(wxSize(0, PointsToPixels(cell * 0.22))));

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

	// a 1px margin keeps the outer border from being clipped: a pen is centered on the
	// path it strokes, so a line drawn right at the edge would have half its width
	// fall outside and effectively disappear
	const double margin = 1.0;

	{
		std::unique_ptr<wxGraphicsContext> gcPtr(
			wxGraphicsRenderer::GetGDIPlusRenderer()->CreateContextFromUnknownDC(dc));
		if (!gcPtr)
			return false;
		auto& gc = *gcPtr;

		gc.SetAntialiasMode(wxANTIALIAS_DEFAULT);
		gc.Translate(offset.x, offset.y);

		gc.SetPen(*wxTRANSPARENT_PEN);
		gc.SetBrush(wxBrush(m_params.BackColor));
		gc.DrawRectangle(0, 0, GridSize(), GridSize());

		auto gridPen = gc.CreatePen(wxGraphicsPenInfo(m_params.GridLineColor).Width(1));

		auto size = n + 1;
		for (int row = 0; row < size; row++) {
			for (int col = 0; col < size; col++) {
				gc.SetPen(gridPen);
				gc.SetBrush(*wxTRANSPARENT_BRUSH);
				gc.DrawRectangle(margin + col * cell, margin + row * cell, cell, cell);

				if (row == 0 && col == 0)
					continue;

				auto cx = margin + col * cell + cell / 2.0 + offset.x;
				auto cy = margin + row * cell + cell / 2.0 + offset.y;

				if (row == 0 || col == 0) {
					auto planet = planets[(row == 0 ? col : row) - 1].Planet;
					glyphs.push_back({ DefaultFont::Get().GetPlanetGlyphAsString(planet),
						cx, cy, glyphFont, m_params.AspectColor });
					continue;
				}

				if (row == col)
					continue;	// no self-aspect; the mirrored pair is drawn on both sides of the diagonal

				auto p1 = planets[row - 1].Planet, p2 = planets[col - 1].Planet;
				auto key = p1 < p2 ? std::make_pair(p1, p2) : std::make_pair(p2, p1);
				auto it = lookup.find(key);
				if (it == lookup.end())
					continue;

				auto const& aspect = *it->second;
				auto colour = m_params.AspectColor;
				if (!aspect.IsMajor())
					colour = m_params.MinorAspectColor;
				if (aspect.IsSoft())
					colour = m_params.SoftAspectColor;
				else if (aspect.IsHard())
					colour = m_params.HardAspectColor;

				glyphs.push_back({ DefaultFont::Get().GetAspectGlyphAsString(aspect.Type),
					cx, cy - cell * 0.12, glyphFont, colour });

				glyphs.push_back({ wxString::Format("%d°%s",
					static_cast<int>(std::lround(aspect.Orb)), aspect.Applying ? "A" : "S"),
					cx, cy + cell * 0.32, infoFont, m_params.AspectColor });
			}
		}
	}	// the context has to be destroyed before the DC is drawn on again

	AstroHelpers::DrawGlyphRuns(dc, glyphs);
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

AspectGridDrawing& AspectGridDrawing::Chart(ChartData const* data) {
	m_data = data;
	return *this;
}

ChartData const* AspectGridDrawing::Chart() const {
	return m_data;
}

AspectGridDrawing& AspectGridDrawing::Aspects(std::vector<AspectData> const* aspects) {
	m_aspects = aspects;
	return *this;
}

int AspectGridDrawing::GridSize() const {
	auto n = m_data ? static_cast<int>(m_data->AllPlanets().size()) : 0;
	return (n + 1) * m_params.CellSize + 2;	// +2 for the 1px margin on each side
}
