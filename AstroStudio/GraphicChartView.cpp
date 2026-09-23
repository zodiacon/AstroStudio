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

	if (!m_D2DFactory && FAILED(::D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_D2DFactory)))
		return false;

	CRect rc;
	GetClientRect(&rc);
	// fixed 96 DPI so a DIP is a pixel, as it was when drawing to a GDI+ bitmap
	auto props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(), 96, 96);
	return SUCCEEDED(m_D2DFactory->CreateHwndRenderTarget(props, D2D1::HwndRenderTargetProperties(m_hWnd, D2D1::SizeU(rc.right, rc.bottom)), &m_RenderTarget));
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
