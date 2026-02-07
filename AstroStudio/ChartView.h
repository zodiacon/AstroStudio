#pragma once

#include "ChartDrawing.h"
#include "ChartData.h"
#include <FrameView.h>
#include "Interfaces.h"
#include <VirtualListView.h>
#include "ChartDetailsView.h"
#include "GraphicChartView.h"
#include <CustomSplitterWindow.h>

class CChartView :
	public CFrameView<CChartView, IMainFrame>,
	public IView,
	public CVirtualListView<CChartView> {
public:
	CChartView(IMainFrame* frame);
	void Chart(ChartData data);
	void ChartForNow();
	ChartData const& Chart() const;

	BEGIN_MSG_MAP(CChartView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CVirtualListView)
		CHAIN_MSG_MAP(BaseFrame)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
	END_MSG_MAP()

private:
	void DisplayPlanets(CDCHandle dc, int x, int y) const;
	void DisplayHouses(CDCHandle dc, int x, int y) const;

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRecalc(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	ChartData m_Data;
	AstroCalculator m_Calc;
	CCustomSplitterWindow m_Splitter;
	CChartDetailsView m_DetailsView;
	CGraphicChartView m_ChartDrawing;
};
