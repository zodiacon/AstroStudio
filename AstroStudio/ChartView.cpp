#include "pch.h"
#include "ChartView.h"
#include "ChartDrawing.h"
#include "Helpers.h"
#include "Aspects.h"
#include "DefaultFont.h"

void CChartView::OnFinalMessage(HWND) {
	delete this;
}

void CChartView::Chart(ChartData const& chart) {
	m_Data = chart;
	Invalidate();
}

void CChartView::Chart(ChartData data) {
	m_Data = std::move(data);
}

ChartData const& CChartView::Chart() const {
	return m_Data;
}

void CChartView::DisplayPlanets(CDCHandle dc, int x, int y) {
	CFont font;
	font.CreatePointFont(110, L"HamburgSymbols");
	dc.SelectFont(font);

	for (auto& p : m_Data.AllPlanets()) {
		dc.TextOut(x, y, DefaultFont::Get().GetPlanetGlyphAsString(p.Planet), -1);
		dc.TextOut(x + 20, y, Helpers::FormatLongitude(p.Longitude, FormatOptions::ShowDegreeGlyph | FormatOptions::ShowSeconds | FormatOptions::UseGlyphs), -1);
		y += 22;
	}
}

void CChartView::DisplayHouses(CDCHandle dc, int x, int y) {
	CFont font, font2;
	font.CreatePointFont(110, L"HamburgSymbols");
	font2.CreatePointFont(110, L"Consolas");

	int i = 1;
	for (auto& cusp : m_Data.Houses().Cusps) {
		dc.SelectFont(font2);
		CString text;
		text.Format(L"H %2d:", i++);
		dc.TextOut(x, y - 3, text, text.GetLength());
		dc.SelectFont(font);
		dc.TextOut(x + 50, y, Helpers::FormatLongitude(cusp, FormatOptions::ShowDegreeGlyph | FormatOptions::ShowSeconds | FormatOptions::UseGlyphs), -1);
		y += 22;
	}
}

LRESULT CChartView::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&) {
	return TRUE;
}

LRESULT CChartView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_DrawingSize = std::min(::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN));
	//auto dt = DateTime::Now();
	DateTime dt(1971, 7, 1, 18, 10, 0, true);
	m_Data.Houses(AstroCalculator::CalcHouses(dt, 47, 28 + 5 / 6.0, HouseSystem::Koch));

	auto planets = Helpers::GetStandardPlanets();
	planets.push_back(PlanetType::Chiron);
	planets.push_back(PlanetType::TrueNode);
	planets.push_back(PlanetType::Lilith);

	for (auto p : planets) {
		m_Data.AddPlanets({ m_Calc.CalcPlanet(p, dt) });
	}
	AspectCalculator ac;
	auto aspects = ac.Calculate(m_Data.AllPlanets());
	m_Drawing.Aspects(std::move(aspects));
	m_Drawing.Chart(m_Data);

	return 0;
}

void CChartView::DoPaint(CDCHandle dc) {
	//CairoSurface target(dc.m_hDC);
	//CairoCtx ctx(target);

	//if (!m_Surface) {
	//	m_Surface = CairoSurface::CreateImage(CairoFormat::ARGB32, m_DrawingSize, m_DrawingSize);
	//}
	//if (m_RedrawNeeded) {
	//	m_Drawing.Draw(m_Surface);
	//	m_RedrawNeeded = false;
	//}

	CRect rc;
	GetClientRect(&rc);
	auto size = std::min(rc.right, rc.bottom);
	int x = size + 30;

	using namespace Gdiplus;

	//ctx.Translate(0, (rc.bottom - size) / 2);
	//ctx.Scale(size / m_DrawingSize, size / m_DrawingSize);
	//ctx.Source(m_Surface, 0, 0);
	//ctx.Paint();

	//dc.FillRect(CRect(x - 30, 0, rc.right, rc.bottom), ::GetSysColorBrush(COLOR_WINDOW));
	if (!m_Bitmap)
		m_Bitmap.reset(new Bitmap(2000, 2000));

	{
		Graphics g(m_Bitmap.get());
		m_Drawing.Draw(g, 2000);
	}
	Graphics g(dc.m_hDC);
	g.DrawImage(m_Bitmap.get(), Rect(0, 0, size, size));
	
	DisplayPlanets(dc.m_hDC, x, 40);
	DisplayHouses(dc.m_hDC, x, 50 + m_Data.PlanetsCount() * 25);
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
