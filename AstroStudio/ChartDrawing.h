#pragma once

#include "ChartData.h"
#include "Aspects.h"

struct ChartDrawingParameters {
	CairoColor BackColor{ StandardColors::White };
	CairoColor ElementColor[4]{
		StandardColors::OrangeRed,
		StandardColors::LightGoldenrodYellow,
		StandardColors::LightGreen,
		StandardColors::LightBlue
	};

	Gdiplus::Color GdiplusElementColor[4] {
		Gdiplus::Color(Gdiplus::Color::OrangeRed),
		Gdiplus::Color(Gdiplus::Color::LightGoldenrodYellow),
		Gdiplus::Color(Gdiplus::Color::LightGreen),
		Gdiplus::Color(Gdiplus::Color::LightBlue)
	};
	CairoColor SoftAspectColor{ StandardColors::Blue };
	CairoColor HardAspectColor{ StandardColors::Red };
	CairoColor AspectColor{ StandardColors::Black };

	Gdiplus::Color GdiplusSoftAspectColor{ Gdiplus::Color::Blue };
	Gdiplus::Color GdiplusHardAspectColor{ Gdiplus::Color::Red };
	Gdiplus::Color GdiplusAspectColor{ Gdiplus::Color::Black };

	double MajorAspectWidth{ 1.2 };
	double MinorAspectWidth{ .7 };
	float ZodiacBeltWidth{ 40 };

	bool DrawAspects{ true };
	bool DrawMinorAspects{ false };
	bool FillZodiacBelts{ true };
	bool DrawHouseLines{ true };
	bool DrawVeryMinorAspects{ false };
	bool DrawNonStandardPlanetAspects{ false };
};

class ChartDrawing {
public:
	ChartDrawing() = default;

	virtual bool Draw(CairoSurface& surface);
	virtual bool Draw(Gdiplus::Graphics& g, int size);
	ChartDrawing& DrawingParameters(ChartDrawingParameters const&);
	ChartDrawingParameters const& DrawingParameters() const;
	ChartDrawingParameters& DrawingParameters();
	ChartDrawing& Chart(ChartData const& data);
	ChartData const& Chart() const;
	ChartData& Chart();
	ChartDrawing& Aspects(std::vector<AspectData>&& aspects);

private:
	CairoPoint PointByAngle(CairoPoint const& center, double radius, double angle) const;
	Gdiplus::PointF PointByAngle(Gdiplus::PointF const& center, float radius, double angle) const;

	ChartDrawingParameters m_params;
	ChartData m_data;
	std::vector<AspectData> m_aspects;
};

