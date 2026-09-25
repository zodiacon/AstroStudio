#include "pch.h"
#include "ChartColorsDlg.h"
#include <WTLHelper.h>

namespace {
	constexpr int ElementCount = static_cast<int>(ChartColor::Count);

	// black or white text, whichever reads on a colour
	COLORREF TextOn(COLORREF color) {
		int luminance = (GetRValue(color) * 299 + GetGValue(color) * 587 + GetBValue(color) * 114) / 1000;
		return luminance > 140 ? RGB(0, 0, 0) : RGB(255, 255, 255);
	}
}

LRESULT CChartColorsDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	CenterWindow(GetParent());

	CComboBox mode;
	mode = GetDlgItem(IDC_COLOR_MODE);
	mode.AddString(L"Light mode");
	mode.AddString(L"Dark mode");
	mode.SetCurSel(m_Dark ? 1 : 0);

	m_List = GetDlgItem(IDC_COLOR_LIST);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
	m_List.InsertColumn(0, L"Element", LVCFMT_LEFT, 200);
	m_List.InsertColumn(1, L"Colour", LVCFMT_LEFT, 150);
	for (int i = 0; i < ElementCount; i++)
		m_List.AddItem(i, 0, ChartColors::Name(static_cast<ChartColor>(i)));
	RefreshList();
	m_List.SelectItem(0);
	GetDlgItem(IDC_COLOR_APPLY).EnableWindow(FALSE);
	return TRUE;
}

CString CChartColorsDlg::ColorText(int element) const {
	auto color = static_cast<ChartColor>(element);
	bool own = m_Colors.For(m_Dark)[element].has_value();
	auto rgb = m_Colors.Get(color, m_Dark);
	CString text;
	text.Format(L"#%02X%02X%02X%s", GetRValue(rgb), GetGValue(rgb), GetBValue(rgb), own ? L"" : L"  (default)");
	return text;
}

void CChartColorsDlg::RefreshList() {
	for (int i = 0; i < ElementCount; i++)
		m_List.SetItemText(i, 1, ColorText(i));
	m_List.Invalidate();
	UpdateButtons();
}

int CChartColorsDlg::Selected() const {
	return m_List.GetSelectedIndex();
}

void CChartColorsDlg::UpdateButtons() {
	int selected = Selected();
	GetDlgItem(IDC_COLOR_CHANGE).EnableWindow(selected >= 0);
	GetDlgItem(IDC_COLOR_RESET).EnableWindow(selected >= 0 && m_Colors.For(m_Dark)[selected].has_value());
	bool any = false;
	for (auto const& own : m_Colors.For(m_Dark))
		any |= own.has_value();
	GetDlgItem(IDC_COLOR_RESETALL).EnableWindow(any);
}

DWORD CChartColorsDlg::OnPrePaint(int, LPNMCUSTOMDRAW) noexcept {
	return CDRF_NOTIFYITEMDRAW;
}

DWORD CChartColorsDlg::OnItemPrePaint(int, LPNMCUSTOMDRAW) noexcept {
	return CDRF_NOTIFYSUBITEMDRAW;
}

DWORD CChartColorsDlg::OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) noexcept {
	auto lv = reinterpret_cast<LPNMLVCUSTOMDRAW>(cd);
	if (lv->iSubItem == 1 && cd->dwItemSpec < ElementCount) {
		// the colour column is painted in the colour itself
		auto rgb = m_Colors.Get(static_cast<ChartColor>(cd->dwItemSpec), m_Dark);
		lv->clrTextBk = rgb;
		lv->clrText = TextOn(rgb);
	}
	return CDRF_DODEFAULT;
}

void CChartColorsDlg::ChangeColor(int element) {
	auto color = static_cast<ChartColor>(element);
	CColorDialog dlg(m_Colors.Get(color, m_Dark), CC_FULLOPEN | CC_ANYCOLOR, m_hWnd);
	dlg.m_cc.lpCustColors = m_Custom;
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (!ok)
		return;
	m_Colors.For(m_Dark)[element] = dlg.GetColor();
	RefreshList();
	GetDlgItem(IDC_COLOR_APPLY).EnableWindow(TRUE);
}

