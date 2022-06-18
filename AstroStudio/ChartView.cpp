#include "pch.h"
#include "ChartView.h"
#include "ChartDrawing.h"
#include "Helpers.h"
#include "Aspects.h"
#include "DefaultFont.h"

void CChartView::OnFinalMessage(HWND) {
	delete this;
}

void CChartView::Chart(ChartData data) {
	m_Data = std::move(data);
	//DateTime dt(1971, 7, 1, 18, 10, 0, true);
	//m_Data.Houses(AstroCalculator::CalcHouses(dt, 47, 28 + 5 / 6.0, HouseSystem::Koch));

	//auto planets = Helpers::GetStandardPlanets();
	//planets.push_back(PlanetType::Chiron);
	//planets.push_back(PlanetType::TrueNode);
	//planets.push_back(PlanetType::Lilith);

	//for (auto p : planets) {
	//	m_Data.AddPlanets({ m_Calc.CalcPlanet(p, dt) });
	//}

	AspectCalculator ac;
	auto aspects = ac.Calculate(m_Data.AllPlanets());
	m_Drawing.Aspects(std::move(aspects));
	m_Drawing.Chart(m_Data);
	m_RedrawNeeded = true;
	Invalidate();
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
		dc.TextOut(x + 20, y, Helpers::FormatLongitude(p.Longitude, FormatOptions::ShowDegreeGlyph | FormatOptions::ShowSeconds | FormatOptions::UseGlyphs), -1);
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

LRESULT CChartView::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&) {
	return TRUE;
}

LRESULT CChartView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_DrawingSize = std::min(::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN));
	//auto dt = DateTime::Now();

	return 0;
}

void CChartView::DoPaint(CDCHandle dc) {
	CRect rc;
	GetClientRect(&rc);
	auto size = std::min(rc.right, rc.bottom);
	int x = size + 30;

	using namespace Gdiplus;

	if (!m_Bitmap)
		m_Bitmap.reset(new Bitmap(m_DrawingSize, m_DrawingSize));

	if(m_RedrawNeeded) {
		Graphics g(m_Bitmap.get());
		m_Drawing.Draw(g, m_DrawingSize);
		m_RedrawNeeded = false;
	}
	Graphics g(dc.m_hDC);
	g.DrawImage(m_Bitmap.get(), Rect(0, 0, size, size));
	
	DisplayPlanets(dc.m_hDC, x, 40);
	DisplayHouses(dc.m_hDC, x, 50 + m_Data.PlanetCount() * 25);
}

LRESULT CChartView::OnEditCopy(WORD, WORD, HWND, BOOL&) {
	return LRESULT();
}

LRESULT CChartView::OnPaint(UINT, WPARAM, LPARAM, BOOL&) {
	CPaintDC dc(m_hWnd);
	DoPaint(dc.m_hDC);
	return 0;
}

LRESULT CChartView::OnSize(UINT, WPARAM, LPARAM, BOOL& handled) {
	//m_RedrawNeeded;
	handled = FALSE;
	return 0;
}
