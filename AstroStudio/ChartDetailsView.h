#pragma once

#include "resource.h"
#include <VirtualListView.h>

class ChartData;

class CChartDetailsView : 
	public CDialogImpl<CChartDetailsView>,
	public CVirtualListView<CChartDetailsView>,
	public CCustomDraw<CChartDetailsView> {
public:
	enum { IDD = IDD_CHARTDETAILS };

	BOOL PreTranslateMessage(MSG* pMsg);

	void SetChartData(ChartData* data);
	void SetNotifyWindow(HWND hWnd);

	CString GetColumnText(HWND, int row, int col) const;

	DWORD OnPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnItemPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd);

	BEGIN_MSG_MAP(CChartDetailsView)
		COMMAND_HANDLER(IDC_HOUSESYSTEM, CBN_SELCHANGE, OnHouseSystemChanged)
		NOTIFY_HANDLER(IDC_DATE, DTN_DATETIMECHANGE, OnDateChanged)
		NOTIFY_HANDLER(IDC_TIME, DTN_DATETIMECHANGE, OnTimeChanged)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitView)
		CHAIN_MSG_MAP(CVirtualListView<CChartDetailsView>)
		CHAIN_MSG_MAP(CCustomDraw<CChartDetailsView>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	enum class ColumnType {
		Planet, Longitude, Latitude, Speed, House,
	};

	void UpdateControls();

	LRESULT OnInitView(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnHouseSystemChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDateChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnTimeChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

	CComboBox m_ctlHouseSystem;
	CDateTimePickerCtrl m_ctlDate, m_ctlTime;
	CListViewCtrl m_ctlPlanets;
	ChartData* m_Data{ nullptr };
	CFont m_Font;
	CWindow m_NotifyWnd;
};