LRESULT CChartColorsDlg::OnChange(WORD, WORD, HWND, BOOL&) {
	if (int selected = Selected(); selected >= 0)
		ChangeColor(selected);
	return 0;
}

LRESULT CChartColorsDlg::OnListDoubleClick(int, LPNMHDR, BOOL&) {
	if (int selected = Selected(); selected >= 0)
		ChangeColor(selected);
	return 0;
}

LRESULT CChartColorsDlg::OnSelectionChanged(int, LPNMHDR, BOOL&) {
	UpdateButtons();
	return 0;
}

LRESULT CChartColorsDlg::OnReset(WORD, WORD, HWND, BOOL&) {
	if (int selected = Selected(); selected >= 0) {
		m_Colors.For(m_Dark)[selected].reset();
		RefreshList();
		GetDlgItem(IDC_COLOR_APPLY).EnableWindow(TRUE);
	}
	return 0;
}

LRESULT CChartColorsDlg::OnResetAll(WORD, WORD, HWND, BOOL&) {
	m_Colors.For(m_Dark) = {};
	RefreshList();
	GetDlgItem(IDC_COLOR_APPLY).EnableWindow(TRUE);
	return 0;
}

LRESULT CChartColorsDlg::OnLoad(WORD, WORD, HWND, BOOL&) {
	CSimpleFileDialog dlg(TRUE, ChartColors::Extension, nullptr, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING, ChartColors::Filter, m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (!ok)
		return 0;

	ChartColors loaded = m_Colors;		// (the looks the file doesn't have stay as they are)
	std::wstring error;
	if (!loaded.Load(dlg.m_szFileName, error)) {
		CString message;
		message.Format(L"%s could not be loaded:\n\n%s", dlg.m_szFileName, error.c_str());
		AtlMessageBox(m_hWnd, (PCWSTR)message, L"Astro Studio", MB_ICONWARNING);
		return 0;
	}
	m_Colors = loaded;
	RefreshList();
	GetDlgItem(IDC_COLOR_APPLY).EnableWindow(TRUE);
	return 0;
}

LRESULT CChartColorsDlg::OnSave(WORD, WORD, HWND, BOOL&) {
	CSimpleFileDialog dlg(FALSE, ChartColors::Extension, L"Colors", OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING, ChartColors::Filter, m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (!ok)
		return 0;

	std::wstring error;
	if (!m_Colors.Save(dlg.m_szFileName, error)) {
		CString message;
		message.Format(L"The colours could not be saved to %s:\n\n%s", dlg.m_szFileName, error.c_str());
		AtlMessageBox(m_hWnd, (PCWSTR)message, L"Astro Studio", MB_ICONWARNING);
	}
	return 0;
}

LRESULT CChartColorsDlg::OnModeChanged(WORD, WORD, HWND, BOOL&) {
	CComboBox mode;
	mode = GetDlgItem(IDC_COLOR_MODE);
	m_Dark = mode.GetCurSel() == 1;
	RefreshList();
	return 0;
}

LRESULT CChartColorsDlg::OnApply(WORD, WORD, HWND, BOOL&) {
	if (m_OnApply)
		m_OnApply(m_Colors);
	m_Applied = true;
	GetDlgItem(IDC_COLOR_APPLY).EnableWindow(FALSE);
	return 0;
}

LRESULT CChartColorsDlg::OnOK(WORD, WORD, HWND, BOOL&) {
	EndDialog(IDOK);
	return 0;
}

LRESULT CChartColorsDlg::OnCancel(WORD, WORD, HWND, BOOL&) {
	// what Apply showed is taken back
	if (m_Applied && m_OnApply)
		m_OnApply(m_Original);
	EndDialog(IDCANCEL);
	return 0;
}
