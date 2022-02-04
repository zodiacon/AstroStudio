#pragma once

#include "ViewBase.h"
#include "ChartDrawing.h"
#include "ChartData.h"

class CChartView :
	public CViewBase<CChartView> {
public:
	DECLARE_WND_CLASS(nullptr)

	using CViewBase::CViewBase;

	BOOL PreTranslateMessage(MSG* pMsg);
	void OnFinalMessage(HWND /*hWnd*/) override;

protected:
	BEGIN_MSG_MAP(CChartView)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CViewBase<CChartView>)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
	END_MSG_MAP()

private:
	LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	ChartDrawing m_Drawing;
	CairoSurface m_Surface;
	ChartData m_Data;
	AstroCalculator m_Calc;
	int m_DrawingSize;
	bool m_RedrawNeeded{ true };
};
