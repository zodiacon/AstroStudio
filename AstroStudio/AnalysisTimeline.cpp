#include "pch.h"
#include "AnalysisTimeline.h"
#include "DateTime.h"
#include "WTLHelper.h"
#include "DarkMode/DarkModeSubclass.h"
#include <algorithm>
#include <cmath>

namespace {
	DateTime FromJd(double jd) {
		return DateTime(jd, DateTime::AfterPapalReform(jd));
	}

	double DayStart(long year, long month, long day) {
		return DateTime::DateToJD(year, month, day, DateTime::AfterPapalReform(year, month, day));
	}

	// a colour part way to another
	COLORREF Mix(COLORREF a, COLORREF b, double t) {
		auto mix = [t](int x, int y) { return (int)std::lround(x + (y - x) * t); };
		return RGB(mix(GetRValue(a), GetRValue(b)), mix(GetGValue(a), GetGValue(b)), mix(GetBValue(a), GetBValue(b)));
	}

	struct Colors {
		COLORREF Back, Text, Grid, Stripe, Axis, Today;
	};

	Colors CurrentColors() {
		Colors c;
		if (WTLHelper::IsDarkMode()) {
			c.Back = DarkMode::getViewBackgroundColor();
			c.Text = DarkMode::getTextColor();
		}
		else {
			c.Back = ::GetSysColor(COLOR_WINDOW);
			c.Text = ::GetSysColor(COLOR_WINDOWTEXT);
		}
		c.Grid = Mix(c.Back, c.Text, 0.15);
		c.Stripe = Mix(c.Back, c.Text, 0.04);
		c.Axis = Mix(c.Back, c.Text, 0.08);
		c.Today = RGB(255, 140, 0);
		return c;
	}
}

void CAnalysisTimeline::SetRange(double from, double to) {
	m_From = from;
	m_To = to > from ? to : from + 1;
	if (IsWindow())
		Invalidate();
}

void CAnalysisTimeline::SetLanes(std::vector<Lane> lanes) {
	m_Lanes = std::move(lanes);
	HideTip();
	if (IsWindow()) {
		Measure();
		UpdateScrollBar();
		Invalidate();
	}
}

void CAnalysisTimeline::SetTextFont(HFONT font) {
	m_Font = font;
	if (IsWindow()) {
		Measure();
		UpdateScrollBar();
		Invalidate();
	}
}

LRESULT CAnalysisTimeline::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_Tip.Create(m_hWnd, nullptr, nullptr, TTS_NOPREFIX | TTS_ALWAYSTIP, WS_EX_TOPMOST);
	m_ToolInfo.cbSize = sizeof(m_ToolInfo);
	m_ToolInfo.uFlags = TTF_TRACK | TTF_ABSOLUTE;
	m_ToolInfo.hwnd = m_hWnd;
	m_ToolInfo.uId = 1;
	m_ToolInfo.lpszText = (PWSTR)L"";
	m_Tip.AddTool(&m_ToolInfo);
	m_Tip.SetMaxTipWidth(DpiHelper::Scale(500, m_hWnd));
	DarkMode::setDarkScrollBar(m_hWnd);
	Measure();
	UpdateScrollBar();
	return 0;
}

void CAnalysisTimeline::Measure() {
	CClientDC dc(m_hWnd);
	auto old = dc.SelectFont(m_Font ? m_Font : AtlGetDefaultGuiFont());
	TEXTMETRIC tm;
	dc.GetTextMetrics(&tm);
	m_LaneHeight = tm.tmHeight + DpiHelper::Scale(8, m_hWnd);
	m_AxisHeight = tm.tmHeight + DpiHelper::Scale(12, m_hWnd);
	int widest = 0;
	for (auto const& lane : m_Lanes) {
		CSize size;
		dc.GetTextExtent(lane.Label, lane.Label.GetLength(), &size);
		widest = std::max(widest, (int)size.cx);
	}
	dc.SelectFont(old);
	CRect rc;
	GetClientRect(&rc);
	// (at most two fifths of the width: a long label is cut short with an ellipsis)
	m_LabelWidth = std::min(widest + DpiHelper::Scale(16, m_hWnd), std::max((int)rc.Width() * 2 / 5, DpiHelper::Scale(120, m_hWnd)));
}

int CAnalysisTimeline::PlotLeft() const {
	return m_LabelWidth + DpiHelper::Scale(8, m_hWnd);
}

