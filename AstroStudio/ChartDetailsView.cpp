#include "pch.h"
#include "ChartDetailsView.h"
#include <AstroCalculator.h>
#include <ChartData.h>
#include "StringHelper.h"
#include "Interfaces.h"

BOOL CChartDetailsView::PreTranslateMessage(MSG* pMsg) {
	return ::IsDialogMessage(m_hWnd, pMsg);
}

void CChartDetailsView::SetChartData(ChartData* data) {
	m_Data = data;
	UpdateControls();
}

void CChartDetailsView::SetNotifyWindow(HWND hWnd) {
	m_NotifyWnd = hWnd;
}

void CChartDetailsView::UpdateControls() {
	ATLASSERT(m_Data);
	m_ctlHouseSystem.SelectString(-1, StringHelper::HouseSystemToString(m_Data->GetHouseSystem()));
}

LRESULT CChartDetailsView::OnInitView(UINT, WPARAM, LPARAM, BOOL&) {
	m_ctlHouseSystem.Attach(GetDlgItem(IDC_HOUSESYSTEM));

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

	m_ctlHouseSystem.SetCurSel(0);
	return 0;
}

LRESULT CChartDetailsView::OnHouseSystemChanged(WORD, WORD, HWND, BOOL&) {
	m_Data->SetHouseSystem((HouseSystem)m_ctlHouseSystem.GetItemData(m_ctlHouseSystem.GetCurSel()));
	if (m_NotifyWnd)
		m_NotifyWnd.PostMessageW(WM_RECALC);
	return 0;
}
