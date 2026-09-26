#include "pch.h"
#include "MidpointOptionsDlg.h"
#include "Helpers.h"

void CMidpointOptionsDlg::FillOrbs(CComboBox& combo, int orb) {
	for (int i = 0; i < _countof(MidpointSettings::Orbs); i++) {
		CString text;
		text.Format(L"%g%c", MidpointSettings::Orbs[i] / 100.0, 0xb0);
		combo.SetItemData(combo.AddString(text), MidpointSettings::Orbs[i]);
	}
	combo.SetCurSel(MidpointSettings::NearestOrb(orb));
}

void CMidpointOptionsDlg::FillKinds(CComboBox& combo, ContactKind kind, bool with45) {
	CString dial;
	dial.Format(L"90%c dial (on, semi-square, square...)", 0xb0);
	combo.SetItemData(combo.AddString(dial), static_cast<DWORD_PTR>(ContactKind::Dial90));
	if (with45) {
		dial.Format(L"45%c dial (all of those together)", 0xb0);
		combo.SetItemData(combo.AddString(dial), static_cast<DWORD_PTR>(ContactKind::Dial45));
	}
	combo.SetItemData(combo.AddString(L"Axis (on and opposite)"), static_cast<DWORD_PTR>(ContactKind::Axis));
	for (int i = 0; i < combo.GetCount(); i++)
		if (static_cast<ContactKind>(combo.GetItemData(i)) == kind)
			combo.SetCurSel(i);
}

LRESULT CMidpointOptionsDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	CenterWindow(GetParent());

	m_Points = GetDlgItem(IDC_MP_POINTS);
	m_Points.SetExtendedListViewStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
	m_Points.InsertColumn(0, L"", LVCFMT_LEFT, 100);
	for (int i = 0; i < static_cast<int>(Planet::NumPlanets); i++) {
		auto planet = static_cast<Planet>(i);
		if (planet == Planet::Earth || planet == Planet::PartOfFortune)
			continue;		// (the charts are seen from the one, and the other is a point worked out from the Sun, Moon and Ascendant)
		int item = m_Points.AddItem(m_Points.GetItemCount(), 0, Helpers::GetPlanetName(planet));
		m_Points.SetItemData(item, i);
		m_Points.SetCheckState(item, m_Options.TakesPart(planet));
	}
	int item = m_Points.AddItem(m_Points.GetItemCount(), 0, L"Ascendant");
	m_Points.SetItemData(item, AscendantItem);
	m_Points.SetCheckState(item, m_Options.Ascendant);
	item = m_Points.AddItem(m_Points.GetItemCount(), 0, L"Midheaven");
	m_Points.SetItemData(item, MidheavenItem);
	m_Points.SetCheckState(item, m_Options.Midheaven);
	// the column takes the whole width, and leaves room for the scroll bar
	CRect rc;
	m_Points.GetClientRect(&rc);
	m_Points.SetColumnWidth(0, rc.Width() - ::GetSystemMetrics(SM_CXVSCROLL));

	CComboBox combo;
	combo = GetDlgItem(IDC_MP_LIST_ORB);
	FillOrbs(combo, m_Options.ListOrb);
	combo = GetDlgItem(IDC_MP_LIST_KIND);
	FillKinds(combo, m_Options.ListKind, true);
	combo = GetDlgItem(IDC_MP_TREE_ORB);
	FillOrbs(combo, m_Options.TreeOrb);
	combo = GetDlgItem(IDC_MP_TREE_KIND);
	FillKinds(combo, m_Options.TreeKind, true);
	return TRUE;
}

LRESULT CMidpointOptionsDlg::OnOK(WORD, WORD, HWND, BOOL&) {
	MidpointSettings options;
	for (int i = 0; i < m_Points.GetItemCount(); i++) {
		auto data = static_cast<int>(m_Points.GetItemData(i));
		bool checked = m_Points.GetCheckState(i);
		if (data == AscendantItem)
			options.Ascendant = checked;
		else if (data == MidheavenItem)
			options.Midheaven = checked;
		else
			options.HiddenPlanets[data] = !checked;
	}
	// (bodies the list doesn't show keep what they had)
	for (auto planet : { Planet::Earth, Planet::PartOfFortune })
		options.HiddenPlanets[static_cast<size_t>(planet)] = m_Options.HiddenPlanets[static_cast<size_t>(planet)];

	CComboBox combo;
	auto orbOf = [&](int id) {
		combo = GetDlgItem(id);
		return static_cast<int>(combo.GetItemData(combo.GetCurSel()));
	};
	auto kindOf = [&](int id) {
		combo = GetDlgItem(id);
		return static_cast<ContactKind>(combo.GetItemData(combo.GetCurSel()));
	};
	options.ListOrb = orbOf(IDC_MP_LIST_ORB);
	options.ListKind = kindOf(IDC_MP_LIST_KIND);
	options.TreeOrb = orbOf(IDC_MP_TREE_ORB);
	options.TreeKind = kindOf(IDC_MP_TREE_KIND);
	m_Options = options;
	EndDialog(IDOK);
	return 0;
}

LRESULT CMidpointOptionsDlg::OnCancel(WORD, WORD, HWND, BOOL&) {
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CMidpointOptionsDlg::OnCheckAll(WORD, WORD id, HWND, BOOL&) {
	for (int i = 0; i < m_Points.GetItemCount(); i++)
		m_Points.SetCheckState(i, id == IDC_MP_ALL);
	return 0;
}
