#pragma once

#include "ChartDrawing.h"
#include "ChartData.h"
#include <FrameView.h>
#include "Interfaces.h"
#include <VirtualListView.h>
#include "ChartDetailsView.h"
#include "GraphicChartView.h"
#include "AspectGridWnd.h"
#include "AspectListView.h"
#include <CustomSplitterWindow.h>
#include <NativeCustomTabView.h>
#include <atlscrl.h>
#include <WTLHelper.h>

class CChartView :
	public CFrameView<CChartView, IMainFrame>,
	public IView,
	public CVirtualListView<CChartView> {
public:
	CChartView(IMainFrame* frame);
	void Chart(ChartData data);
	void ChartForNow();
	ChartData const& Chart() const;

	BOOL PreTranslateMessage(MSG* pMsg);

	BEGIN_MSG_MAP(CChartView)
		MESSAGE_HANDLER(WM_RECALC, OnRecalc)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_FORWARDMSG, OnForwardMsg)
		MESSAGE_HANDLER(WTLHelper::ThemeChangedMessage, OnThemeChanged)
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
	LRESULT OnForwardMsg(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	void UpdateAspectGridScrollSize();
	void UpdateAspectGridScrollBarTheme();

	ChartData m_Data;
	AstroCalculator m_Calc;
	CCustomSplitterWindow m_Splitter;
	CNativeCustomTabView m_DetailsTabs;
	CChartDetailsView m_DetailsView;
	CScrollContainer m_AspectGridScroll;
	CAspectGridWnd m_AspectGrid;
	CAspectListView m_AspectList;
	CGraphicChartView m_ChartDrawing;
};
