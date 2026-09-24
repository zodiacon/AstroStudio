#pragma once

#include <DialogHelper.h>
#include "resource.h"
#include "ChartData.h"
#include "LocationControls.h"
#include "TimeControls.h"

// Collects the details of a new chart: who/what, where, when (as wall-clock time in a time zone or with a
// manual UT offset) and the house system.
class CNewChartDlg :
	public CDialogImpl<CNewChartDlg>,
	public CDialogHelper<CNewChartDlg> {
public:
	enum { IDD = IDD_NEWCHART };

	// the values the dialog opens with
	void SetChartInfo(ChartInfo const& info);
	void SetHouseSystem(HouseSystem system);

	// valid after DoModal returned IDOK
	ChartInfo const& GetChartInfo() const;
	HouseSystem GetHouseSystem() const;
	// "Last, First", or whatever was typed; empty if no name was given
	CString GetTitle() const;

	BEGIN_MSG_MAP(CNewChartDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_HERE_RESULT, OnHereResult)
		MESSAGE_HANDLER(WM_LOOKUP_RESULT, OnLookupResult)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_NOW, OnNow)
		COMMAND_ID_HANDLER(IDC_HERE, OnHere)
		COMMAND_ID_HANDLER(IDC_LOOKUP, OnLookup)
		COMMAND_HANDLER(IDC_MONTH, CBN_SELCHANGE, OnMonthOrYearChanged)
		COMMAND_HANDLER(IDC_YEAR, EN_KILLFOCUS, OnMonthOrYearChanged)
		COMMAND_HANDLER(IDC_MANUALTZ, BN_CLICKED, OnManualToggled)
		COMMAND_HANDLER(IDC_TIME, EN_KILLFOCUS, OnTimeKillFocus)
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnHereResult(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnLookupResult(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnLookup(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnHere(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnMonthOrYearChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTimeKillFocus(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnManualToggled(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	bool Fail(UINT control, PCWSTR message);

	ChartInfo m_Info{};
	HouseSystem m_HouseSystem{ HouseSystem::Koch };
	CString m_OriginalLocation;		// to tell whether the location text was edited
	CLocationControls m_Location;
	CTimeControls m_Time;
	CComboBox m_ctlHouseSystem, m_ctlType;
};
