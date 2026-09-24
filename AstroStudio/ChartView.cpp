#include "pch.h"
#include "ChartView.h"
#include "TimeZones.h"
#include "Helpers.h"
#include "Aspects.h"
#include "DefaultFont.h"
#include <DarkMode/DmlibColor.h>
#include <DarkMode/DarkModeSubclass.h>

CChartView::CChartView(IMainFrame* frame) : CFrameView(frame), m_ChartDrawing(frame), m_AspectGrid(frame), m_AspectList(frame) {
}

BOOL CChartView::PreTranslateMessage(MSG* pMsg) {
	return m_DetailsTabs.PreTranslateMessage(pMsg);
}

void CChartView::Chart(ChartData data) {
	AstroCalculator calc;
	calc.Calculate(data);
	m_Data = std::move(data);
	m_DetailsView.SetChartData(&m_Data);
	m_ChartDrawing.SetChartData(&m_Data);
	m_AspectGrid.SetChartData(&m_Data);

	AspectCalculator ac;
	auto aspects = ac.Calculate(m_Data.AllPlanets());
	m_AspectList.SetAspects(aspects);
	m_AspectList.Refresh();
	m_AspectGrid.SetAspects(aspects);
	m_AspectGrid.Refresh();
	UpdateAspectGridScrollSize();
	m_ChartDrawing.SetAspects(std::move(aspects));
	m_ChartDrawing.Refresh();
}

void CChartView::ChartForNow() {
	auto info = Frame()->DefaultChartInfo();
	info.Time = DateTime::Now();
	info.TimeZone = TimeZones::Machine();

	// The chart opens straight away; if the location hasn't arrived yet it is a
	// placeholder and the details view says so until WM_LOCATION_UPDATED.
	m_AwaitingLocation = Frame()->IsLocationPending();

	Chart(Helpers::CreateChartData(std::move(info)));
	m_DetailsView.SetLocationPending(m_AwaitingLocation);
}

LRESULT CChartView::OnLocationUpdated(UINT, WPARAM wParam, LPARAM, BOOL&) {
	if (!m_AwaitingLocation)
		return 0;

	m_AwaitingLocation = false;

	// Don't stomp a location the user typed while the lookup was still running.
	if (!wParam || m_DetailsView.IsLocationEdited()) {
		m_DetailsView.SetLocationPending(false);
		return 0;
	}

	auto const& source = Frame()->DefaultChartInfo();
	auto& info = m_Data.Info();
	info.Latitude = source.Latitude;
	info.Longitude = source.Longitude;
	info.Elevation = source.Elevation;
	info.City = source.City;
	info.State = source.State;
	info.Country = source.Country;

	// The time is deliberately left alone - it was captured when the chart was
	// created, and shifting it now would silently change the chart.
	SendMessage(WM_RECALC, static_cast<WPARAM>(Recalc::Houses));

	// Has to come after the copy above: SetLocationPending refreshes the
	// latitude/longitude fields and the location text from the chart's info,
	// and UpdateControls does not touch them at all.
	m_DetailsView.SetLocationPending(false);
	m_DetailsView.UpdateControls();

	return 0;
}

ChartData const& CChartView::Chart() const {
	return m_Data;
}

void CChartView::DisplayPlanets(CDCHandle dc, int x, int y) const {
	CFont font;
	font.CreatePointFont(110, L"HamburgSymbols");
	dc.SelectFont(font);

	for (auto& p : m_Data.AllPlanets()) {
		dc.TextOut(x, y, DefaultFont::Get().GetPlanetGlyphAsString(p.Planet), -1);
		dc.TextOut(x + 20, y, Helpers::FormatLongitude(p.Longitude,
			FormatOptions::ShowDegreeGlyph | FormatOptions::ShowSeconds | FormatOptions::UseGlyphs), -1);
		y += 22;
	}
}

