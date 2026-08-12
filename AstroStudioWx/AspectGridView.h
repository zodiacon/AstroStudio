#pragma once

#include "AspectGridDrawing.h"

//
// Replacement for CAspectGridWnd (AstroStudio\AspectGridWnd.h) *and* the
// CScrollContainer that hosted it.
//
// The WTL build needed three pieces for one scrollable grid: CAspectGridWnd,
// a CScrollContainer wrapper, and CChartView::UpdateAspectGridScrollSize()
// pushing AspectGridDrawing::GridSize() into the container after every recalc.
// It also needed CChartView::UpdateAspectGridScrollBarTheme(), because
// CScrollContainer is a plain custom-class window that the dark mode
// subclassing did not reach (ChartView.cpp:143).
//
// wxScrolledWindow is the scroll container, so this is one class: set the
// virtual size and wx handles bars, thumb, wheel and theming.
//
class AspectGridView : public wxScrolledWindow {
public:
	explicit AspectGridView(wxWindow* parent);

	void SetChartData(ChartData const* data);
	void SetAspects(std::vector<AspectData> aspects);
	void Refresh();

private:
	void OnPaint(wxPaintEvent& e);

	AspectGridDrawing m_Drawing;
	ChartData const* m_ChartData{};
	std::vector<AspectData> m_Aspects;
};
