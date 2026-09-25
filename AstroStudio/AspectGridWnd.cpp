#include "pch.h"
#include "AspectGridWnd.h"
#include "ChartColors.h"

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

void CAspectGridWnd::SetOverlay(ChartOverlay const* overlay) noexcept {
	m_Overlay = overlay;
}

SIZE CAspectGridWnd::NaturalSize() const noexcept {
	return m_Drawing.GridSize();
}

void CAspectGridWnd::Refresh() {
	// the wheel's colours - the program's for the look, with the user's on top - so that the grid is of a piece with the wheel
	bool dark = WTLHelper::IsDarkMode();
	auto wheel = dark ? ChartDrawingParameters::Dark() : ChartDrawingParameters();
	ChartColors::Current().Apply(wheel, dark);
	AspectGridDrawingParameters params;
	params.BackColor = wheel.BackColor;
	params.GridLineColor = wheel.GridColor;
	params.AspectColor = wheel.TextColor;
	params.SoftAspectColor = wheel.SoftAspectColor;
	params.HardAspectColor = wheel.HardAspectColor;
	params.MinorAspectColor = wheel.MinorAspectColor;
	params.OverlayColor = wheel.OverlayColor;
	m_Drawing.DrawingParameters(params);
	m_Drawing.Chart(m_ChartData);
	m_Drawing.Aspects(&m_Aspects);
	m_Drawing.Overlay(m_Overlay);

	Invalidate();
}

LRESULT CAspectGridWnd::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&) {
	Refresh();
	return 0;
}
