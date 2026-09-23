#include "pch.h"
#include "AspectGridWnd.h"

LRESULT CAspectGridWnd::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&) {
	return 1;
}

LRESULT CAspectGridWnd::OnPaint(UINT, WPARAM, LPARAM, BOOL&) {
	CPaintDC dc(*this);		// validates the update region; drawing goes through the render target
	if (!EnsureRenderTarget())
		return 0;

	m_RenderTarget->BeginDraw();
	if (FAILED(m_Drawing.Draw(m_RenderTarget)))
		m_RenderTarget->Clear(m_Drawing.DrawingParameters().BackColor);	// no chart yet
	if (m_RenderTarget->EndDraw() == D2DERR_RECREATE_TARGET)
		m_RenderTarget.Release();	// device lost; recreated on the next paint
	return 0;
}

LRESULT CAspectGridWnd::OnSize(UINT, WPARAM, LPARAM lp, BOOL&) {
	if (m_RenderTarget)
		m_RenderTarget->Resize(D2D1::SizeU(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)));
	Invalidate();
	return 0;
}

bool CAspectGridWnd::EnsureRenderTarget() {
	if (m_RenderTarget)
		return true;

	auto& resources = D2DResources::Get();
	return SUCCEEDED(resources.Ensure()) && SUCCEEDED(resources.CreateWindowRenderTarget(m_hWnd, &m_RenderTarget));
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
		params.BackColor = ColorFromRgb(30, 30, 30);
		params.GridLineColor = ColorFromRgb(90, 90, 90);
		params.AspectColor = ColorFromRgb(220, 220, 220);
	}
	m_Drawing.DrawingParameters(params);
	m_Drawing.Chart(m_ChartData);
	m_Drawing.Aspects(&m_Aspects);

	Invalidate();
}

LRESULT CAspectGridWnd::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&) {
	Refresh();
	return 0;
}
