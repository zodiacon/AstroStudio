#pragma once

#include "ChartDrawing.h"
#include "ChartData.h"
#include <FrameView.h>
#include "Interfaces.h"
#include "ChartDetailsView.h"
#include <VirtualListView.h>

class CChartView :
	public CFrameView<CChartView, IMainFrame>,
	public IView,
	public CVirtualListView<CChartView>,
	public CDoubleBufferImpl<CChartView> {
public:
	using CFrameView::CFrameView;
	void Chart(ChartData data);
	void ChartForNow();
	ChartData const& Chart() const;

	void DoPaint(CDCHandle dc);

	BEGIN_MSG_MAP(CChartView)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		//MESSAGE_HANDLER(WM_PAINT, OnPaint)
		//MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_RECALC, OnRecalc)
		CHAIN_MSG_MAP(CVirtualListView<CChartView>)
		CHAIN_MSG_MAP(CDoubleBufferImpl<CChartView>)
		CHAIN_MSG_MAP(BaseFrame)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
	END_MSG_MAP()

private:
	void DisplayPlanets(CDCHandle dc, int x, int y) const;
	void DisplayHouses(CDCHandle dc, int x, int y) const;

	LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRecalc(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	ChartDrawing m_Drawing;
	ChartData m_Data;
	AstroCalculator m_Calc;
	CSplitterWindow m_Splitter;
	CChartDetailsView m_DetailsView;
	std::unique_ptr<Gdiplus::Bitmap> m_Bitmap;
	int m_DrawingSize;
	bool m_RedrawNeeded{ true };
};
