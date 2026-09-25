#pragma once

#include <DialogHelper.h>
#include "resource.h"
#include "DerivedCharts.h"
#include "TimeControls.h"

// Asks for what a directed chart needs: the date it is for (wall-clock time in a zone, like the New Chart dialog's) and the
// yearly arc the chart's points are moved by. Without the arc (HideArc) it asks for a moment only (the date of an overlay).
class CDirectionsDlg :
	public CDialogImpl<CDirectionsDlg>,
	public CDialogHelper<CDirectionsDlg> {
public:
	enum { IDD = IDD_DIRECTIONS };

	// what the dialog opens with, and the window's title
	void Init(DateTime const& target, TimeZoneInfo const& zone, ArcKey key, PCWSTR caption) {
		m_Target = target;
		m_Zone = zone;
		m_Key = key;
		m_Caption = caption;
	}
	// for a dialog that only asks for a moment: no arc to choose
	void HideArc() {
		m_ShowArc = false;
	}
	// valid after DoModal returned IDOK
	DateTime const& Target() const {
		return m_Target;
	}
	TimeZoneInfo const& Zone() const {
		return m_Zone;
	}
	ArcKey Key() const {
		return m_Key;
	}

	BEGIN_MSG_MAP(CDirectionsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_NOW, OnNow)
		COMMAND_HANDLER(IDC_MONTH, CBN_SELCHANGE, OnMonthOrYearChanged)
		COMMAND_HANDLER(IDC_YEAR, EN_KILLFOCUS, OnMonthOrYearChanged)
		COMMAND_HANDLER(IDC_MANUALTZ, BN_CLICKED, OnManualToggled)
		COMMAND_HANDLER(IDC_TIME, EN_KILLFOCUS, OnTimeKillFocus)
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnMonthOrYearChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnManualToggled(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTimeKillFocus(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	DateTime m_Target;
	TimeZoneInfo m_Zone;
	ArcKey m_Key{ ArcKey::Actual };
	CString m_Caption;
	bool m_ShowArc{ true };
	CTimeControls m_Time;
	CComboBox m_Arc;
};
