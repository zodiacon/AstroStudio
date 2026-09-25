#include "pch.h"
#include "GraphicChartView.h"
#include "Helpers.h"
#include "DerivedCharts.h"
#include <DarkMode/DarkModeSubclass.h>
#include <numbers>
#include <cmath>

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
	ApplyState();

	m_RenderTarget->BeginDraw();
	// (the part of the window outside the square the chart is drawn in)
	m_RenderTarget->Clear(WTLHelper::IsDarkMode() ? ColorFromRgb(20, 20, 20) : D2D1::ColorF(D2D1::ColorF::LightGray));
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
	// A planet's tip stays and shows the new values (the chart may be moving with the clock). Aspects come and go
	// with a recalculation and their numbers with them, so what was under the mouse is looked up again on the next move.
	bool overlay = m_Hover.Type == ChartHit::Kind::OverlayPlanet;
	if (m_TipShown && (m_Hover.Type == ChartHit::Kind::Planet || overlay) && m_ChartData &&
		m_Hover.Index < (overlay ? (m_Overlay ? m_Overlay->Data.PlanetCount() : 0) : m_ChartData->PlanetCount())) {
		CPoint pt;
		::GetCursorPos(&pt);
		ScreenToClient(&pt);
		m_TipText = overlay ? OverlayPlanetTip(m_Hover.Index) : PlanetTip(m_Hover.Index);
		ShowTip(pt, m_TipText);
		return;
	}
	m_Hover = {};
	HideTip();
}

LRESULT CGraphicChartView::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL& handled) {
	Invalidate();
	handled = FALSE;		// the message goes to everything below the frame
	return 0;
}

void CGraphicChartView::ApplyState() {
	// the pictures (Export, Copy) follow the mode as well: they are what is on the screen
	m_Drawing.DrawingParameters(WTLHelper::IsDarkMode() ? ChartDrawingParameters::Dark() : ChartDrawingParameters());
	m_Drawing.Rotation(m_Rotation).Highlight(m_Selected);
	m_Drawing.Overlay(m_Overlay);
}

void CGraphicChartView::SetOverlay(ChartOverlay const* overlay) {
	m_Overlay = overlay;
	// (the caller refreshes the view)
}

void CGraphicChartView::ClearOverlay() {
	m_Overlay = nullptr;
	if (m_Selected && m_Selected->Overlay)
		m_Selected.reset();
	m_Hover = {};
	HideTip();
	Invalidate();
}

void CGraphicChartView::ResetRotation() {
	m_Rotation = 0;
	Invalidate();
}

CComPtr<IWICBitmap> CGraphicChartView::RenderImage(int size) {
	m_Drawing.Chart(m_ChartData);
	m_Drawing.Aspects(&m_Aspects);
	ApplyState();
	return ChartImage::Render(m_Drawing, size);
}

void CGraphicChartView::SetAspects(std::vector<AspectData> aspects) noexcept {
	m_Aspects = std::move(aspects);
}

//
// mouse
//

D2D1_POINT_2F CGraphicChartView::ToLogical(POINT pt) const {
	CRect rc;
	GetClientRect(&rc);
	float size = (float)std::max(1, (int)std::min(rc.right, rc.bottom));
	return D2D1::Point2F(pt.x * 1000.0f / size, pt.y * 1000.0f / size);
}

double CGraphicChartView::AngleOf(D2D1_POINT_2F const& pt) {
	// the screen's Y axis points down
	return std::atan2(D2DChartDrawing::Center.y - pt.y, pt.x - D2DChartDrawing::Center.x) * 180 / std::numbers::pi;
}

int CGraphicChartView::HouseOf(AstroPoint const& longitude) const {
	return DerivedCharts::HouseOf(m_ChartData->Houses(), longitude);
}

static CString FormatOrb(double orb) {
	int degrees = (int)orb;
	int minutes = (int)((orb - degrees) * 60 + 0.5);
	if (minutes == 60) {
		degrees++;
		minutes = 0;
	}
	CString text;
	text.Format(L"%d\u00b0%02d'", degrees, minutes);
	return text;
}

