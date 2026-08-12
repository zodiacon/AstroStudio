#pragma once

#include "ChartData.h"
#include "Aspects.h"

class wxGraphicsContext;

//
// Port of AstroStudio\AspectGridDrawing.h. Gdiplus::Color -> wxColour;
// geometry, cell size and layout are unchanged.
//
struct AspectGridDrawingParameters {
	wxColour BackColor{ 255, 255, 255 };			// White
	wxColour GridLineColor{ 128, 128, 128 };		// Gray

	wxColour SoftAspectColor{ 0, 0, 255 };			// Blue
	wxColour HardAspectColor{ 255, 0, 0 };			// Red
	wxColour MinorAspectColor{ 128, 0, 128 };		// Purple
	wxColour AspectColor{ 0, 0, 0 };				// Black

	int CellSize{ 44 };
};

class AspectGridDrawing {
public:
	AspectGridDrawing() = default;

	//
	// offset is the top-left of the grid in device pixels; the scrolling view
	// passes a negative offset instead of relying on the DC origin.
	//
	// Takes the DC rather than a wxGraphicsContext for the same reason as
	// ChartDrawing::Draw - see AstroHelpers::GlyphRun.
	//
	bool Draw(wxDC& dc, wxPoint offset = wxPoint(0, 0));

	AspectGridDrawing& DrawingParameters(AspectGridDrawingParameters const& params);
	AspectGridDrawingParameters const& DrawingParameters() const;
	AspectGridDrawingParameters& DrawingParameters();

	AspectGridDrawing& Chart(ChartData const* data);
	ChartData const* Chart() const;
	AspectGridDrawing& Aspects(std::vector<AspectData> const* aspects);

	// Pixel size (width == height) needed to draw the current chart's grid.
	int GridSize() const;

private:
	AspectGridDrawingParameters m_params;
	ChartData const* m_data{};
	std::vector<AspectData> const* m_aspects{};
};
