#include "pch.h"
#include "ChartDrawing.h"
#include <cmath>
#include "Helpers.h"
#include "PlanetSpacer.h"
#include "DefaultFont.h"

const double PI = std::atan(1) * 4;

constexpr double Rad(double angle) {
	return angle * PI / 180;
}

bool ChartDrawing::Draw(Gdiplus::Graphics& g, int size) {
	using namespace Gdiplus;

	Matrix xform;
	xform.Scale(size / 1000.0f, size / 1000.0f);
	g.MultiplyTransform(&xform);
	g.Clear(m_params.BackColor);

	Pen pen(Color::Black, 2);
	PointF center(500, 500);

	//
	// draw ASC/MC line
	//
	g.DrawLine(&pen, PointF(5, 500), PointF(995, 500));

	//
	// draw MC/IC line
	//

	g.DrawLine(&pen, PointByAngle(center, 495, m_data.Houses().MC), PointByAngle(center, 495, m_data.Houses().MC.Opposite()));

	double startAngle = m_data.Houses().Asc.NextSign() + 120 + m_data.Houses().Asc.DegreeInSign();
	static const WCHAR text[] = L"asdfghjklzxc";
	Font font(L"HamburgSymbols", 17);
	
	//
	// draw zodiac
	//
	std::array<SolidBrush, 4> elements {
		SolidBrush { m_params.ElementColor[0] },
		SolidBrush { m_params.ElementColor[1] },
		SolidBrush { m_params.ElementColor[2] },
		SolidBrush { m_params.ElementColor[3] },
	};
	Pen thin(Color::Black, 1);
	GraphicsPath path;
	RectF outerRect(20, 20, 960, 960);
	RectF innerZodiac(outerRect);
	innerZodiac.Inflate(-m_params.ZodiacBeltWidth, -m_params.ZodiacBeltWidth);
	path.AddEllipse(innerZodiac);
	g.SetClip(&path, CombineMode::CombineModeExclude);
	auto angle = (float)startAngle;
	SolidBrush blackBrush(Color::Black);
	StringFormat format;
	format.SetAlignment(StringAlignment::StringAlignmentCenter);
	format.SetLineAlignment(StringAlignment::StringAlignmentCenter);
	WCHAR sign[] = L"a";

	for (int i = 0; i < 12; i++) {
		if(m_params.FillZodiacBelts)
			g.FillPie(&elements[i % 4], outerRect, angle, 30);
		g.DrawPie(&thin, outerRect, angle, 30);
		angle += 15;
		auto x = 500 + (innerZodiac.Width + m_params.ZodiacBeltWidth) / 2 * (float)std::cos(Rad(angle));
		auto y = 500 + (innerZodiac.Height + m_params.ZodiacBeltWidth) / 2 * (float)std::sin(Rad(angle));
		sign[0] = text[i];
		g.DrawString(sign, 1, &font, PointF(x, y), &format, &blackBrush);
		angle -= 45;
	}
	g.ResetClip();
	g.DrawEllipse(&thin, innerZodiac);

	//
	// draw houses
	//
	if (m_params.DrawHouseLines) {
		Pen pen(Color::Gray);
		for (int i = 0; i < 12; i++) {
			if (i % 3 == 0)
				continue;
			g.DrawLine(&pen, center, PointByAngle(center, innerZodiac.Width / 2, m_data.Houses().Cusps[i]));
		}
	}

	//
	// draw planets
	//
	float r = 420;
	auto asc = m_data.Houses().Asc;
	PlanetSpacer spacer(m_data.AllPlanets());
	spacer.Space();

	Font planetFont(L"HamburgSymbols", 20);
	for (auto& pp : spacer.NewPositions()) {
		auto pt = PointByAngle(center, r, pp.Longitude);
		auto x = pt.X, y = pt.Y;
		auto glyph = DefaultFont::Get().GetPlanetGlyphAsString(pp.Planet);
		g.DrawString(glyph, 1, &planetFont, PointF(x, y), &format, &blackBrush);
	}

	r = 395;
	SolidBrush blueBrush(Color::Blue);
	for (auto& pp : m_data.AllPlanets()) {
		auto pt = PointByAngle(center, r, pp.Longitude);
		g.FillEllipse(&blueBrush, RectF(pt.X - 3, pt.Y - 3, 6, 6));
	}

	//
	// draw aspects
	//
	if (m_params.DrawAspects) {
		for (auto const& aspect : m_aspects) {
			if (aspect.Type == AspectType::Conjunction)
				continue;

			if (!m_params.DrawVeryMinorAspects && (aspect.Type == AspectType::Septile || aspect.Type == AspectType::BiSeptile ||
				aspect.Type == AspectType::Quintile || aspect.Type == AspectType::BiQuintile))
				continue;

			if (!m_params.DrawNonStandardPlanetAspects && (aspect.Planet1.Planet > PlanetType::Pluto || aspect.Planet2.Planet > PlanetType::Pluto))
				continue;

			auto pt1 = PointByAngle(center, r, aspect.Planet1.Longitude);
			auto pt2 = PointByAngle(center, r, aspect.Planet2.Longitude);
			auto color(m_params.AspectColor);
			double width = m_params.MinorAspectWidth;
			if (aspect.IsMajor()) {
				width = m_params.MajorAspectWidth;
			}
			else {
				color = m_params.MinorAspectColor;
			}
			if (aspect.IsSoft())
				color = m_params.SoftAspectColor;
			else if (aspect.IsHard())
				color = m_params.HardAspectColor;

			auto maxWidth(width);
			width = 4 - aspect.Orb / 2;
			if (width > maxWidth)
				width = maxWidth;
			else if (width < .5f)
				width = .5f;
			Pen pen(color, (float)width);
			g.DrawLine(&pen, pt1, pt2);
			g.DrawString(DefaultFont::Get().GetAspectGlyphAsString(aspect.Type), 1, &font, PointF((pt1.X + pt2.X) / 2, (pt1.Y + pt2.Y) / 2), &format, &blackBrush);
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

Gdiplus::PointF ChartDrawing::PointByAngle(Gdiplus::PointF const& center, float radius, double angle) const {
	angle = Rad(angle - m_data.Houses().Asc + 180);
	return Gdiplus::PointF(center.X + radius * (float)std::cos(angle), center.Y - radius * (float)std::sin(angle));
}
