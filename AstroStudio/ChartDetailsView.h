#pragma once

#include "resource.h"
#include <VirtualListView.h>
#include "Interfaces.h"
#include <DialogHelper.h>

class ChartData;
struct PlanetPosition;

class CChartDetailsView : 
	public CDialogImpl<CChartDetailsView>,
	public CVirtualListView<CChartDetailsView>,
	public CDialogHelper<CChartDetailsView>,
	public CCustomDraw<CChartDetailsView> {
public:
	enum { IDD = IDD_CHARTDETAILS };

	BOOL PreTranslateMessage(MSG* pMsg);

	void SetChartData(ChartData* data);
	void SetNotifyWindow(HWND hWnd);

	CString GetColumnText(HWND, int row, int col) const;
	void DoSort(SortInfo const* si);

	DWORD OnPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnItemPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd);

	BEGIN_MSG_MAP(CChartDetailsView)
		COMMAND_HANDLER(IDC_HOUSESYSTEM, CBN_SELCHANGE, OnHouseSystemChanged)
		COMMAND_HANDLER(IDC_HARMONIC, EN_CHANGE, OnHarmonicChanged)
		NOTIFY_HANDLER(IDC_DATE, DTN_DATETIMECHANGE, OnDateChanged)
		NOTIFY_HANDLER(IDC_TIME, DTN_DATETIMECHANGE, OnTimeChanged)
		COMMAND_ID_HANDLER(IDC_NOW, OnNow)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitView)
		CHAIN_MSG_MAP(CCustomDraw)
		CHAIN_MSG_MAP(CVirtualListView)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	enum class ColumnType {
		Planet, Longitude, Latitude, Speed, House, HouseLongitude,
	};

	void UpdateControls(Recalc type = Recalc::All);

	LRESULT OnInitView(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnHouseSystemChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDateChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnTimeChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnHarmonicChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CComboBox m_ctlHouseSystem;
	CDateTimePickerCtrl m_ctlDate, m_ctlTime;
	CListViewCtrl m_ctlPlanets, m_ctlHouses;
	CUpDownCtrl m_ctlHarmonicSpin;
	ChartData* m_Data{ nullptr };
	std::vector<PlanetPosition> m_Planets;
	CFont m_Font;
	CWindow m_NotifyWnd;
};

