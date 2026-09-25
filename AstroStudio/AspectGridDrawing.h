#pragma once

#include "ChartData.h"
#include "Aspects.h"
#include "D2DResources.h"
#include "ChartOverlay.h"

struct AspectGridDrawingParameters {
	D2D1_COLOR_F BackColor{ D2D1::ColorF(D2D1::ColorF::White) };
	D2D1_COLOR_F GridLineColor{ D2D1::ColorF(D2D1::ColorF::Gray) };

	D2D1_COLOR_F SoftAspectColor{ D2D1::ColorF(D2D1::ColorF::Blue) };
	D2D1_COLOR_F HardAspectColor{ D2D1::ColorF(D2D1::ColorF::Red) };
	D2D1_COLOR_F MinorAspectColor{ D2D1::ColorF(D2D1::ColorF::Purple) };
	D2D1_COLOR_F AspectColor{ D2D1::ColorF(D2D1::ColorF::Black) };
	D2D1_COLOR_F OverlayColor{ D2D1::ColorF(D2D1::ColorF::Crimson) };		// the overlay's planets, when they are down the side

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
	// With an overlay (transits...) the grid is the overlay's planets down the side against the chart's along the top, with the
	// overlay's aspects between them (the chart's own aspects are not shown then). Null: the chart's planets against each other.
	AspectGridDrawing& Overlay(ChartOverlay const* overlay);

	// the pixel size needed to draw the grid as it is now (not square with an overlay: it has its own planets)
	SIZE GridSize() const;

private:
	HRESULT EnsureFormats();

	AspectGridDrawingParameters m_params;
	ChartData* m_data{ nullptr };
	std::vector<AspectData>* m_aspects{ nullptr };
	ChartOverlay const* m_overlay{ nullptr };

	// text formats scale with the cell size, so they are rebuilt when it changes
	CComPtr<IDWriteTextFormat> m_glyphFormat;
	CComPtr<IDWriteTextFormat> m_infoFormat;
	CComPtr<IDWriteTextFormat> m_labelFormat;
	int m_formatCellSize{ 0 };
	std::wstring m_formatFamily;
};
