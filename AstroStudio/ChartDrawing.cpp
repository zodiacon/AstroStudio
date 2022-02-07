#include "pch.h"
#include "ChartDrawing.h"
#include <cmath>
#include "Helpers.h"
#include "PlanetSpacer.h"
#include "DefaultFont.h"

bool ChartDrawing::Draw(CairoSurface& surface) {
	auto scale = surface.Width() / 1000.0;
	CairoCtx ctx(surface);
	ctx.Scale(scale, scale);
	ctx.Clear(m_params.BackColor);

	double startAngle = m_data.Houses().Asc.NextSign() + 150 + m_data.Houses().Asc.DegreeInSign();
	ctx.FontSize(24);
	char text[] = "asdfghjklzxc";
	CFont font;
	font.CreatePointFont(100, L"HamburgSymbols");
	CairoFont cfont(font);
	ctx.FontFace(cfont);

	//
	// draw asc/dsc line
	//
	ctx.MoveTo(5, 500).LineWidth(2).SourceColor(StandardColors::Black).LineTo(995, 500).Stroke();

	//
	// draw MC/IC line
	//
	ctx.MoveTo(500, 500).LineWidth(1).SourceColor(StandardColors::Black);
	ctx.LineTo(PointByAngle(CairoPoint(500, 500), 495, m_data.Houses().MC)).Stroke();
	ctx.MoveTo(500, 500).LineTo(PointByAngle(CairoPoint(500, 500), 495, m_data.Houses().MC.Opposite())).Stroke();

	//
	// draw zodiac
	//
	for (int i = 0; i < 12; i++) {
		auto angle = startAngle + 30 * i;
		ctx.Arc(500, 500, 470, Rad(angle), Rad(angle + 30)).
			Arc(500, 500, 430, Rad(angle + 30), Rad(angle), true).
			ClosePath().
			SourceColor(m_params.ElementColor[i % 4]).Fill(true).
			SourceColor(StandardColors::Black).LineWidth(1).Stroke(true);
		auto extents = ctx.FillExtents();
		angle += 15;
		double x = 500 + 450 * std::cos(Rad(angle));
		double y = 500 + 450 * std::sin(Rad(angle));
		char sign[] = "a";
		sign[0] = text[11 - i];
		auto ext = ctx.TextExtents(sign);
		ctx.MoveTo(x - ext.width / 2, y + ext.width / 2).ShowText(sign);
		ctx.NewPath();
	}

	//
	// draw houses
	//
	if (m_params.DrawHouseLines) {
		ctx.LineWidth(1).SourceColor(StandardColors::Gray);
		for (int i = 0; i < 12; i++) {
			if (i % 3 == 0)
				continue;
			ctx.MoveTo(500, 500);
			ctx.LineTo(PointByAngle(CairoPoint(500, 500), 430, m_data.Houses().Cusps[i])).Stroke();
		}
	}

	//
	// draw planets
	//
	double r = 395;
	auto asc = m_data.Houses().Asc;
	PlanetSpacer spacer(m_data.AllPlanets());
	spacer.Space(5);

	ctx.FontSize(30);
	for(auto& pp : spacer.NewPositions()) {
		auto pt = PointByAngle(CairoPoint(500, 500), r, pp.Longitude);
		auto x = pt.X, y = pt.Y;
		CStringA glyph(DefaultFont::Get().GetPlanetGlyphAsString(pp.Planet));
		auto ext = ctx.TextExtents(glyph);
		ctx.MoveTo(x - ext.width / 2, y + ext.width / 2).SourceColor(StandardColors::Black).ShowText(glyph);
		ctx.NewPath();
	}

	r = 370;
	for (auto& pp : m_data.AllPlanets()) {
		auto pt = PointByAngle(CairoPoint(500, 500), r, pp.Longitude);
		ctx.Circle(pt.X, pt.Y, 4).SourceColor(StandardColors::DarkBlue).Fill();
	}

	//
	// draw aspects
	//
	if (m_params.DrawAspects) {
		for (auto& aspect : m_aspects) {
			if (aspect.Type == AspectType::Conjunction)
				continue;

			if (!m_params.DrawVeryMinorAspects && (aspect.Type == AspectType::Septile || aspect.Type == AspectType::BiSeptile ||
				aspect.Type == AspectType::Quintile || aspect.Type == AspectType::BiQuintile))
				continue;

			if (!m_params.DrawNonStandardPlanetAspects && (aspect.Planet1.Planet > PlanetType::Pluto || aspect.Planet2.Planet > PlanetType::Pluto))
				continue;

			auto pt1 = PointByAngle(CairoPoint(500, 500), r, aspect.Planet1.Longitude);
			auto pt2 = PointByAngle(CairoPoint(500, 500), r, aspect.Planet2.Longitude);
			CairoColor color(m_params.AspectColor);
			double width = m_params.MinorAspectWidth;
			if (aspect.IsMajor())
				width = m_params.MajorAspectWidth;
			if (aspect.IsSoft())
				color = m_params.SoftAspectColor;
			else if (aspect.IsHard())
				color = m_params.HardAspectColor;

			ctx.MoveTo(pt1).SourceColor(color).LineWidth(width).LineTo(pt2).Stroke();
			ctx.MoveTo((pt1.X + pt2.X) / 2, (pt1.Y + pt2.Y) / 2).FontSize(20).SourceColor(StandardColors::Black).
				ShowText(CStringA(DefaultFont::Get().GetAspectGlyphAsString(aspect.Type)));
		}
	}

	return true;
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

ChartDrawing& ChartDrawing::Chart(ChartData const& data) {
	m_data = data;
	return *this;
}

ChartData const& ChartDrawing::Chart() const {
	return m_data;
}

ChartData& ChartDrawing::Chart() {
	return m_data;
}

ChartDrawing& ChartDrawing::Aspects(std::vector<AspectData>&& aspects) {
	m_aspects = std::move(aspects);
	return *this;
}

CairoPoint ChartDrawing::PointByAngle(CairoPoint const& center, double radius, double angle) const {
	angle = Rad(angle - m_data.Houses().Asc + 180);
	return CairoPoint(center.X + radius * std::cos(angle), center.Y - radius * std::sin(angle));
}
