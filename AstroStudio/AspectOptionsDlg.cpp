#include "pch.h"
#include "AspectOptionsDlg.h"
#include "Helpers.h"
#include "WTLHelper.h"

namespace {
	// the bodies that can take part in aspects
	const Planet Bodies[] = {
		Planet::Sun, Planet::Moon, Planet::Mercury, Planet::Venus, Planet::Mars, Planet::Jupiter, Planet::Saturn,
		Planet::Uranus, Planet::Neptune, Planet::Pluto, Planet::MeanNode, Planet::TrueNode, Planet::Lilith, Planet::OscuApog,
		Planet::Chiron, Planet::Pholus, Planet::Ceres, Planet::Pallas, Planet::Juno, Planet::Vesta,
	};

	CString Degrees(double value) {
		CString text;
		text.Format(L"%g", value);
		return text;
	}
}

void CAspectOptionsDlg::SetOptions(AspectOptions const& options) {
	m_Options = options;
}

AspectOptions const& CAspectOptionsDlg::GetOptions() const {
	return m_Options;
}

bool CAspectOptionsDlg::ParseOrb(CString text, float& orb) {
	text.Trim();
	if (text.IsEmpty())
		return false;
	PWSTR end;
	double value = wcstod(text, &end);
	if (*end != 0 || !(value >= 0 && value <= AspectOptions::MaxOrb))
		return false;
	orb = (float)value;
	return true;
}

LRESULT CAspectOptionsDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	CenterWindow(GetParent());

	m_SetCombo = GetDlgItem(IDC_ASP_SET);
	m_SetCombo.AddString(L"Chart aspects");
	m_SetCombo.AddString(L"Transit / overlay aspects");
	m_SetCombo.SetCurSel(0);

	m_Aspects = GetDlgItem(IDC_ASP_ASPECTS);
	m_Aspects.SetExtendedListViewStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
	m_Aspects.InsertColumn(0, L"Aspect", LVCFMT_LEFT, 88);
	m_Aspects.InsertColumn(1, L"Angle", LVCFMT_RIGHT, 56);
	m_Aspects.InsertColumn(2, L"Orb", LVCFMT_RIGHT, 40);

	m_Planets = GetDlgItem(IDC_ASP_PLANETS);
	m_Planets.SetExtendedListViewStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
	m_Planets.InsertColumn(0, L"Planet", LVCFMT_LEFT, 96);
	m_Planets.InsertColumn(1, L"Extra orb", LVCFMT_RIGHT, 60);

	GetDlgItem(IDC_ASP_MAJORORB).SendMessage(EM_LIMITTEXT, 6);
	GetDlgItem(IDC_ASP_MINORORB).SendMessage(EM_LIMITTEXT, 6);
	GetDlgItem(IDC_ASP_ORB).SendMessage(EM_LIMITTEXT, 6);
	GetDlgItem(IDC_ASP_EXTRA).SendMessage(EM_LIMITTEXT, 6);

	ShowSet();
	return TRUE;
}

void CAspectOptionsDlg::ShowSet() {
	m_Loading = true;
	auto& s = Current();
	SetDlgItemText(IDC_ASP_MAJORORB, Degrees(s.MajorAspectOrb));
	SetDlgItemText(IDC_ASP_MINORORB, Degrees(s.MinorAspectOrb));
	CheckDlgButton(IDC_ASP_MAJORONLY, s.MajorOnly);
	FillAspects();
	FillPlanets();
	m_Loading = false;
}

void CAspectOptionsDlg::FillAspects() {
	auto& s = Current();
	m_Aspects.DeleteAllItems();
	for (int i = 0; i < AspectSettings::AspectTypeCount; i++) {
		auto type = static_cast<AspectType>(i);
		int item = m_Aspects.AddItem(i, 0, Helpers::GetAspectName(type));
		CString angle;
		angle.Format(L"%.4g\u00b0", AspectCalculator::GetAspectAngle(type));
		m_Aspects.SetItemText(item, 1, angle);
		m_Aspects.SetItemText(item, 2, Degrees(s.OrbFor(type)));
		m_Aspects.SetCheckState(item, s.AspectEnabled[i]);
	}
	m_Aspects.SelectItem(0);
	SetDlgItemText(IDC_ASP_ORB, s.AspectOrb[0] >= 0 ? Degrees(s.AspectOrb[0]) : CString());
}

