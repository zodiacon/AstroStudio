#include "pch.h"
#include "DirectionsDlg.h"

LRESULT CDirectionsDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	CenterWindow(GetParent());
	SetWindowText(m_Caption);

	m_Time.Init(m_hWnd);
	m_Time.Set(m_Target, m_Zone);

	m_Arc.Attach(GetDlgItem(IDC_DIR_ARC));
	if (m_Choice == Choice::Angles) {
		SetDlgItemText(IDC_DIR_ARC_LABEL, L"Angles:");
		struct { ProgressedAngles Angles; PCWSTR Text; } modes[] = {
			{ ProgressedAngles::Calculated, L"Cast for the progressed moment at the birth place" },
			{ ProgressedAngles::SolarArc, L"The birth Midheaven moved on by the solar arc" },
			{ ProgressedAngles::Natal, L"The birth houses, unchanged" },
		};
		for (auto const& mode : modes) {
			int item = m_Arc.AddString(mode.Text);
			m_Arc.SetItemData(item, static_cast<DWORD_PTR>(mode.Angles));
			if (mode.Angles == m_Angles)
				m_Arc.SetCurSel(item);
		}
	}
	else {
		struct { ArcKey Key; PCWSTR Text; } keys[] = {
			{ ArcKey::Actual, L"Solar arc: the distance the progressed Sun has moved" },
			{ ArcKey::Naibod, L"Naibod: 0.9856 degrees a year" },
			{ ArcKey::Ptolemy, L"Ptolemy: 1 degree a year" },
		};
		for (auto const& key : keys) {
			if (key.Key == ArcKey::Actual && !m_AllowActual)
				continue;
			int item = m_Arc.AddString(key.Text);
			m_Arc.SetItemData(item, static_cast<DWORD_PTR>(key.Key));
			if (key.Key == m_Key)
				m_Arc.SetCurSel(item);
		}
	}
	if (m_Arc.GetCurSel() < 0 && m_Arc.GetCount() > 0)
		m_Arc.SetCurSel(0);
	if (m_Choice == Choice::None) {
		// the arc goes and the buttons move up into its place
		CRect arc, ok;
		m_Arc.GetWindowRect(&arc);
		GetDlgItem(IDOK).GetWindowRect(&ok);
		int shift = ok.top - arc.top;
		m_Arc.ShowWindow(SW_HIDE);
		GetDlgItem(IDC_DIR_ARC_LABEL).ShowWindow(SW_HIDE);
		for (int id : { IDOK, IDCANCEL }) {
			CRect rc;
			GetDlgItem(id).GetWindowRect(&rc);
			ScreenToClient(&rc);
			GetDlgItem(id).SetWindowPos(nullptr, rc.left, rc.top - shift, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		}
		CRect window;
		GetWindowRect(&window);
		SetWindowPos(nullptr, 0, 0, window.Width(), window.Height() - shift, SWP_NOMOVE | SWP_NOZORDER);
		CenterWindow(GetParent());
	}
	return TRUE;
}

LRESULT CDirectionsDlg::OnOK(WORD, WORD, HWND, BOOL&) {
	DateTime ut;
	TimeZoneInfo zone;
	if (auto error = m_Time.Get(ut, zone); error != CTimeControls::Error::None) {
		AtlMessageBox(m_hWnd, CTimeControls::Message(error), (PCWSTR)m_Caption, MB_ICONWARNING);
		GetDlgItem(CTimeControls::ControlFor(error)).SetFocus();
		return 0;
	}
	m_Target = ut;
	m_Zone = zone;
	if (m_Choice == Choice::Arc)
		m_Key = static_cast<ArcKey>(m_Arc.GetItemData(m_Arc.GetCurSel()));
	else if (m_Choice == Choice::Angles)
		m_Angles = static_cast<ProgressedAngles>(m_Arc.GetItemData(m_Arc.GetCurSel()));
	EndDialog(IDOK);
	return 0;
}

LRESULT CDirectionsDlg::OnCancel(WORD, WORD, HWND, BOOL&) {
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CDirectionsDlg::OnNow(WORD, WORD, HWND, BOOL&) {
	// the current moment, shown in whichever zone is selected
	DateTime ut;
	TimeZoneInfo zone;
	if (m_Time.Get(ut, zone) != CTimeControls::Error::None)
		zone = TimeZones::Machine();
	m_Time.Set(DateTime::Now(), zone);
	return 0;
}

LRESULT CDirectionsDlg::OnMonthOrYearChanged(WORD, WORD, HWND, BOOL&) {
	m_Time.UpdateDays();
	return 0;
}

LRESULT CDirectionsDlg::OnManualToggled(WORD, WORD, HWND, BOOL&) {
	m_Time.ManualToggled();
	return 0;
}

LRESULT CDirectionsDlg::OnTimeKillFocus(WORD, WORD, HWND, BOOL&) {
	m_Time.NormalizeTime();
	return 0;
}
