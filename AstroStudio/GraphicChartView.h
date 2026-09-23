#pragma once

#include <FrameView.h>
#include "D2DChartDrawing.h"
#include "Interfaces.h"

class CGraphicChartView : public CFrameView<CGraphicChartView, IMainFrame> {
public:
	using CFrameView::CFrameView;

	void SetChartData(ChartData* data) noexcept;
	void Refresh();
	void SetAspects(std::vector<AspectData> aspects) noexcept;

	BEGIN_MSG_MAP(CGraphicChartView)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		CHAIN_MSG_MAP(BaseFrame)
	END_MSG_MAP()

private:
	LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	bool EnsureRenderTarget();

private:
	CComPtr<ID2D1Factory> m_D2DFactory;
	CComPtr<ID2D1HwndRenderTarget> m_RenderTarget;
	D2DChartDrawing m_Drawing;
	ChartData* m_ChartData{ nullptr };
	std::vector<AspectData> m_Aspects;
};
