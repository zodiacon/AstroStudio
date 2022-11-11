#pragma once

#include "resource.h"

class ChartData;

class CChartDetailsView : public CDialogImpl<CChartDetailsView> {
public:
	enum { IDD = IDD_CHARTDETAILS };

	BOOL PreTranslateMessage(MSG* pMsg);

	void SetChartData(ChartData* data);
	void SetNotifyWindow(HWND hWnd);

	BEGIN_MSG_MAP(CChartDetailsView)
		COMMAND_HANDLER(IDC_HOUSESYSTEM, CBN_SELCHANGE, OnHouseSystemChanged)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitView)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	void UpdateControls();

	LRESULT OnInitView(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnHouseSystemChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CComboBox m_ctlHouseSystem;
	ChartData* m_Data{ nullptr };
	CWindow m_NotifyWnd;
};

