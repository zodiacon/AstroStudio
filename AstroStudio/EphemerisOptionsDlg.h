#pragma once

#include <DialogHelper.h>
#include "resource.h"
#include "AstroCalculator.h"
#include "DateTime.h"

// What the ephemeris view shows: from which date, in what steps, of which bodies, and which extra columns.
struct EphemerisSettings {
	DateTime Start;					// UT midnight of the first row
	double Step{ 1 };				// days between rows
	std::vector<Planet> Planets;	// the columns, in order
	bool Eclipses{ false };			// a column with the eclipses of each row's stretch of time
	bool VoidOfCourse{ false };		// and one with the Moon's void of course periods

	// the extra columns only make sense while a row is a short stretch of time
	static constexpr double MaxEclipseStep = 31, MaxVoidStep = 7;
	static constexpr double MaxStep = 366;
};

// The "Ephemeris Options" dialog: start date, step, bodies and the extra columns.
class CEphemerisOptionsDlg :
	public CDialogImpl<CEphemerisOptionsDlg>,
	public CDialogHelper<CEphemerisOptionsDlg> {
public:
	enum { IDD = IDD_EPHEMERISOPTIONS };

	void SetSettings(EphemerisSettings const& settings);
	// valid after DoModal returned IDOK
	EphemerisSettings const& GetSettings() const;

	BEGIN_MSG_MAP(CEphemerisOptionsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_EPH_TODAY, OnToday)
		COMMAND_ID_HANDLER(IDC_EPH_STANDARD, OnStandard)
		COMMAND_ID_HANDLER(IDC_EPH_ALL, OnAll)
		COMMAND_HANDLER(IDC_EPH_STEP, EN_CHANGE, OnStepChanged)
		COMMAND_HANDLER(IDC_MONTH, CBN_SELCHANGE, OnMonthOrYearChanged)
		COMMAND_HANDLER(IDC_YEAR, EN_KILLFOCUS, OnMonthOrYearChanged)
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToday(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnStandard(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnStepChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnMonthOrYearChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	bool Fail(UINT control, PCWSTR message);
	// the step typed in, or 0 if it is not a whole number of days in range
	double TypedStep() const;
	// the extra columns are only on offer for short steps
	void UpdateExtras();
	void CheckBodies(bool all);
	// shows a date in the day, month and year boxes
	void ShowDate(DateTime const& date);
	// The year typed, if it is one the ephemeris covers.
	bool GetYear(long& year) const;
	// rebuilds the list of days to the number the month and year have
	void UpdateDays();

	EphemerisSettings m_Settings;
	CListViewCtrl m_Bodies;
	CComboBox m_Day, m_Month;
};
