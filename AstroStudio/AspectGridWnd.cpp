#include "pch.h"
#include "AspectGridWnd.h"

using namespace Gdiplus;

LRESULT CAspectGridWnd::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&) {
	return 1;
}

LRESULT CAspectGridWnd::OnPaint(UINT, WPARAM, LPARAM, BOOL&) const {
	CRect rc;
	GetClientRect(&rc);

	CPaintDC dc(*this);
	Graphics g(dc.m_hDC);
	SolidBrush b(m_Drawing.DrawingParameters().BackColor);
	g.FillRectangle(&b, 0, 0, rc.right, rc.bottom);
	if (m_Bitmap)
		g.DrawImage(m_Bitmap.get(), 0, 0);
	return 0;
}

CAspectGridWnd::CAspectGridWnd(IMainFrame* frame) : CFrameView(frame) {
}

void CAspectGridWnd::SetChartData(ChartData* data) noexcept {
	m_ChartData = data;
}

void CAspectGridWnd::SetAspects(std::vector<AspectData> aspects) noexcept {
	m_Aspects = std::move(aspects);
}

int CAspectGridWnd::NaturalSize() const noexcept {
	return m_Drawing.GridSize();
}

void CAspectGridWnd::Refresh() {
	AspectGridDrawingParameters params;
	if (WTLHelper::IsDarkMode()) {
		params.BackColor = Color(30, 30, 30);
		params.GridLineColor = Color(90, 90, 90);
		params.AspectColor = Color(220, 220, 220);
	}
	m_Drawing.DrawingParameters(params);
	m_Drawing.Chart(m_ChartData);
	m_Drawing.Aspects(&m_Aspects);

	auto size = std::max(m_Drawing.GridSize(), 1);
	m_Bitmap.reset(new Bitmap(size, size));
	Graphics g(m_Bitmap.get());
	m_Drawing.Draw(g);

	Invalidate();
}

LRESULT CAspectGridWnd::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&) {
	Refresh();
	return 0;
}
