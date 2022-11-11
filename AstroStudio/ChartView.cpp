#include "pch.h"
#include "ChartView.h"
#include "ChartDrawing.h"
#include "Helpers.h"
#include "Aspects.h"
#include "DefaultFont.h"


void CChartView::Chart(ChartData data) {
	data.Info().Time = DateTime::Now();

	AstroCalculator calc;
	calc.Calculate(data);

	m_Data = std::move(data);
	m_DetailsView.SetChartData(&m_Data);

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
	m_Drawing.Chart(&m_Data);
	m_RedrawNeeded = true;
	Invalidate();
}

void CChartView::ChartForNow() {
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

LRESULT CChartView::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&) {
	return TRUE;
}

LRESULT CChartView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_DrawingSize = std::min(::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN));
	m_DetailsView.Create(m_hWnd);
	m_DetailsView.SetNotifyWindow(m_hWnd);

	return 0;
}

void CChartView::DoPaint(CDCHandle dc) {
	CRect rc;
	GetClientRect(&rc);
	dc.FillRect(&rc, ::GetSysColorBrush(COLOR_WINDOW));

	auto size = std::min(rc.right, rc.bottom);

	using namespace Gdiplus;

	if (!m_Bitmap)
		m_Bitmap.reset(new Bitmap(m_DrawingSize, m_DrawingSize));

	if (m_RedrawNeeded) {
		Graphics g(m_Bitmap.get());
		m_Drawing.Draw(g, m_DrawingSize);
		m_RedrawNeeded = false;
	}
	Graphics g(dc.m_hDC);
	g.DrawImage(m_Bitmap.get(), Rect(0, 0, size, size));
}

LRESULT CChartView::OnEditCopy(WORD, WORD, HWND, BOOL&) {
	return LRESULT();
}

LRESULT CChartView::OnRecalc(UINT, WPARAM, LPARAM, BOOL&) {
	m_Calc.Calculate(m_Data);
	AspectCalculator ac;
	auto aspects = ac.Calculate(m_Data.AllPlanets());
	m_Drawing.Aspects(std::move(aspects));
	m_RedrawNeeded = true;
	Invalidate(FALSE);
	return 0;
}

LRESULT CChartView::OnPaint(UINT, WPARAM, LPARAM, BOOL&) {
	CClientDC dc(m_hWndClient);
	DoPaint(dc.m_hDC);
	ValidateRect(nullptr);
	return 0;
}

LRESULT CChartView::OnSize(UINT, WPARAM, LPARAM lp, BOOL& handled) {
	int cx = GET_X_LPARAM(lp), cy = GET_Y_LPARAM(lp);
	auto size = std::min(cx, cy);
	m_DetailsView.SetWindowPos(nullptr, size, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	m_RedrawNeeded = true;
	Invalidate(FALSE);
	handled = FALSE;
	return 0;
}
