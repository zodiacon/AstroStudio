#pragma once

#include "ChartData.h"
#include "Aspects.h"
#include "D2DResources.h"

struct AspectGridDrawingParameters {
	D2D1_COLOR_F BackColor{ D2D1::ColorF(D2D1::ColorF::White) };
	D2D1_COLOR_F GridLineColor{ D2D1::ColorF(D2D1::ColorF::Gray) };

	D2D1_COLOR_F SoftAspectColor{ D2D1::ColorF(D2D1::ColorF::Blue) };
	D2D1_COLOR_F HardAspectColor{ D2D1::ColorF(D2D1::ColorF::Red) };
	D2D1_COLOR_F MinorAspectColor{ D2D1::ColorF(D2D1::ColorF::Purple) };
	D2D1_COLOR_F AspectColor{ D2D1::ColorF(D2D1::ColorF::Black) };

	int CellSize{ 44 };
};

class AspectGridDrawing {
public:
	// Clears the whole render target and draws the grid from its top-left corner. The caller owns
	// BeginDraw/EndDraw (and so D2DERR_RECREATE_TARGET handling).
	HRESULT Draw(ID2D1RenderTarget* rt);
	AspectGridDrawing& DrawingParameters(AspectGridDrawingParameters const&);
	AspectGridDrawingParameters const& DrawingParameters() const;
	AspectGridDrawingParameters& DrawingParameters();
	AspectGridDrawing& Chart(ChartData* data);
	ChartData* Chart() const;
	AspectGridDrawing& Aspects(std::vector<AspectData>* aspects);

	// pixel size (width == height) needed to draw the current chart's grid
	int GridSize() const;

private:
	HRESULT EnsureFormats();

	AspectGridDrawingParameters m_params;
	ChartData* m_data{ nullptr };
	std::vector<AspectData>* m_aspects{ nullptr };

	// text formats scale with the cell size, so they are rebuilt when it changes
	CComPtr<IDWriteTextFormat> m_glyphFormat;
	CComPtr<IDWriteTextFormat> m_infoFormat;
	int m_formatCellSize{ 0 };
};
