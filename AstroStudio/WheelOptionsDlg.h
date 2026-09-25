#pragma once

#include <DialogHelper.h>
#include "resource.h"
#include "WheelOptions.h"

// The "Chart Wheel" dialog: which planets and which aspect lines the chart wheel draws.
class CWheelOptionsDlg :
	public CDialogImpl<CWheelOptionsDlg>,
	public CDialogHelper<CWheelOptionsDlg> {
public:
	enum { IDD = IDD_WHEELOPTIONS };

	void SetOptions(WheelOptions const& options) {
		m_Options = options;
	}
	// valid after DoModal returned IDOK
	WheelOptions const& GetOptions() const {
		return m_Options;
	}

	BEGIN_MSG_MAP(CWheelOptionsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_WHEEL_PLANETS_ALL, OnCheckAll)
		COMMAND_ID_HANDLER(IDC_WHEEL_PLANETS_NONE, OnCheckAll)
		COMMAND_ID_HANDLER(IDC_WHEEL_ASPECTS_ALL, OnCheckAll)
		COMMAND_ID_HANDLER(IDC_WHEEL_ASPECTS_NONE, OnCheckAll)
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// the All and None buttons of the two lists
	LRESULT OnCheckAll(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	void FillList(CListViewCtrl& list);

	WheelOptions m_Options;
	CListViewCtrl m_Planets, m_Aspects;
};
