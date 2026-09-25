#pragma once

#include <FrameView.h>
#include "AspectGridDrawing.h"
#include "Interfaces.h"
#include <WTLHelper.h>

class CAspectGridWnd : public CFrameView<CAspectGridWnd, IMainFrame> {
public:
	explicit CAspectGridWnd(IMainFrame* frame);

	void SetChartData(ChartData* data) noexcept;
	void SetAspects(std::vector<AspectData> aspects) noexcept;
	void Refresh();
	// the overlay's planets go down the side (null: the chart's own); the overlay must outlive its use here
	void SetOverlay(ChartOverlay const* overlay) noexcept;
	SIZE NaturalSize() const noexcept;

	BEGIN_MSG_MAP(CAspectGridWnd)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WTLHelper::ThemeChangedMessage, OnThemeChanged)
		CHAIN_MSG_MAP(BaseFrame)
	END_MSG_MAP()

private:
	LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	bool EnsureRenderTarget();

	CComPtr<ID2D1HwndRenderTarget> m_RenderTarget;
	AspectGridDrawing m_Drawing;
	ChartData* m_ChartData{ nullptr };
	std::vector<AspectData> m_Aspects;
	ChartOverlay const* m_Overlay{ nullptr };
};
