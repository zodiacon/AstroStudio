#pragma once

#include "ChartData.h"
#include "Aspects.h"

struct AspectGridDrawingParameters {
	Gdiplus::Color BackColor{ Gdiplus::Color(Gdiplus::Color::White) };
	Gdiplus::Color GridLineColor{ Gdiplus::Color(Gdiplus::Color::Gray) };

	Gdiplus::Color SoftAspectColor{ Gdiplus::Color(Gdiplus::Color::Blue) };
	Gdiplus::Color HardAspectColor{ Gdiplus::Color(Gdiplus::Color::Red) };
	Gdiplus::Color MinorAspectColor{ Gdiplus::Color(Gdiplus::Color::Purple) };
	Gdiplus::Color AspectColor{ Gdiplus::Color(Gdiplus::Color::Black) };

	int CellSize{ 44 };
};

class AspectGridDrawing {
public:
	AspectGridDrawing() = default;

	bool Draw(Gdiplus::Graphics& g);
	AspectGridDrawing& DrawingParameters(AspectGridDrawingParameters const&);
	AspectGridDrawingParameters const& DrawingParameters() const;
	AspectGridDrawingParameters& DrawingParameters();
	AspectGridDrawing& Chart(ChartData* data);
	ChartData* Chart() const;
	AspectGridDrawing& Aspects(std::vector<AspectData>* aspects);

	// pixel size (width == height) needed to draw the current chart's grid
	int GridSize() const;

private:
	AspectGridDrawingParameters m_params;
	ChartData* m_data{ nullptr };
	std::vector<AspectData>* m_aspects{ nullptr };
};