void CAspectOptionsDlg::FillPlanets() {
	auto& s = Current();
	m_Planets.DeleteAllItems();
	for (auto planet : Bodies) {
		int i = static_cast<int>(planet);
		int item = m_Planets.AddItem(m_Planets.GetItemCount(), 0, Helpers::GetPlanetName(planet));
		m_Planets.SetItemData(item, (DWORD_PTR)planet);
		m_Planets.SetItemText(item, 1, Degrees(s.PlanetOrbAdd[i]));
		m_Planets.SetCheckState(item, s.PlanetEnabled[i]);
	}
	m_Planets.SelectItem(0);
	SetDlgItemText(IDC_ASP_EXTRA, Degrees(s.PlanetOrbAdd[static_cast<int>(Bodies[0])]));
}

void CAspectOptionsDlg::RefreshAspectOrbs() {
	bool was = m_Loading;
	m_Loading = true;
	auto& s = Current();
	for (int i = 0; i < m_Aspects.GetItemCount(); i++)
		m_Aspects.SetItemText(i, 2, Degrees(s.OrbFor(static_cast<AspectType>(i))));
	m_Loading = was;
}

void CAspectOptionsDlg::RefreshPlanetExtra(int item) {
	bool was = m_Loading;
	m_Loading = true;
	m_Planets.SetItemText(item, 1, Degrees(Current().PlanetOrbAdd[static_cast<int>((Planet)m_Planets.GetItemData(item))]));
	m_Loading = was;
}

bool CAspectOptionsDlg::ReadGeneral(AspectSettings& settings, UINT& badControl) {
	CString text;
	GetDlgItemText(IDC_ASP_MAJORORB, text);
	if (!ParseOrb(text, settings.MajorAspectOrb)) {
		badControl = IDC_ASP_MAJORORB;
		return false;
	}
	GetDlgItemText(IDC_ASP_MINORORB, text);
	if (!ParseOrb(text, settings.MinorAspectOrb)) {
		badControl = IDC_ASP_MINORORB;
		return false;
	}
	settings.MajorOnly = IsDlgButtonChecked(IDC_ASP_MAJORONLY) == BST_CHECKED;
	return true;
}

bool CAspectOptionsDlg::Fail(UINT control, PCWSTR message) {
	AtlMessageBox(m_hWnd, message, L"Astro Studio", MB_ICONWARNING);
	GotoDlgCtrl(GetDlgItem(control));
	return false;
}

LRESULT CAspectOptionsDlg::OnSetChanged(WORD, WORD, HWND, BOOL&) {
	int set = m_SetCombo.GetCurSel();
	if (set < 0 || set == m_Set)
		return 0;
	// what was typed in the general boxes went into the model as it was typed, when it was fit to use
	m_Set = set;
	ShowSet();
	return 0;
}

LRESULT CAspectOptionsDlg::OnGeneralChanged(WORD, WORD, HWND, BOOL&) {
	if (m_Loading)
		return 0;
	auto& s = Current();
	CString text;
	GetDlgItemText(IDC_ASP_MAJORORB, text);
	ParseOrb(text, s.MajorAspectOrb);
	GetDlgItemText(IDC_ASP_MINORORB, text);
	ParseOrb(text, s.MinorAspectOrb);
	s.MajorOnly = IsDlgButtonChecked(IDC_ASP_MAJORONLY) == BST_CHECKED;
	RefreshAspectOrbs();
	return 0;
}

LRESULT CAspectOptionsDlg::OnAspectItemChanged(int, LPNMHDR pnmh, BOOL&) {
	if (m_Loading)
		return 0;
	auto view = reinterpret_cast<LPNMLISTVIEW>(pnmh);
	if (!(view->uChanged & LVIF_STATE) || view->iItem < 0 || view->iItem >= AspectSettings::AspectTypeCount)
		return 0;

	auto& s = Current();
	if ((view->uNewState ^ view->uOldState) & LVIS_STATEIMAGEMASK)
		s.AspectEnabled[view->iItem] = m_Aspects.GetCheckState(view->iItem) != FALSE;
	if ((view->uNewState & LVIS_SELECTED) && !(view->uOldState & LVIS_SELECTED)) {
		m_Loading = true;
		SetDlgItemText(IDC_ASP_ORB, s.AspectOrb[view->iItem] >= 0 ? Degrees(s.AspectOrb[view->iItem]) : CString());
		m_Loading = false;
	}
	return 0;
}

