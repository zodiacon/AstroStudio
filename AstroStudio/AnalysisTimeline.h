#pragma once

#include <functional>
#include <vector>

// The analysis view's Timeline: every stay of an aspect within its orb as a bar along the dates of the analysis, one lane each - the
// bar runs from when it entered the orb to when it left (to the edge if the range cut it off), with a mark at each time it was exact.
// A date axis runs along the top, today is marked if it is in the range, a lane's tip says what it is and when, and a double click
// hands the lane and the moment clicked to the owner. It knows nothing of analyses: the owner makes the lanes.
class CAnalysisTimeline : public CWindowImpl<CAnalysisTimeline> {
public:
	DECLARE_WND_CLASS_EX(L"AstroStudioTimeline", CS_DBLCLKS, -1)

	struct Lane {
		CString Label;				// at the left: "Transit Saturn square natal Sun"
		CString Tip;				// the tooltip (lines separated by \n)
		COLORREF Color{ RGB(128, 128, 128) };
		double Start{ 0 }, End{ 0 };	// Julian days (UT): the stay as far as the range shows it
		bool OpenStart{ false }, OpenEnd{ false };		// it was already within the orb when the range began / still is when it ends
		std::vector<double> Exacts;
		int Tag{ 0 };				// the owner's
	};

	// the dates along the axis (Julian days, UT)
	void SetRange(double from, double to);
	// the lanes, top to bottom; the scroll position is kept if it can be
	void SetLanes(std::vector<Lane> lanes);
	void SetTextFont(HFONT font);
	int LaneCount() const {
		return static_cast<int>(m_Lanes.size());
	}

	// a double click on a lane, at the moment (Julian day) under the mouse
	std::function<void(Lane const& lane, double jd)> Activated;

	BEGIN_MSG_MAP(CAnalysisTimeline)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_VSCROLL, OnVScroll)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
		MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnDoubleClick)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
		MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
		MESSAGE_HANDLER(WM_GETDLGCODE, OnGetDlgCode)
	END_MSG_MAP()

private:
	LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnEraseBackground(UINT, WPARAM, LPARAM, BOOL&) {
		return 1;		// (painted whole, through a memory DC)
	}
	LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnVScroll(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnMouseWheel(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnMouseMove(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnMouseLeave(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDoubleClick(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnLButtonDown(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnKeyDown(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnGetDlgCode(UINT, WPARAM, LPARAM, BOOL&) {
		return DLGC_WANTARROWS;
	}

	// the sizes for the font and the DPI: a lane's height, the axis band's, the label column's
	void Measure();
	void UpdateScrollBar();
	void ScrollTo(int position);
	// where the bars are drawn, left and right (client x)
	int PlotLeft() const;
	int PlotRight() const;
	double XToJd(int x) const;
	int JdToX(double jd) const;
	// the lane under a point of the client area, -1 if none
	int LaneAt(POINT pt) const;
	void ShowTip(int lane, POINT pt);
	void HideTip();

	struct Tick {
		double Jd;
		CString Text;
	};
	// the ticks of the axis for the range: days, months, years or decades by how long it is
	std::vector<Tick> Ticks() const;

	std::vector<Lane> m_Lanes;
	double m_From{ 0 }, m_To{ 1 };
	HFONT m_Font{ nullptr };
	int m_LaneHeight{ 20 }, m_AxisHeight{ 26 }, m_LabelWidth{ 200 };
	int m_Scroll{ 0 };			// pixels scrolled down
	int m_TipLane{ -1 };
	bool m_Tracking{ false };
	CToolTipCtrl m_Tip;
	TOOLINFO m_ToolInfo{};
	CString m_TipText;
};