void CChartView::DisplayHouses(CDCHandle dc, int x, int y) const {
	CFont font;
	font.CreatePointFont(110, L"HamburgSymbols");
	dc.SelectFont(font);

	auto& houses = m_Data.Houses();
	int i = 1;
	CString text;
	dc.TextOut(x, y, L"Z  " + Helpers::FormatLongitude(houses.Asc, FormatOptions::ShowDegreeGlyph | FormatOptions::ShowSeconds | FormatOptions::UseGlyphs));
	y += 22;
	dc.TextOut(x, y, L"X  " + Helpers::FormatLongitude(houses.MC, FormatOptions::ShowDegreeGlyph | FormatOptions::ShowSeconds | FormatOptions::UseGlyphs));
	y += 44;

	for (auto& cusp : houses.Cusps) {
		text.Format(L"%02d.  %s", i++, (PCWSTR)Helpers::FormatLongitude(cusp, FormatOptions::ShowDegreeGlyph | FormatOptions::ShowSeconds | FormatOptions::UseGlyphs));
		dc.TextOut(x, y - 3, text, text.GetLength());
		y += 22;
	}
}

LRESULT CChartView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, WS_EX_TRANSPARENT);
	m_ChartDrawing.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_ChartDrawing.SetStatic();

	m_DetailsTabs.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

	m_DetailsView.Create(m_DetailsTabs);
	m_DetailsView.SetNotifyWindow(m_hWnd);

	m_AspectGridScroll.Create(m_DetailsTabs, rcDefault, nullptr, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_HSCROLL | WS_VSCROLL);
	m_AspectGrid.Create(m_AspectGridScroll, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_AspectGrid.SetStatic();
	m_AspectGridScroll.SetClient(m_AspectGrid);

	m_AspectList.Create(m_DetailsTabs, rcDefault, nullptr, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_AspectList.SetStatic();

	m_DetailsTabs.AddPage(m_DetailsView.m_hWnd, L"Details");
	m_DetailsTabs.AddPage(m_AspectGridScroll.m_hWnd, L"Aspect Grid");
	m_DetailsTabs.AddPage(m_AspectList.m_hWnd, L"Aspect List");
	m_DetailsTabs.SetActivePage(0);

	m_Splitter.SetSplitterPanes(m_ChartDrawing, m_DetailsTabs);
	m_Splitter.SetSplitterPosPct(50);

	DarkMode::setDarkWndNotifySafe(m_hWnd);
	UpdateAspectGridScrollBarTheme();

	return 0;
}

LRESULT CChartView::OnEditCopy(WORD, WORD, HWND, BOOL&) {
	return 0;
}

LRESULT CChartView::OnForwardMsg(UINT, WPARAM, LPARAM lParam, BOOL& bHandled) {
	bHandled = TRUE;
	return m_DetailsTabs.PreTranslateMessage((LPMSG)lParam);
}

LRESULT CChartView::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&) {
	UpdateAspectGridScrollBarTheme();
	return 0;
}

void CChartView::UpdateAspectGridScrollSize() {
	auto size = m_AspectGrid.NaturalSize();
	m_AspectGridScroll.SetScrollSize(size, size);
	m_AspectGridScroll.UpdateLayout();
}

void CChartView::UpdateAspectGridScrollBarTheme() {
	// CScrollContainer is a plain custom-class window, not a recognized common control,
	// so DarkMode::setDarkWndNotifySafe's generic child-control theming doesn't reach its
	// native scroll bars - they need to be themed explicitly.
	DarkMode::setDarkScrollBar(m_AspectGridScroll.m_hWnd);
	m_AspectGridScroll.Invalidate();
}

LRESULT CChartView::OnRecalc(UINT, WPARAM wp, LPARAM, BOOL&) {
	switch (static_cast<Recalc>(wp)) {
	case Recalc::Houses:
		m_Data.CalcHouses(m_Calc);
		break;

	case Recalc::Planets:
		m_Data.CalcPlanets(m_Calc);
		break;

	case Recalc::All:
		m_Data.CalcPlanets(m_Calc);
		m_Data.CalcHouses(m_Calc);
		break;
	}
	AspectCalculator ac;
	auto aspects = ac.Calculate(m_Data.AllPlanets());
	m_AspectList.SetAspects(aspects);
	m_AspectList.Refresh();
	m_AspectGrid.SetAspects(aspects);
	m_AspectGrid.Refresh();
	UpdateAspectGridScrollSize();
	m_ChartDrawing.SetAspects(std::move(aspects));
	m_ChartDrawing.Refresh();
	return 0;
}

