#pragma once

#include "Interfaces.h"
#include "AstroCalculator.h"

class GraphicChartView;
class AspectGridView;
class wxSplitterWindow;
class wxNotebook;

//
// Replacement for CChartView (AstroStudio\ChartView.h).
//
// The WTL class inherited CFrameView + IView + CVirtualListView and owned six
// child windows including a CCustomSplitterWindow and a CScrollContainer. Here
// it is a wxPanel holding a wxSplitterWindow: the chart wheel on the left, a
// notebook of detail pages on the right, exactly as
// CChartView::OnCreate arranged them.
//
class ChartView : public wxPanel, public IView {
public:
	ChartView(wxWindow* parent, IMainFrame* frame);

	// Calculates and adopts the chart, then pushes it to the sub-views.
	// Equivalent to CChartView::Chart(ChartData).
	void SetChart(ChartData data);

	// Equivalent to CChartView::ChartForNow().
	void ChartForNow();

	ChartData const& Chart() const;

	// Replaces the WM_RECALC handler. Named Recalculate, not Recalc, because a
	// member named Recalc would shadow the enum type inside the class.
	void Recalculate(Recalc what);

	// IView
	void PageActivated(bool active) override;

private:
	// Recomputes aspects and refreshes every sub-view. This is the tail shared
	// by CChartView::Chart() and CChartView::OnRecalc(), which duplicated it.
	void RefreshFromData();

	IMainFrame* m_Frame;
	ChartData m_Data;
	AstroCalculator m_Calc;

	wxSplitterWindow* m_Splitter{};
	wxNotebook* m_DetailsTabs{};
	GraphicChartView* m_ChartDrawing{};
	AspectGridView* m_AspectGrid{};
	wxStaticText* m_DetailsSummary{};
	bool m_SashCentred{ false };
};
