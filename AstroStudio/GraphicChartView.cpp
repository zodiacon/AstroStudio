#include "pch.h"
#include "GraphicChartView.h"

using namespace Gdiplus;

LRESULT CGraphicChartView::OnEraseBkgnd(UINT, WPARAM wp, LPARAM, BOOL&) {
	return 1;
}

LRESULT CGraphicChartView::OnPaint(UINT, WPARAM, LPARAM, BOOL&) const {
	CRect rc;
	GetClientRect(&rc);
	auto size = std::min(rc.right, rc.bottom);

	CPaintDC dc(*this);
	Graphics g(dc.m_hDC);
	SolidBrush b(Color::LightGray);
	g.FillRectangle(&b, std::min(rc.right, size), 0, rc.right - size, rc.bottom);
	g.FillRectangle(&b, 0, std::min(rc.bottom, size), rc.right, rc.bottom - size);
	g.DrawImage(m_Bitmap.get(), Rect(0, 0, size, size));
	return 0;
}

LRESULT CGraphicChartView::OnSize(UINT, WPARAM, LPARAM lp, BOOL&) {
	int size = std::min(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
	if (m_DrawingSize == 0 || size != m_DrawingSize) {
		m_DrawingSize = size;
		m_Bitmap.reset();
		Refresh();
	}

	return 0;
}

void CGraphicChartView::SetChartData(ChartData* data) noexcept {
	m_ChartData = data;
}

void CGraphicChartView::Refresh() {
	if (!m_Bitmap)
		m_Bitmap.reset(new Bitmap(m_DrawingSize, m_DrawingSize));
	Graphics g(m_Bitmap.get());
	m_Drawing.Chart(m_ChartData);
	m_Drawing.Aspects(&m_Aspects);
	m_Drawing.Draw(g, m_DrawingSize);
	Invalidate();
}

void CGraphicChartView::SetAspects(std::vector<AspectData> aspects) noexcept {
	m_Aspects = std::move(aspects);
}
