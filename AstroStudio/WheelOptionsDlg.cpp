#include "pch.h"
#include "WheelOptionsDlg.h"
#include "Helpers.h"

namespace {
	// the aspects that have a line (all but the conjunction), in the order of AspectType
	constexpr int FirstLineAspect = 1;
}

LRESULT CWheelOptionsDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	CenterWindow(GetParent());

	m_Planets = GetDlgItem(IDC_WHEEL_PLANETS);
	m_Aspects = GetDlgItem(IDC_WHEEL_ASPECTS);
	for (auto* list : { &m_Planets, &m_Aspects }) {
		list->SetExtendedListViewStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
		list->InsertColumn(0, L"", LVCFMT_LEFT, 100);
	}

	for (int i = 0; i < static_cast<int>(Planet::NumPlanets); i++) {
		auto planet = static_cast<Planet>(i);
		if (planet == Planet::Earth)
			continue;		// (the charts are seen from it)
		int item = m_Planets.AddItem(m_Planets.GetItemCount(), 0, Helpers::GetPlanetName(planet));
		m_Planets.SetItemData(item, i);
		m_Planets.SetCheckState(item, m_Options.ShowsPlanet(planet));
	}
	for (int i = FirstLineAspect; i < AspectSettings::AspectTypeCount; i++) {
		auto type = static_cast<AspectType>(i);
		int item = m_Aspects.AddItem(m_Aspects.GetItemCount(), 0, Helpers::GetAspectName(type));
		m_Aspects.SetItemData(item, i);
		m_Aspects.SetCheckState(item, m_Options.ShowsAspect(type));
	}
	// each column takes the whole width, and leaves room for the scroll bar
	for (auto* list : { &m_Planets, &m_Aspects }) {
		CRect rc;
		list->GetClientRect(&rc);
		list->SetColumnWidth(0, rc.Width() - ::GetSystemMetrics(SM_CXVSCROLL));
	}

	CheckDlgButton(IDC_WHEEL_BEYOND, m_Options.BeyondPluto);
	return TRUE;
}

LRESULT CWheelOptionsDlg::OnOK(WORD, WORD, HWND, BOOL&) {
	WheelOptions options;
	for (int i = 0; i < m_Planets.GetItemCount(); i++)
		options.HiddenPlanets[m_Planets.GetItemData(i)] = !m_Planets.GetCheckState(i);
	for (int i = 0; i < m_Aspects.GetItemCount(); i++)
		options.HiddenAspects[m_Aspects.GetItemData(i)] = !m_Aspects.GetCheckState(i);
	options.BeyondPluto = IsDlgButtonChecked(IDC_WHEEL_BEYOND) == BST_CHECKED;
	m_Options = options;
	EndDialog(IDOK);
	return 0;
}

LRESULT CWheelOptionsDlg::OnCancel(WORD, WORD, HWND, BOOL&) {
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CWheelOptionsDlg::OnCheckAll(WORD, WORD id, HWND, BOOL&) {
	bool planets = id == IDC_WHEEL_PLANETS_ALL || id == IDC_WHEEL_PLANETS_NONE;
	bool all = id == IDC_WHEEL_PLANETS_ALL || id == IDC_WHEEL_ASPECTS_ALL;
	auto& list = planets ? m_Planets : m_Aspects;
	for (int i = 0; i < list.GetItemCount(); i++)
		list.SetCheckState(i, all);
	return 0;
}
