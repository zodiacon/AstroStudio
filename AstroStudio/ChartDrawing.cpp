#include "pch.h"
#include "ChartDrawing.h"
#include <cmath>
#include "Helpers.h"

bool ChartDrawing::Draw(CairoSurface& surface) {
	auto scale = surface.Width() / 1000.0;
	CairoCtx ctx(surface);
	ctx.Scale(scale, scale);
	ctx.Clear(m_params.BackColor);

	double startAngle = m_data.Houses().Asc.NextSign() + 150 + m_data.Houses().Asc.DegreeInSign();
	ctx.FontSize(20);
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
	ctx.LineWidth(1).SourceColor(StandardColors::Gray);
	for (int i = 0; i < 12; i++) {
		if (i % 3 == 0)
			continue;
		ctx.MoveTo(500, 500);
		ctx.LineTo(PointByAngle(CairoPoint(500, 500), 430, m_data.Houses().Cusps[i])).Stroke();
	}

	//
	// draw planets
	//
	double r = 370;
	auto asc = m_data.Houses().Asc;
	ctx.FontSize(30);
	for (int i = 0; i < m_data.PlanetsCount(); i++) {
		auto& pp = m_data.Planet(i);
		auto x = 500 + r * std::cos(Rad(pp.Longitude - asc + 180));
		auto y = 500 - r * std::sin(Rad(pp.Longitude - asc + 180));
		ctx.Circle(x, y, 4).SourceColor(StandardColors::DarkBlue).Fill();
		x += 25 * std::cos(Rad(pp.Longitude - asc + 180));
		y -= 25 * std::sin(Rad(pp.Longitude - asc + 180));
		CStringA glyph(Helpers::GetPlanetGlyphAsString(pp.Planet));
		auto ext = ctx.TextExtents(glyph);
		ctx.MoveTo(x - ext.width / 2, y + ext.width / 2).SourceColor(StandardColors::Black).ShowText(glyph);
		ctx.NewPath();
	}

	return true;
}

ChartDrawing& ChartDrawing::Chart(ChartData const& data) {
	m_data = data;
	return *this;
}

ChartData const& ChartDrawing::Chart() const {
	return m_data;
}

CairoPoint ChartDrawing::PointByAngle(CairoPoint const& center, double radius, double angle) const {
	angle = Rad(angle - m_data.Houses().Asc + 180);
	return CairoPoint(center.X + radius * std::cos(angle), center.Y - radius * std::sin(angle));
}
