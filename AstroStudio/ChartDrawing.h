#pragma once

#include "ChartData.h"

struct ChartDrawingParameters {
	CairoColor BackColor{ StandardColors::White };
	CairoColor ElementColor[4] {
		StandardColors::OrangeRed,
		StandardColors::LightGoldenrodYellow,
		StandardColors::LightGreen,
		StandardColors::LightBlue
	};
};

class ChartDrawing {
public:
	ChartDrawing() = default;

	virtual bool Draw(CairoSurface& surface);
	ChartDrawing& DrawingParameters();
	ChartDrawingParameters const& DrawingParameters() const;
	ChartDrawing& Chart(ChartData const& data);
	ChartData const& Chart() const;

private:
	CairoPoint PointByAngle(CairoPoint const& center, double radius, double angle) const;

	ChartDrawingParameters m_params;
	ChartData m_data;
};