int CAnalysisTimeline::PlotRight() const {
	CRect rc;
	GetClientRect(&rc);
	return std::max(PlotLeft() + 10, (int)rc.right - DpiHelper::Scale(12, m_hWnd));
}

int CAnalysisTimeline::JdToX(double jd) const {
	double t = (jd - m_From) / (m_To - m_From);
	return PlotLeft() + (int)std::lround(t * (PlotRight() - PlotLeft()));
}

double CAnalysisTimeline::XToJd(int x) const {
	double t = double(x - PlotLeft()) / std::max(1, PlotRight() - PlotLeft());
	return m_From + std::clamp(t, 0.0, 1.0) * (m_To - m_From);
}

int CAnalysisTimeline::LaneAt(POINT pt) const {
	if (pt.y < m_AxisHeight)
		return -1;
	int lane = (pt.y - m_AxisHeight + m_Scroll) / m_LaneHeight;
	return lane < (int)m_Lanes.size() ? lane : -1;
}

void CAnalysisTimeline::UpdateScrollBar() {
	CRect rc;
	GetClientRect(&rc);
	int page = std::max(0, (int)rc.Height() - m_AxisHeight);
	int total = (int)m_Lanes.size() * m_LaneHeight;
	SCROLLINFO si{ sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL };
	si.nMin = 0;
	si.nMax = std::max(0, total - 1);
	si.nPage = page;
	m_Scroll = std::clamp(m_Scroll, 0, std::max(0, total - page));
	si.nPos = m_Scroll;
	SetScrollInfo(SB_VERT, &si);
}

void CAnalysisTimeline::ScrollTo(int position) {
	CRect rc;
	GetClientRect(&rc);
	int page = std::max(0, (int)rc.Height() - m_AxisHeight);
	int total = (int)m_Lanes.size() * m_LaneHeight;
	position = std::clamp(position, 0, std::max(0, total - page));
	if (position == m_Scroll)
		return;
	m_Scroll = position;
	SetScrollPos(SB_VERT, m_Scroll);
	HideTip();
	Invalidate();
}

LRESULT CAnalysisTimeline::OnSize(UINT, WPARAM, LPARAM, BOOL&) {
	Measure();
	UpdateScrollBar();
	Invalidate();
	return 0;
}

LRESULT CAnalysisTimeline::OnVScroll(UINT, WPARAM wp, LPARAM, BOOL&) {
	CRect rc;
	GetClientRect(&rc);
	int page = std::max(m_LaneHeight, (int)rc.Height() - m_AxisHeight);
	int position = m_Scroll;
	switch (LOWORD(wp)) {
		case SB_LINEUP: position -= m_LaneHeight; break;
		case SB_LINEDOWN: position += m_LaneHeight; break;
		case SB_PAGEUP: position -= page; break;
		case SB_PAGEDOWN: position += page; break;
		case SB_TOP: position = 0; break;
		case SB_BOTTOM: position = INT_MAX / 2; break;
		case SB_THUMBTRACK:
		case SB_THUMBPOSITION: {
			SCROLLINFO si{ sizeof(si), SIF_TRACKPOS };
			GetScrollInfo(SB_VERT, &si);
			position = si.nTrackPos;
			break;
		}
	}
	ScrollTo(position);
	return 0;
}

LRESULT CAnalysisTimeline::OnMouseWheel(UINT, WPARAM wp, LPARAM, BOOL&) {
	int delta = GET_WHEEL_DELTA_WPARAM(wp);
	ScrollTo(m_Scroll - delta * 3 * m_LaneHeight / WHEEL_DELTA);
	return 0;
}

LRESULT CAnalysisTimeline::OnKeyDown(UINT, WPARAM wp, LPARAM, BOOL& handled) {
	BOOL dummy;
	switch (wp) {
		case VK_UP: OnVScroll(0, SB_LINEUP, 0, dummy); break;
		case VK_DOWN: OnVScroll(0, SB_LINEDOWN, 0, dummy); break;
		case VK_PRIOR: OnVScroll(0, SB_PAGEUP, 0, dummy); break;
		case VK_NEXT: OnVScroll(0, SB_PAGEDOWN, 0, dummy); break;
		case VK_HOME: OnVScroll(0, SB_TOP, 0, dummy); break;
		case VK_END: OnVScroll(0, SB_BOTTOM, 0, dummy); break;
		default: handled = FALSE; break;
	}
	return 0;
}

