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
	int NaturalSize() const noexcept;

	BEGIN_MSG_MAP(CAspectGridWnd)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WTLHelper::ThemeChangedMessage, OnThemeChanged)
		CHAIN_MSG_MAP(BaseFrame)
	END_MSG_MAP()

private:
	LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) const;
	LRESULT OnThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	std::unique_ptr<Gdiplus::Bitmap> m_Bitmap;
	AspectGridDrawing m_Drawing;
	ChartData* m_ChartData{ nullptr };
	std::vector<AspectData> m_Aspects;
};
