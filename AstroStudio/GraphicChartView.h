#pragma once

#include <FrameView.h>
#include "D2DChartDrawing.h"
#include "Interfaces.h"
#include "ChartImage.h"

class CGraphicChartView : public CFrameView<CGraphicChartView, IMainFrame> {
public:
	using CFrameView::CFrameView;

	void SetChartData(ChartData* data) noexcept;
	void Refresh();
	void SetAspects(std::vector<AspectData> aspects) noexcept;
	// The chart as a size x size picture, drawn the same way as on screen. Null if it couldn't be drawn.
	CComPtr<IWICBitmap> RenderImage(int size);

	// The wheel can be turned by dragging it and a planet picked with a click, which fades the aspects that
	// are not its own. Neither belongs to the chart itself.
	void ResetRotation();
	std::optional<ChartSelection> SelectedPlanet() const noexcept {
		return m_Selected;
	}

	// Shows the transits (the planets of another moment) around the chart, with their aspects to it, and a caption
	// saying when. The data must outlive the view's use of it; ClearTransits goes back to the plain chart.
	void SetTransits(ChartData* data, std::vector<AspectData> aspects, std::wstring caption);
	void ClearTransits();

	BEGIN_MSG_MAP(CGraphicChartView)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
		MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
		MESSAGE_HANDLER(WM_CAPTURECHANGED, OnCaptureChanged)
		MESSAGE_HANDLER(WM_SETCURSOR, OnSetCursor)
		CHAIN_MSG_MAP(BaseFrame)
	END_MSG_MAP()

private:
	LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMouseLeave(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCaptureChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSetCursor(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	bool EnsureRenderTarget();

	// the point in the 1000x1000 space the drawing works in
	D2D1_POINT_2F ToLogical(POINT pt) const;
	// direction of a logical point as seen from the middle of the wheel, degrees counterclockwise
	static double AngleOf(D2D1_POINT_2F const& pt);
	void UpdateHover(POINT pt);
	void ShowTip(POINT pt, PCWSTR text);
	void HideTip();
	CString PlanetTip(int index) const;
	CString AspectTip(int index) const;
	CString TransitPlanetTip(int index) const;
	CString TransitAspectTip(int index) const;
	// what the drawing needs to know besides the chart
	void ApplyState();
	int HouseOf(AstroPoint const& longitude) const;

private:
	CComPtr<ID2D1HwndRenderTarget> m_RenderTarget;
	D2DChartDrawing m_Drawing;
	ChartData* m_ChartData{ nullptr };
	std::vector<AspectData> m_Aspects;

	double m_Rotation{ 0 };
	std::optional<ChartSelection> m_Selected;
	ChartData* m_Transit{ nullptr };
	std::vector<AspectData> m_TransitAspects;
	std::wstring m_TransitCaption;
	ChartHit m_Hover;
	// the left button: pressed, and moved far enough to count as a drag
	bool m_MouseDown{ false }, m_Dragging{ false }, m_Tracking{ false };
	CPoint m_DownPoint;
	double m_DownAngle{ 0 }, m_DownRotation{ 0 };
	// to recognize a double click (which turns the wheel back) without the window class having to ask for them
	DWORD m_LastClickTime{ 0 };
	CPoint m_LastClickPoint;
	CString m_TipText;			// what the hover is about
	CString m_TipShownText;		// what the tooltip window has
	CPoint m_TipPoint{ -1, -1 };
	CToolTipCtrl m_Tip;
	TOOLINFO m_TipTool{};
	bool m_TipShown{ false };
};