LRESULT CAnalysisTimeline::OnLButtonDown(UINT, WPARAM, LPARAM, BOOL&) {
	SetFocus();		// (so the wheel and the arrow keys scroll it)
	return 0;
}

LRESULT CAnalysisTimeline::OnMouseMove(UINT, WPARAM, LPARAM lp, BOOL&) {
	if (!m_Tracking) {
		TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, m_hWnd };
		m_Tracking = ::TrackMouseEvent(&tme);
	}
	POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
	int lane = LaneAt(pt);
	if (lane < 0)
		HideTip();
	else
		ShowTip(lane, pt);
	return 0;
}

LRESULT CAnalysisTimeline::OnMouseLeave(UINT, WPARAM, LPARAM, BOOL&) {
	m_Tracking = false;
	HideTip();
	return 0;
}

void CAnalysisTimeline::ShowTip(int lane, POINT pt) {
	// the text is set only when the lane changes (setting it again makes the tip repaint, which flickers)
	if (lane != m_TipLane) {
		m_TipLane = lane;
		m_TipText = m_Lanes[lane].Label + L"\n" + m_Lanes[lane].Tip;
		m_ToolInfo.lpszText = (PWSTR)(PCWSTR)m_TipText;
		m_Tip.UpdateTipText(&m_ToolInfo);
		m_Tip.TrackActivate(&m_ToolInfo, TRUE);
	}
	POINT screen = pt;
	ClientToScreen(&screen);
	m_Tip.TrackPosition(screen.x + DpiHelper::Scale(16, m_hWnd), screen.y + DpiHelper::Scale(20, m_hWnd));
}

void CAnalysisTimeline::HideTip() {
	if (m_TipLane < 0 || !m_Tip.IsWindow())
		return;
	m_TipLane = -1;
	m_Tip.TrackActivate(&m_ToolInfo, FALSE);
}

LRESULT CAnalysisTimeline::OnDoubleClick(UINT, WPARAM, LPARAM lp, BOOL&) {
	POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
	int lane = LaneAt(pt);
	if (lane >= 0 && Activated) {
		HideTip();
		Activated(m_Lanes[lane], XToJd(pt.x));
	}
	return 0;
}

std::vector<CAnalysisTimeline::Tick> CAnalysisTimeline::Ticks() const {
	std::vector<Tick> ticks;
	double span = m_To - m_From;
	auto start = FromJd(m_From);
	long year = start.Year(), month = start.Month();
	CString text;
	if (span <= 62) {
		// days
		for (double jd = DayStart(year, month, start.Day()); jd <= m_To; jd += 1) {
			if (jd < m_From)
				continue;
			auto d = FromJd(jd + 0.5);
			text.Format(L"%02ld/%02ld", d.Month(), d.Day());
			ticks.push_back({ jd, text });
		}
	}
	else if (span <= 3 * 366) {
		// months
		for (;; ) {
			double jd = DayStart(year, month, 1);
			if (jd > m_To)
				break;
			if (jd >= m_From) {
				text.Format(L"%ld/%02ld", year, month);
				ticks.push_back({ jd, text });
			}
			if (++month > 12) {
				month = 1;
				year++;
			}
		}
	}
	else {
		// years, or decades for a long range
		int step = span <= 60 * 366 ? 1 : 10;
		long y = step == 1 ? year : (long)std::floor(year / 10.0) * 10;
		for (;; y += step) {
			double jd = DayStart(y, 1, 1);
			if (jd > m_To)
				break;
			if (jd >= m_From) {
				text.Format(L"%ld", y);
				ticks.push_back({ jd, text });
			}
		}
	}
	return ticks;
}