CString CGraphicChartView::PlanetTip(int index) const {
	auto const& planets = m_ChartData->AllPlanets();
	auto const& pp = planets[index];

	CString text;
	text.Format(L"%s   %s\r\n", Helpers::GetPlanetName(pp.Planet),
		(PCWSTR)Helpers::FormatLongitude(pp.Longitude, FormatOptions::ShowSeconds | FormatOptions::ShowDegreeGlyph));
	CString line;
	line.Format(L"House %d    speed %+.4f\u00b0/day", HouseOf(pp.Longitude), pp.Speed);
	text += line;

	// the planet's aspects, conjunctions included
	int shown = 0;
	for (auto const& aspect : m_Aspects) {
		bool first = aspect.Planet1.Planet == pp.Planet;
		if (!first && aspect.Planet2.Planet != pp.Planet)
			continue;
		if (++shown > 12) {
			text += L"\r\n...";
			break;
		}
		line.Format(L"\r\n%s %s   %s%s", Helpers::GetAspectName(aspect.Type),
			Helpers::GetPlanetName((first ? aspect.Planet2 : aspect.Planet1).Planet), (PCWSTR)FormatOrb(aspect.Orb),
			aspect.Applying ? L" applying" : L"");
		text += line;
	}

	// and the overlay's to it
	if (m_Overlay) {
		for (auto const& aspect : m_Overlay->Aspects) {
			if (aspect.Planet2.Planet != pp.Planet)
				continue;
			line.Format(L"\r\n%s %s %s   %s%s", m_Overlay->Label.c_str(), Helpers::GetPlanetName(aspect.Planet1.Planet), Helpers::GetAspectName(aspect.Type),
				(PCWSTR)FormatOrb(aspect.Orb), aspect.Applying ? L" applying" : L"");
			text += line;
		}
	}
	return text;
}

CString CGraphicChartView::OverlayPlanetTip(int index) const {
	auto const& pp = m_Overlay->Data.AllPlanets()[index];
	CString text;
	text.Format(L"%s %s   %s\r\n", m_Overlay->Label.c_str(), Helpers::GetPlanetName(pp.Planet),
		(PCWSTR)Helpers::FormatLongitude(pp.Longitude, FormatOptions::ShowSeconds | FormatOptions::ShowDegreeGlyph));
	CString line;
	line.Format(L"in house %d of the chart    speed %+.4f\u00b0/day", HouseOf(pp.Longitude), pp.Speed);
	text += line;

	for (auto const& aspect : m_Overlay->Aspects) {
		if (aspect.Planet1.Planet != pp.Planet)
			continue;
		line.Format(L"\r\n%s %s %s   %s%s", Helpers::GetAspectName(aspect.Type), m_Overlay->BaseLabel.c_str(), Helpers::GetPlanetName(aspect.Planet2.Planet),
			(PCWSTR)FormatOrb(aspect.Orb), aspect.Applying ? L" applying" : L"");
		text += line;
	}
	return text;
}

CString CGraphicChartView::OverlayAspectTip(int index) const {
	auto const& aspect = m_Overlay->Aspects[index];
	CString text;
	text.Format(L"%s %s %s %s %s\r\nOrb %s, %s", m_Overlay->Label.c_str(), Helpers::GetPlanetName(aspect.Planet1.Planet), Helpers::GetAspectName(aspect.Type),
		m_Overlay->BaseLabel.c_str(), Helpers::GetPlanetName(aspect.Planet2.Planet), (PCWSTR)FormatOrb(aspect.Orb), aspect.Applying ? L"applying" : L"separating");
	return text;
}

CString CGraphicChartView::AspectTip(int index) const {
	auto const& aspect = m_Aspects[index];
	CString text;
	text.Format(L"%s %s %s\r\nOrb %s, %s", Helpers::GetPlanetName(aspect.Planet1.Planet), Helpers::GetAspectName(aspect.Type),
		Helpers::GetPlanetName(aspect.Planet2.Planet), (PCWSTR)FormatOrb(aspect.Orb), aspect.Applying ? L"applying" : L"separating");
	return text;
}

void CGraphicChartView::ShowTip(POINT pt, PCWSTR text) {
	if (!m_Tip) {
		m_Tip.Create(m_hWnd, nullptr, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX);
		m_Tip.SetMaxTipWidth(450);
		m_TipTool.cbSize = sizeof(TOOLINFO);
		m_TipTool.uFlags = TTF_TRACK | TTF_ABSOLUTE;
		m_TipTool.hwnd = m_hWnd;
		m_TipTool.uId = 1;
		m_TipTool.lpszText = const_cast<LPWSTR>(L"");
		m_Tip.AddTool(&m_TipTool);
	}
	// Only touch the tooltip when something changed: setting the same text again, or moving it to the same place,
	// makes it repaint (a flash) - and this runs on every mouse move and, while the chart is live, every tick.
	if (m_TipShownText != text) {
		m_TipTool.lpszText = const_cast<LPWSTR>(text);
		m_Tip.SetToolInfo(&m_TipTool);
		m_TipShownText = text;
	}
	CPoint screen(pt);
	ClientToScreen(&screen);
	if (screen != m_TipPoint) {
		m_Tip.TrackPosition(screen.x + 16, screen.y + 20);
		m_TipPoint = screen;
	}
	if (!m_TipShown) {
		// (dark or light, whichever the program is in right now)
		DarkMode::setDarkTooltips(m_Tip, static_cast<int>(DarkMode::ToolTipsType::tooltip));
		m_Tip.TrackActivate(&m_TipTool, TRUE);
		m_TipShown = true;
	}
}

void CGraphicChartView::HideTip() {
	if (m_TipShown && m_Tip) {
		m_Tip.TrackActivate(&m_TipTool, FALSE);
		m_TipShown = false;
		m_TipShownText.Empty();
		m_TipPoint = CPoint(-1, -1);
	}
}

