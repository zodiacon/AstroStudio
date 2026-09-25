#pragma once

#include <DialogHelper.h>
#include <functional>
#include "resource.h"
#include "ChartColors.h"

// The "Chart Colors" dialog: the colours of the chart wheel's elements, for the light and the dark look. Apply shows the
// colours on the charts without closing (Cancel takes them back).
class CChartColorsDlg :
	public CDialogImpl<CChartColorsDlg>,
	public CDialogHelper<CChartColorsDlg>,
	public CCustomDraw<CChartColorsDlg> {
public:
	enum { IDD = IDD_CHARTCOLORS };

	// the colours to start from, the look to show first, and what to do when Apply is pressed
	void Init(ChartColors const& colors, bool dark, std::function<void(ChartColors const&)> apply) {
		m_Colors = colors;
		m_Original = colors;
		m_Dark = dark;
		m_OnApply = std::move(apply);
	}
	// valid after DoModal returned IDOK
	ChartColors const& GetColors() const {
		return m_Colors;
	}

	DWORD OnPrePaint(int, LPNMCUSTOMDRAW) noexcept;
	DWORD OnItemPrePaint(int, LPNMCUSTOMDRAW) noexcept;
	DWORD OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) noexcept;

	BEGIN_MSG_MAP(CChartColorsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_COLOR_APPLY, OnApply)
		COMMAND_ID_HANDLER(IDC_COLOR_CHANGE, OnChange)
		COMMAND_ID_HANDLER(IDC_COLOR_RESET, OnReset)
		COMMAND_ID_HANDLER(IDC_COLOR_RESETALL, OnResetAll)
		COMMAND_ID_HANDLER(IDC_COLOR_LOAD, OnLoad)
		COMMAND_ID_HANDLER(IDC_COLOR_SAVE, OnSave)
		COMMAND_HANDLER(IDC_COLOR_MODE, CBN_SELCHANGE, OnModeChanged)
		NOTIFY_HANDLER(IDC_COLOR_LIST, NM_DBLCLK, OnListDoubleClick)
		NOTIFY_HANDLER(IDC_COLOR_LIST, LVN_ITEMCHANGED, OnSelectionChanged)
		CHAIN_MSG_MAP(CCustomDraw<CChartColorsDlg>)
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnApply(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnReset(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnResetAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLoad(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnModeChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnListDoubleClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnSelectionChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

	// what the Colour column of an element says
	CString ColorText(int element) const;
	void RefreshList();
	void UpdateButtons();
	int Selected() const;
	void ChangeColor(int element);

	ChartColors m_Colors, m_Original;
	bool m_Dark{ false };
	bool m_Applied{ false };
	std::function<void(ChartColors const&)> m_OnApply;
	CListViewCtrl m_List;
	COLORREF m_Custom[16]{};
};
