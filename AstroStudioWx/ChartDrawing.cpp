#include "pch.h"
#include "ChartDrawing.h"

#include "AstroFont.h"
#include "AstroHelpers.h"
#include "PlanetSpacer.h"

#include <wx/graphics.h>
#include <cmath>

namespace {

	constexpr double PI = 3.14159265358979323846;

	constexpr double Rad(double degrees) {
		return degrees * PI / 180;
	}

	//
	// The WTL build created its fonts as Gdiplus::Font(family, N), and that
	// constructor defaults to UnitPoint. Everything was then scaled by the
	// 1000-unit transform, so the effective glyph size in logical units is
	// N points at the 96 dpi the original implicitly assumed. Converting here
	// keeps the wheel identical rather than 25% smaller.
	//
	constexpr double PointsToLogical(double points) {
		return points * 96.0 / 72.0;
	}

} // namespace

bool ChartDrawing::Draw(wxDC& dc, int size) {
	if (m_data == nullptr)
		return false;

	AstroHelpers::LoadAstroFont();

	auto const scale = size / 1000.0;
	auto const& houses = m_data->Houses();
	const wxPoint2DDouble center(500, 500);

	// Glyphs are collected here and drawn through the DC once the graphics
	// context has been flushed - see AstroHelpers::GlyphRun for why.
	std::vector<AstroHelpers::GlyphRun> glyphs;
	auto signFont = AstroHelpers::GlyphFont(wxRound(PointsToLogical(17) * scale));
	auto planetFont = AstroHelpers::GlyphFont(wxRound(PointsToLogical(20) * scale));

	auto addGlyph = [&](wxString const& text, wxPoint2DDouble const& at,
		wxFont const& font, wxColour const& colour) {
			glyphs.push_back({ text, at.m_x * scale, at.m_y * scale, font, colour });
		};

	{
		// CreateContextFromUnknownDC rather than CreateContext: the caller's DC
		// is a wxAutoBufferedPaintDC, whose concrete base varies by platform.
		std::unique_ptr<wxGraphicsContext> gcPtr(
			wxGraphicsRenderer::GetGDIPlusRenderer()->CreateContextFromUnknownDC(dc));
		if (!gcPtr)
			return false;
		auto& gc = *gcPtr;

		gc.SetAntialiasMode(wxANTIALIAS_DEFAULT);

		// Everything below is expressed in a 1000x1000 space, exactly as the
		// WTL version was; this is the Matrix::Scale it applied.
		gc.Scale(scale, scale);

		gc.SetPen(*wxTRANSPARENT_PEN);
		gc.SetBrush(wxBrush(m_params.BackColor));
		gc.DrawRectangle(0, 0, 1000, 1000);

		auto forePen = gc.CreatePen(wxGraphicsPenInfo(m_params.ForeColor).Width(2));
		auto thinPen = gc.CreatePen(wxGraphicsPenInfo(m_params.ForeColor).Width(1));

		//
		// ASC/DSC line
		//
		gc.SetPen(forePen);
		gc.StrokeLine(5, 500, 995, 500);

		//
		// MC/IC line
		//
		auto mc = PointByAngle(center, 495, houses.MC);
		auto ic = PointByAngle(center, 495, houses.MC.Opposite());
		gc.StrokeLine(mc.m_x, mc.m_y, ic.m_x, ic.m_y);

		//
		// Zodiac belt.
		//
		// The WTL code filled a full pie per sign and relied on
		// SetClip(innerEllipse, CombineModeExclude) to keep only the annulus.
		// wxGraphicsContext has no subtractive clipping, so each sign is built
		// as an explicit ring sector instead: out along the outer arc, in, back
		// along the inner arc. That is fewer operations than the original and
		// needs no clip state at all.
		//
		// wxGraphicsPath::AddArc takes radians and shares GDI+'s screen
		// convention - angle 0 at 3 o'clock, increasing angle sweeping
		// clockwise because y grows downwards - so the angles carry over
		// unchanged.
		//
		const double outerRadius = 480;										// outerRect(20,20,960,960)
		const double innerRadius = outerRadius - m_params.ZodiacBeltWidth;	// inflate(-40)
		const double glyphRadius = (innerRadius + outerRadius) / 2;

		double angle = houses.Asc.NextSign() + 120 + houses.Asc.DegreeInSign();

		for (int i = 0; i < 12; i++) {
			auto start = Rad(angle);
			auto end = Rad(angle + 30);

			auto sector = gc.CreatePath();
			sector.AddArc(center.m_x, center.m_y, outerRadius, start, end, true);
			sector.AddArc(center.m_x, center.m_y, innerRadius, end, start, false);
			sector.CloseSubpath();

			if (m_params.FillZodiacBelts) {
				gc.SetBrush(wxBrush(m_params.ElementColor[i % 4]));
				gc.FillPath(sector);
			}
			gc.SetPen(thinPen);
			gc.StrokePath(sector);

			// Step to the middle of the sign for the glyph, then back by 45 - a
			// net -30 per sign, i.e. the signs run anticlockwise on screen.
			angle += 15;
			addGlyph(DefaultFont::Get().GetSignGlyphAsString(static_cast<ZodiacSign>(i)),
				wxPoint2DDouble(500 + glyphRadius * std::cos(Rad(angle)),
					500 + glyphRadius * std::sin(Rad(angle))),
				signFont, m_params.ForeColor);
			angle -= 45;
		}

		gc.SetPen(thinPen);
		gc.SetBrush(*wxTRANSPARENT_BRUSH);
		gc.DrawEllipse(center.m_x - innerRadius, center.m_y - innerRadius,
			innerRadius * 2, innerRadius * 2);

		//
		// House cusps
		//
		if (m_params.DrawHouseLines) {
			gc.SetPen(gc.CreatePen(wxGraphicsPenInfo(m_params.HouseLineColor).Width(1)));
			for (int i = 0; i < 12; i++) {
				if (i % 3 == 0)
					continue;	// the angles are already drawn as the ASC/MC lines
				auto p = PointByAngle(center, innerRadius, houses.Cusps[i]);
				gc.StrokeLine(center.m_x, center.m_y, p.m_x, p.m_y);
			}
		}

		//
		// Planets, nudged apart so clustered glyphs stay legible.
		//
		PlanetSpacer spacer(m_data->AllPlanets());
		spacer.Space();
		for (auto const& pp : spacer.NewPositions())
			addGlyph(DefaultFont::Get().GetPlanetGlyphAsString(pp.Planet),
				PointByAngle(center, 420, pp.Longitude), planetFont, m_params.ForeColor);

		// True (unspaced) positions, marked on the aspect ring.
		const double aspectRadius = 395;
		gc.SetPen(*wxTRANSPARENT_PEN);
		gc.SetBrush(wxBrush(m_params.PlanetDotColor));
		for (auto const& pp : m_data->AllPlanets()) {
			auto pt = PointByAngle(center, aspectRadius, pp.Longitude);
			gc.DrawEllipse(pt.m_x - 3, pt.m_y - 3, 6, 6);
		}

		//
		// Aspects
		//
		if (m_params.DrawAspects && m_aspects) {
			for (auto const& aspect : *m_aspects) {
				if (aspect.Type == AspectType::Conjunction)
					continue;

				if (!m_params.DrawVeryMinorAspects &&
					(aspect.Type == AspectType::Septile || aspect.Type == AspectType::BiSeptile ||
						aspect.Type == AspectType::Quintile || aspect.Type == AspectType::BiQuintile))
					continue;

				if (!m_params.DrawNonStandardPlanetAspects &&
					(aspect.Planet1.Planet > Planet::Pluto || aspect.Planet2.Planet > Planet::Pluto))
					continue;

				auto pt1 = PointByAngle(center, aspectRadius, aspect.Planet1.Longitude);
				auto pt2 = PointByAngle(center, aspectRadius, aspect.Planet2.Longitude);

				auto colour = m_params.AspectColor;
				double maxWidth = m_params.MinorAspectWidth;
				if (aspect.IsMajor())
					maxWidth = m_params.MajorAspectWidth;
				else
					colour = m_params.MinorAspectColor;

				if (aspect.IsSoft())
					colour = m_params.SoftAspectColor;
				else if (aspect.IsHard())
					colour = m_params.HardAspectColor;

				// Tighter orbs draw heavier, capped at the type's own width.
				auto width = std::clamp(4 - aspect.Orb / 2.0, 0.5, maxWidth);

				gc.SetPen(gc.CreatePen(wxGraphicsPenInfo(colour).Width(width)));
				gc.StrokeLine(pt1.m_x, pt1.m_y, pt2.m_x, pt2.m_y);

				addGlyph(DefaultFont::Get().GetAspectGlyphAsString(aspect.Type),
					wxPoint2DDouble((pt1.m_x + pt2.m_x) / 2, (pt1.m_y + pt2.m_y) / 2),
					signFont, m_params.ForeColor);
			}
		}
	}	// the context has to be destroyed before the DC is drawn on again

	AstroHelpers::DrawGlyphRuns(dc, glyphs);
	return true;
}

//
// Maps a zodiac longitude onto the wheel: rotated so the ascendant sits at the
// left, and anticlockwise. Note this uses the mathematical convention (y up,
// hence the minus) while the zodiac belt above uses the screen convention -
// the same split the WTL original had.
//
wxPoint2DDouble ChartDrawing::PointByAngle(wxPoint2DDouble const& center, double radius, double angle) const {
	auto a = Rad(angle - m_data->Houses().Asc + 180);
	return wxPoint2DDouble(center.m_x + radius * std::cos(a), center.m_y - radius * std::sin(a));
}

ChartDrawing& ChartDrawing::DrawingParameters(ChartDrawingParameters const& params) {
	m_params = params;
	return *this;
}

ChartDrawingParameters const& ChartDrawing::DrawingParameters() const {
	return m_params;
}

ChartDrawingParameters& ChartDrawing::DrawingParameters() {
	return m_params;
}

ChartDrawing& ChartDrawing::Chart(ChartData const* data) {
	m_data = data;
	return *this;
}

ChartData const* ChartDrawing::Chart() const {
	return m_data;
}

ChartDrawing& ChartDrawing::Aspects(std::vector<AspectData> const* aspects) {
	m_aspects = aspects;
	return *this;
}
