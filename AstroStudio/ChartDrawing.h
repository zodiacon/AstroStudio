#pragma once

#include "ChartData.h"
#include "Aspects.h"

struct ChartDrawingParameters {
	CairoColor BackColor{ StandardColors::White };
	CairoColor ElementColor[4] {
		StandardColors::OrangeRed,
		StandardColors::LightGoldenrodYellow,
		StandardColors::LightGreen,
		StandardColors::LightBlue
	};
	CairoColor SoftAspectColor{ StandardColors::Blue };
	CairoColor HardAspectColor{ StandardColors::Red };
	CairoColor AspectColor{ StandardColors::Black };
	double MajorAspectWidth{ 1.2 };
	double MinorAspectWidth{ .7 };

	bool DrawAspects{ true };
	bool DrawMinorAspects{ false };
	bool FillZodiacBelts{ true };
	bool DrawHouseLines{ true };
};

class ChartDrawing {
public:
	ChartDrawing() = default;

	virtual bool Draw(CairoSurface& surface);
	ChartDrawing& DrawingParameters(ChartDrawingParameters const&);
	ChartDrawingParameters const& DrawingParameters() const;
	ChartDrawing& Chart(ChartData const& data);
	ChartData const& Chart() const;
	ChartDrawing& Aspects(std::vector<AspectData>&& aspects);

private:
	CairoPoint PointByAngle(CairoPoint const& center, double radius, double angle) const;

	ChartDrawingParameters m_params;
	ChartData m_data;
	std::vector<AspectData> m_aspects;
};

