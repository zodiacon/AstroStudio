#include "pch.h"
#include "ChartDetailsView.h"
#include <AstroCalculator.h>
#include <ChartData.h>
#include "StringHelper.h"
#include "Interfaces.h"
#include "DefaultFont.h"
#include "Helpers.h"

BOOL CChartDetailsView::PreTranslateMessage(MSG* pMsg) {
	return IsDialogMessage(pMsg);
}

void CChartDetailsView::SetChartData(ChartData* data) {
	m_Data = data;
	UpdateControls();
}

void CChartDetailsView::SetNotifyWindow(HWND hWnd) {
	m_NotifyWnd = hWnd;
}

CString CChartDetailsView::GetColumnText(HWND, int row, int col) const {
	auto& pos = m_Data->Planet(row);
	switch (GetColumnManager(m_ctlPlanets)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Planet: return DefaultFont::Get().GetPlanetGlyphAsString(pos.Planet);
		case ColumnType::Longitude: return Helpers::FormatLongitude(pos.Longitude, FormatOptions::ShowSeconds | FormatOptions::UseGlyphs);
		case ColumnType::Latitude: return Helpers::FormatLatitude(pos.Latitude);
	}
	return CString();
}

DWORD CChartDetailsView::OnPrePaint(int, LPNMCUSTOMDRAW cd) {
	if (cd->hdr.hwndFrom == m_ctlPlanets)
		return CDRF_NOTIFYITEMDRAW;
	return 0;
}

DWORD CChartDetailsView::OnItemPrePaint(int, LPNMCUSTOMDRAW cd) {
	if (cd->hdr.hwndFrom == m_ctlPlanets)
		return CDRF_NOTIFYSUBITEMDRAW;
	return 0;
}

DWORD CChartDetailsView::OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) {
	auto lv = (LPNMLVCUSTOMDRAW)cd;
	::SelectObject(cd->hdc, lv->iSubItem < 2 ? m_Font.m_hFont : m_ctlPlanets.GetFont());
	return CDRF_NEWFONT;
}

void CChartDetailsView::UpdateControls() {
	ATLASSERT(m_Data);
	m_ctlHouseSystem.SelectString(-1, StringHelper::HouseSystemToString(m_Data->GetHouseSystem()));
	auto st = m_Data->Info().Time.AsSystemTime();
	m_ctlDate.SetSystemTime(GDT_VALID, &st);
	m_ctlTime.SetSystemTime(GDT_VALID, &st);
	m_ctlPlanets.SetItemCountEx((int)m_Data->AllPlanets().size(), LVSICF_NOSCROLL);
}

LRESULT CChartDetailsView::OnInitView(UINT, WPARAM, LPARAM, BOOL&) {
	m_ctlHouseSystem.Attach(GetDlgItem(IDC_HOUSESYSTEM));
	m_ctlDate.Attach(GetDlgItem(IDC_DATE));
	m_ctlTime.Attach(GetDlgItem(IDC_TIME));
	m_ctlPlanets.Attach(GetDlgItem(IDC_PLANETS));
	m_ctlPlanets.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
	m_Font.CreatePointFont(100, L"HamburgSymbols");

	HouseSystem systems[] = {
		HouseSystem::Placidus,
		HouseSystem::Koch,
		HouseSystem::Porphyrius,
		HouseSystem::Regiomontanus,
		HouseSystem::Campanus,
		HouseSystem::Equal,
		HouseSystem::Morinus,
		HouseSystem::Topocentric,
		HouseSystem::Alcabitus,
		HouseSystem::Horizontal,
		HouseSystem::Krusinski,
		HouseSystem::EqualWholeSign,
		HouseSystem::CarterPoliEqu,
		HouseSystem::EqualMC,
		HouseSystem::Sunshine,
		HouseSystem::SunshineAlt,
		HouseSystem::APCHouses,
	};

	for (int i = 0; i < _countof(systems); i++) {
		int n = m_ctlHouseSystem.AddString(StringHelper::HouseSystemToString(systems[i]));
		m_ctlHouseSystem.SetItemData(n, (int)systems[i]);
	}

	auto cm = GetColumnManager(m_ctlPlanets);
	cm->AddColumn(L"P", 0, 30, ColumnType::Planet);
	cm->AddColumn(L"Longitude", 0, 100, ColumnType::Longitude);
	cm->AddColumn(L"Latitude", 0, 90, ColumnType::Latitude);
	cm->UpdateColumns();

	return 0;
}

LRESULT CChartDetailsView::OnHouseSystemChanged(WORD, WORD, HWND, BOOL&) {
	m_Data->SetHouseSystem((HouseSystem)m_ctlHouseSystem.GetItemData(m_ctlHouseSystem.GetCurSel()));
	if (m_NotifyWnd)
		m_NotifyWnd.PostMessageW(WM_RECALC);
	return 0;
}

LRESULT CChartDetailsView::OnDateChanged(int, LPNMHDR, BOOL&) {
	SYSTEMTIME st;
	m_ctlDate.GetSystemTime(&st);
	auto& info = m_Data->Info();
	info.Time.SetDate(st.wYear, st.wMonth, st.wDay);
	if (m_NotifyWnd)
		m_NotifyWnd.PostMessageW(WM_RECALC);
	return 0;
}

LRESULT CChartDetailsView::OnTimeChanged(int, LPNMHDR, BOOL&) {
	SYSTEMTIME st;
	m_ctlTime.GetSystemTime(&st);
	auto& info = m_Data->Info();
	info.Time.SetTime(st.wHour, st.wMinute, st.wSecond);
	if (m_NotifyWnd)
		m_NotifyWnd.PostMessageW(WM_RECALC);
	return 0;
}