LRESULT CAspectOptionsDlg::OnAspectOrbChanged(WORD, WORD, HWND, BOOL&) {
	int item = m_Aspects.GetSelectedIndex();
	if (m_Loading || item < 0)
		return 0;
	CString text;
	GetDlgItemText(IDC_ASP_ORB, text);
	text.Trim();
	auto& s = Current();
	float orb;
	if (text.IsEmpty())
		s.AspectOrb[item] = -1;			// the general orb
	else if (ParseOrb(text, orb))
		s.AspectOrb[item] = orb;
	else
		return 0;						// not a number (yet)
	RefreshAspectOrbs();
	return 0;
}

LRESULT CAspectOptionsDlg::OnPlanetItemChanged(int, LPNMHDR pnmh, BOOL&) {
	if (m_Loading)
		return 0;
	auto view = reinterpret_cast<LPNMLISTVIEW>(pnmh);
	if (!(view->uChanged & LVIF_STATE) || view->iItem < 0 || view->iItem >= m_Planets.GetItemCount())
		return 0;

	auto& s = Current();
	int planet = static_cast<int>(m_Planets.GetItemData(view->iItem));
	if ((view->uNewState ^ view->uOldState) & LVIS_STATEIMAGEMASK)
		s.PlanetEnabled[planet] = m_Planets.GetCheckState(view->iItem) != FALSE;
	if ((view->uNewState & LVIS_SELECTED) && !(view->uOldState & LVIS_SELECTED)) {
		m_Loading = true;
		SetDlgItemText(IDC_ASP_EXTRA, Degrees(s.PlanetOrbAdd[planet]));
		m_Loading = false;
	}
	return 0;
}

LRESULT CAspectOptionsDlg::OnPlanetExtraChanged(WORD, WORD, HWND, BOOL&) {
	int item = m_Planets.GetSelectedIndex();
	if (m_Loading || item < 0)
		return 0;
	CString text;
	GetDlgItemText(IDC_ASP_EXTRA, text);
	text.Trim();
	float extra = 0;
	if (!text.IsEmpty() && !ParseOrb(text, extra))
		return 0;						// not a number (yet)
	Current().PlanetOrbAdd[static_cast<int>((Planet)m_Planets.GetItemData(item))] = extra;
	RefreshPlanetExtra(item);
	return 0;
}

LRESULT CAspectOptionsDlg::OnDefaults(WORD, WORD, HWND, BOOL&) {
	// of the set on show only; the other keeps what it has
	AspectOptions defaults;
	Current() = m_Set == 0 ? defaults.Chart : defaults.Transit;
	ShowSet();
	return 0;
}

LRESULT CAspectOptionsDlg::OnLoad(WORD, WORD, HWND, BOOL&) {
	CSimpleFileDialog dlg(TRUE, AspectOptions::Extension, nullptr, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING, AspectOptions::Filter, m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (!ok)
		return 0;

	AspectOptions loaded;
	std::wstring error;
	if (!loaded.Load(dlg.m_szFileName, error)) {
		CString message;
		message.Format(L"%s could not be loaded:\n\n%s", dlg.m_szFileName, error.c_str());
		AtlMessageBox(m_hWnd, (PCWSTR)message, L"Astro Studio", MB_ICONWARNING);
		return 0;
	}
	m_Options = loaded;
	ShowSet();
	return 0;
}

LRESULT CAspectOptionsDlg::OnSave(WORD, WORD, HWND, BOOL&) {
	// what is in the boxes must be fit to write
	UINT bad = 0;
	AspectSettings check;
	if (!ReadGeneral(check, bad)) {
		Fail(bad, L"The orb must be a number from 0 to 30 degrees.");
		return 0;
	}

	CSimpleFileDialog dlg(FALSE, AspectOptions::Extension, L"Aspects", OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING, AspectOptions::Filter, m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (!ok)
		return 0;

	std::wstring error;
	if (!m_Options.Save(dlg.m_szFileName, error)) {
		CString message;
		message.Format(L"The settings could not be saved to %s:\n\n%s", dlg.m_szFileName, error.c_str());
		AtlMessageBox(m_hWnd, (PCWSTR)message, L"Astro Studio", MB_ICONWARNING);
	}
	return 0;
}

LRESULT CAspectOptionsDlg::OnOK(WORD, WORD, HWND, BOOL&) {
	// the general orbs are in the model as far as they were fit to use; say so if the boxes hold something else
	UINT bad = 0;
	AspectSettings check;
	if (!ReadGeneral(check, bad)) {
		Fail(bad, L"The orb must be a number from 0 to 30 degrees.");
		return 0;
	}
	EndDialog(IDOK);
	return 0;
}

LRESULT CAspectOptionsDlg::OnCancel(WORD, WORD, HWND, BOOL&) {
	EndDialog(IDCANCEL);
	return 0;
}
