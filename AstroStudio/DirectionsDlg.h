#pragma once

#include <DialogHelper.h>
#include "resource.h"
#include "DerivedCharts.h"
#include "TimeControls.h"

// Asks for what a directed or progressed chart needs: the date it is for (wall-clock time in a zone, like the New Chart dialog's)
// and one choice - the yearly arc the chart's points are moved by (Init), or for secondary progressions how the angles are found
// (InitProgressed). Without that (HideArc) it asks for a moment only (the date of an overlay).
class CDirectionsDlg :
	public CDialogImpl<CDirectionsDlg>,
	public CDialogHelper<CDirectionsDlg> {
public:
	enum { IDD = IDD_DIRECTIONS };

	// what the dialog opens with, and the window's title; the true solar arc is not offered when allowActual is off (primary directions)
	void Init(DateTime const& target, TimeZoneInfo const& zone, ArcKey key, PCWSTR caption, bool allowActual = true) {
		m_Target = target;
		m_Zone = zone;
		m_Key = key;
		m_Caption = caption;
		m_AllowActual = allowActual;
		m_Choice = Choice::Arc;
	}
	// for secondary progressions: the choice is how the angles are found
	void InitProgressed(DateTime const& target, TimeZoneInfo const& zone, ProgressedAngles angles, PCWSTR caption) {
		Init(target, zone, ArcKey::Actual, caption);
		m_Angles = angles;
		m_Choice = Choice::Angles;
	}
	// for a dialog that only asks for a moment: nothing else to choose
	void HideArc() {
		m_Choice = Choice::None;
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
	ProgressedAngles Angles() const {
		return m_Angles;
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
	enum class Choice { Arc, Angles, None };
	ArcKey m_Key{ ArcKey::Actual };
	ProgressedAngles m_Angles{ ProgressedAngles::Calculated };
	CString m_Caption;
	Choice m_Choice{ Choice::Arc };
	bool m_AllowActual{ true };
	CTimeControls m_Time;
	CComboBox m_Arc;
};
