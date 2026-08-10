#include "pch.h"
#include "ChartView.h"
#include "ChartDrawing.h"
#include "Helpers.h"
#include "Aspects.h"
#include "DefaultFont.h"
#include <DarkMode/DmlibColor.h>

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
	ChartData data;
	auto& info = data.Info();
	info = Frame()->DefaultChartInfo();
	info.Time = DateTime::Now();
	auto planets = Helpers::GetStandardPlanets();
	data.AddPlanets(planets);
	data.AddPlanets({ Planet::Chiron, Planet::TrueNode, Planet::Lilith });
	Chart(std::move(data));
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

	return 0;
}

LRESULT CChartView::OnEditCopy(WORD, WORD, HWND, BOOL&) {
	return 0;
}

LRESULT CChartView::OnForwardMsg(UINT, WPARAM, LPARAM lParam, BOOL& bHandled) {
	bHandled = TRUE;
	return m_DetailsTabs.PreTranslateMessage((LPMSG)lParam);
}

void CChartView::UpdateAspectGridScrollSize() {
	auto size = m_AspectGrid.NaturalSize();
	m_AspectGridScroll.SetScrollSize(size, size);
	m_AspectGridScroll.UpdateLayout();
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

