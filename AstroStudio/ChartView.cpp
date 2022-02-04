#include "pch.h"
#include "ChartView.h"
#include "ChartDrawing.h"
#include "Helpers.h"

void CChartView::OnFinalMessage(HWND) {
	delete this;
}

LRESULT CChartView::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&) {
	return TRUE;
}

LRESULT CChartView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_DrawingSize = std::min(::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN));
	DateTime dt(1971, 7, 1, 18, 10, 0, true);
	m_Data.Houses() = m_Calc.CalcHouses(dt, 47, 28 + 5 / 6.0, HouseSystem::Koch);

	for (auto p : Helpers::GetStandardPlanets()) {
		m_Data.AddPlanets({ m_Calc.CalcPlanet(p, dt) });
	}
	m_Drawing.Chart(m_Data);

	return 0;
}

LRESULT CChartView::OnPaint(UINT, WPARAM, LPARAM, BOOL&) {
	CPaintDC dc(m_hWnd);
	CairoSurface target(dc.m_hDC);
	CairoCtx ctx(target);

	if (!m_Surface) {
		m_Surface = CairoSurface::CreateImage(CairoFormat::ARGB32, m_DrawingSize, m_DrawingSize);
	}
	if (m_RedrawNeeded) {
		m_Drawing.Draw(m_Surface);
		m_RedrawNeeded = false;
	}

	CRect rc;
	GetClientRect(&rc);
	auto size = (double)std::min(rc.right, rc.bottom);
	ctx.Translate(0, (rc.bottom - size) / 2);
	ctx.Scale(size / m_DrawingSize, size / m_DrawingSize);
	ctx.Source(m_Surface, 0, 0);
	ctx.Paint();

	return 0;
}

LRESULT CChartView::OnEditCopy(WORD, WORD, HWND, BOOL&) {
	return LRESULT();
}