void CGraphicChartView::UpdateHover(POINT pt) {
	auto hit = m_ChartData ? m_Drawing.HitTest(ToLogical(pt)) : ChartHit{};
	bool same = hit.Type == m_Hover.Type && hit.Index == m_Hover.Index;
	m_Hover = hit;

	if (hit.Type == ChartHit::Kind::None) {
		HideTip();
		return;
	}
	if (!same || !m_TipShown)
		switch (hit.Type) {
			case ChartHit::Kind::Planet: m_TipText = PlanetTip(hit.Index); break;
			case ChartHit::Kind::Aspect: m_TipText = AspectTip(hit.Index); break;
			case ChartHit::Kind::OverlayPlanet: m_TipText = OverlayPlanetTip(hit.Index); break;
			default: m_TipText = OverlayAspectTip(hit.Index); break;
		}
	ShowTip(pt, m_TipText);		// also follows the mouse while it stays on the same thing
}

LRESULT CGraphicChartView::OnMouseMove(UINT, WPARAM, LPARAM lp, BOOL&) {
	CPoint pt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
	if (!m_Tracking) {
		TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, m_hWnd, 0 };
		m_Tracking = ::TrackMouseEvent(&tme);
	}

	if (m_MouseDown) {
		if (!m_Dragging && (abs(pt.x - m_DownPoint.x) > 4 || abs(pt.y - m_DownPoint.y) > 4)) {
			m_Dragging = true;
			m_Hover = {};
			HideTip();
		}
		if (m_Dragging) {
			// the wheel follows the mouse round the middle
			m_Rotation = m_DownRotation + (AngleOf(ToLogical(pt)) - m_DownAngle);
			m_Rotation = std::remainder(m_Rotation, 360.0);
			Invalidate();
		}
		return 0;
	}
	UpdateHover(pt);
	return 0;
}

LRESULT CGraphicChartView::OnMouseLeave(UINT, WPARAM, LPARAM, BOOL&) {
	m_Tracking = false;
	m_Hover = {};
	HideTip();
	return 0;
}

LRESULT CGraphicChartView::OnLButtonDown(UINT, WPARAM, LPARAM lp, BOOL&) {
	// Take the focus: Copy then means the chart picture, not whatever text box was used last.
	SetFocus();
	SetCapture();
	m_MouseDown = true;
	m_Dragging = false;
	m_DownPoint = CPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
	m_DownAngle = AngleOf(ToLogical(m_DownPoint));
	m_DownRotation = m_Rotation;
	HideTip();
	return 0;
}

LRESULT CGraphicChartView::OnLButtonUp(UINT, WPARAM, LPARAM lp, BOOL&) {
	bool wasDragging = m_Dragging;
	if (::GetCapture() == m_hWnd)
		::ReleaseCapture();		// WM_CAPTURECHANGED clears the button state
	if (wasDragging || !m_ChartData)
		return 0;

	// a click: pick the planet under the mouse (again to let go of it), or let go of the planet on an empty spot
	CPoint pt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
	auto hit = m_Drawing.HitTest(ToLogical(pt));
	if (hit.Type == ChartHit::Kind::Planet || hit.Type == ChartHit::Kind::OverlayPlanet) {
		bool overlay = hit.Type == ChartHit::Kind::OverlayPlanet;
		ChartSelection picked{ (overlay ? m_Overlay->Data : *m_ChartData).AllPlanets()[hit.Index].Planet, overlay };
		m_Selected = m_Selected == picked ? std::nullopt : std::optional<ChartSelection>(picked);
	}
	else if (hit.Type == ChartHit::Kind::None) {
		m_Selected.reset();
		// two clicks on an empty spot turn the wheel back
		DWORD now = ::GetMessageTime();
		if (now - m_LastClickTime <= ::GetDoubleClickTime() && abs(pt.x - m_LastClickPoint.x) <= 4 && abs(pt.y - m_LastClickPoint.y) <= 4) {
			m_Rotation = 0;
			m_LastClickTime = 0;
		}
		else {
			m_LastClickTime = now;
			m_LastClickPoint = pt;
		}
	}
	Invalidate();
	return 0;
}

LRESULT CGraphicChartView::OnCaptureChanged(UINT, WPARAM, LPARAM, BOOL&) {
	m_MouseDown = false;
	m_Dragging = false;
	return 0;
}

LRESULT CGraphicChartView::OnSetCursor(UINT, WPARAM, LPARAM lp, BOOL& handled) {
	if (LOWORD(lp) != HTCLIENT) {
		handled = FALSE;
		return 0;
	}
	PCWSTR cursor = m_Dragging ? IDC_SIZEALL : (m_Hover.Type != ChartHit::Kind::None ? IDC_HAND : IDC_ARROW);
	::SetCursor(::LoadCursor(nullptr, cursor));
	return TRUE;
}
