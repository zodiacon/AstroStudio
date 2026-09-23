#include "pch.h"
#include "GraphicChartView.h"

LRESULT CGraphicChartView::OnEraseBkgnd(UINT, WPARAM wp, LPARAM, BOOL&) {
	return 1;
}

LRESULT CGraphicChartView::OnPaint(UINT, WPARAM, LPARAM, BOOL&) {
	CPaintDC dc(*this);		// validates the update region; drawing goes through the render target
	if (!EnsureRenderTarget())
		return 0;

	CRect rc;
	GetClientRect(&rc);
	auto size = (float)std::min(rc.right, rc.bottom);

	m_Drawing.Chart(m_ChartData);
	m_Drawing.Aspects(&m_Aspects);

	m_RenderTarget->BeginDraw();
	m_RenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::LightGray));
	m_Drawing.Draw(m_RenderTarget, size);
	if (m_RenderTarget->EndDraw() == D2DERR_RECREATE_TARGET)
		m_RenderTarget.Release();	// device lost; recreated on the next paint
	return 0;
}

LRESULT CGraphicChartView::OnSize(UINT, WPARAM, LPARAM lp, BOOL&) {
	if (m_RenderTarget)
		m_RenderTarget->Resize(D2D1::SizeU(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)));
	Invalidate();
	return 0;
}

bool CGraphicChartView::EnsureRenderTarget() {
	if (m_RenderTarget)
		return true;

	auto& resources = D2DResources::Get();
	return SUCCEEDED(resources.Ensure()) && SUCCEEDED(resources.CreateWindowRenderTarget(m_hWnd, &m_RenderTarget));
}

void CGraphicChartView::SetChartData(ChartData* data) noexcept {
	m_ChartData = data;
}

void CGraphicChartView::Refresh() {
	Invalidate();
}

void CGraphicChartView::SetAspects(std::vector<AspectData> aspects) noexcept {
	m_Aspects = std::move(aspects);
}
