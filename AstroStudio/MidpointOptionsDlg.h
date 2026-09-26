#pragma once

#include <DialogHelper.h>
#include "resource.h"
#include "MidpointSettings.h"

// The "Midpoints" dialog: which points take part in midpoints, and the orb and kind of contact of the list and of the tree.
class CMidpointOptionsDlg :
	public CDialogImpl<CMidpointOptionsDlg>,
	public CDialogHelper<CMidpointOptionsDlg> {
public:
	enum { IDD = IDD_MIDPOINTOPTIONS };

	void SetOptions(MidpointSettings const& options) {
		m_Options = options;
	}
	// valid after DoModal returned IDOK
	MidpointSettings const& GetOptions() const {
		return m_Options;
	}

	BEGIN_MSG_MAP(CMidpointOptionsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_MP_ALL, OnCheckAll)
		COMMAND_ID_HANDLER(IDC_MP_NONE, OnCheckAll)
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCheckAll(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	void FillOrbs(CComboBox& combo, int orb);
	void FillKinds(CComboBox& combo, ContactKind kind, bool with45);

	// the list's items for the angles (the bodies' are their Planet numbers)
	static constexpr int AscendantItem = 1000;
	static constexpr int MidheavenItem = 1001;

	MidpointSettings m_Options;
	CListViewCtrl m_Points;
};
