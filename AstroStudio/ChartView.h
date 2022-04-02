#pragma once

#include "ViewBase.h"
#include "ChartDrawing.h"
#include "ChartData.h"

class CChartView :
	public CViewBase<CChartView>,
	public CDoubleBufferImpl<CChartView> {
public:
	using BaseView = CViewBase<CChartView, CDoubleBufferWindowImpl<CChartView>>;
	DECLARE_WND_CLASS(nullptr)

	using CViewBase::CViewBase;

	void OnFinalMessage(HWND /*hWnd*/) override;
	void Chart(ChartData const& chart);
	void Chart(ChartData data);
	ChartData const& Chart() const;

	void DoPaint(CDCHandle dc);

	BEGIN_MSG_MAP(CChartView)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		//MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CViewBase<CChartView>)
		//CHAIN_MSG_MAP(CDoubleBufferImpl<CChartView>)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
	END_MSG_MAP()

private:
	void DisplayPlanets(CDCHandle dc, int x, int y);
	void DisplayHouses(CDCHandle dc, int x, int y);

	LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	ChartDrawing m_Drawing;
	CairoSurface m_Surface;
	ChartData m_Data;
	AstroCalculator m_Calc;
	std::unique_ptr<Gdiplus::Bitmap> m_Bitmap;
	int m_DrawingSize;
	bool m_RedrawNeeded{ true };
};