LRESULT CAnalysisTimeline::OnPaint(UINT, WPARAM, LPARAM, BOOL&) {
	CPaintDC paint(m_hWnd);
	CRect rc;
	GetClientRect(&rc);
	CMemoryDC dc(paint, rc);
	auto colors = CurrentColors();
	dc.FillSolidRect(&rc, colors.Back);
	auto oldFont = dc.SelectFont(m_Font ? m_Font : AtlGetDefaultGuiFont());
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(colors.Text);

	int pad = DpiHelper::Scale(6, m_hWnd);

	if (m_Lanes.empty()) {
		CRect text(rc);
		text.DeflateRect(pad * 3, pad * 3);
		dc.DrawText(L"No stays within an orb to show. The timeline shows the aspects of the events the filter lets through.", -1, &text,
			DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
		dc.SelectFont(oldFont);
		return 0;
	}

	// the ticks: a grid line through the lanes and a label in the axis band, leaving out labels that would run into the one before
	auto ticks = Ticks();
	CPen gridPen;
	gridPen.CreatePen(PS_SOLID, 1, colors.Grid);
	CRect axis(rc.left, rc.top, rc.right, m_AxisHeight);
	dc.FillSolidRect(&axis, colors.Axis);

	// the lanes, below the axis band
	dc.IntersectClipRect(rc.left, m_AxisHeight, rc.right, rc.bottom);
	int first = m_Scroll / m_LaneHeight;
	int last = std::min((int)m_Lanes.size() - 1, (m_Scroll + rc.Height()) / m_LaneHeight);
	for (int i = first; i <= last; i++) {
		int top = m_AxisHeight + i * m_LaneHeight - m_Scroll;
		CRect laneRect(rc.left, top, rc.right, top + m_LaneHeight);
		if (i % 2)
			dc.FillSolidRect(&laneRect, colors.Stripe);
	}
	{
		auto oldPen = dc.SelectPen(gridPen);
		for (auto const& tick : ticks) {
			int x = JdToX(tick.Jd);
			dc.MoveTo(x, m_AxisHeight);
			dc.LineTo(x, rc.bottom);
		}
		dc.SelectPen(oldPen);
	}
	for (int i = first; i <= last; i++) {
		auto const& lane = m_Lanes[i];
		int top = m_AxisHeight + i * m_LaneHeight - m_Scroll;
		CRect label(pad, top, m_LabelWidth, top + m_LaneHeight);
		dc.SetTextColor(colors.Text);
		dc.DrawText(lane.Label, lane.Label.GetLength(), &label, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

		// the bar, a little thinner than the lane, and the exact moments across it
		int barTop = top + m_LaneHeight / 4, barBottom = top + m_LaneHeight - m_LaneHeight / 4;
		int x1 = JdToX(std::max(lane.Start, m_From)), x2 = std::max(x1 + 2, JdToX(std::min(lane.End, m_To)));
		CRect bar(x1, barTop, x2, barBottom);
		dc.FillSolidRect(&bar, lane.Color);
		// an end the range cut off fades into the edge
		auto fade = [&](int from, int to) {
			int steps = std::abs(to - from);
			for (int s = 0; s < steps; s++) {
				int x = from < to ? from + s : from - s;
				CRect strip(x, barTop, x + 1, barBottom);
				dc.FillSolidRect(&strip, Mix(lane.Color, colors.Back, double(s) / steps));
			}
		};
		int fadeWidth = std::min(DpiHelper::Scale(14, m_hWnd), (x2 - x1) / 3);
		if (lane.OpenStart)
			fade(x1 + fadeWidth, x1);
		if (lane.OpenEnd)
			fade(x2 - fadeWidth, x2);
		for (double exact : lane.Exacts) {
			int x = JdToX(exact);
			CRect mark(x - 1, top + 2, x + 2, top + m_LaneHeight - 2);
			dc.FillSolidRect(&mark, colors.Text);
		}
	}

	// today
	double now = DateTime::Now().Julian();
	if (now >= m_From && now <= m_To) {
		int x = JdToX(now);
		CRect line(x - 1, m_AxisHeight, x + 1, rc.bottom);
		dc.FillSolidRect(&line, colors.Today);
	}

	// the axis band's labels (drawn last, over nothing but the band)
	dc.SelectClipRgn(nullptr);
	dc.SetTextColor(colors.Text);
	int lastRight = INT_MIN;
	for (auto const& tick : ticks) {
		int x = JdToX(tick.Jd);
		CSize size;
		dc.GetTextExtent(tick.Text, tick.Text.GetLength(), &size);
		if (x + 3 < lastRight + pad || x + 3 + size.cx > rc.right)
			continue;
		dc.TextOut(x + 3, (m_AxisHeight - size.cy) / 2, tick.Text);
		lastRight = x + 3 + size.cx;
		auto oldPen = dc.SelectPen(gridPen);
		dc.MoveTo(x, m_AxisHeight - pad);
		dc.LineTo(x, m_AxisHeight);
		dc.SelectPen(oldPen);
	}
	dc.SelectFont(oldFont);
	return 0;
}
