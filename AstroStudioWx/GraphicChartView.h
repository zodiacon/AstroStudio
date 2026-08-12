#pragma once

#include "ChartDrawing.h"

//
// Replacement for CGraphicChartView (AstroStudio\GraphicChartView.h).
//
// Most of the WTL class was manual double buffering: a Gdiplus::Bitmap kept in
// m_Bitmap, recreated on WM_SIZE, blitted in OnPaint, plus a WM_ERASEBKGND
// handler returning 1 to stop the flicker. wx does that with one background
// style and wxAutoBufferedPaintDC, so none of it survives - and neither does
// the resize bookkeeping, because there is no backing bitmap to resize.
//
class GraphicChartView : public wxWindow {
public:
	explicit GraphicChartView(wxWindow* parent);

	void SetChartData(ChartData const* data);
	void SetAspects(std::vector<AspectData> aspects);
	void Refresh();

private:
	void OnPaint(wxPaintEvent& e);

	ChartDrawing m_Drawing;
	ChartData const* m_ChartData{};
	std::vector<AspectData> m_Aspects;
};
