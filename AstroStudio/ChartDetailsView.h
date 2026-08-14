#pragma once

#include "resource.h"
#include <VirtualListView.h>
#include "Interfaces.h"
#include <DialogHelper.h>

class ChartData;
struct PlanetPosition;

const UINT WM_HERE_RESULT = WM_APP + 100;

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

	// While pending, the Location field reads "<Locating...>" instead of the
	// (still empty) city/state/country.
	void SetLocationPending(bool pending);

	// True once the user has typed a location or used Here, so an arriving
	// geolocation result knows not to overwrite it.
	bool IsLocationEdited() const;

	void UpdateControls(Recalc type = Recalc::All);

	CString GetColumnText(HWND, int row, int col) const;
	void DoSort(SortInfo const* si);

	DWORD OnPrePaint(int, LPNMCUSTOMDRAW cd) noexcept ;
	DWORD OnItemPrePaint(int, LPNMCUSTOMDRAW cd) noexcept;
	DWORD OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) const noexcept;

	BEGIN_MSG_MAP(CChartDetailsView)
		COMMAND_HANDLER(IDC_HOUSESYSTEM, CBN_SELCHANGE, OnHouseSystemChanged)
		COMMAND_HANDLER(IDC_HARMONIC, EN_CHANGE, OnHarmonicChanged)
		NOTIFY_HANDLER(IDC_DATE, DTN_DATETIMECHANGE, OnDateChanged)
		NOTIFY_HANDLER(IDC_TIME, DTN_DATETIMECHANGE, OnTimeChanged)
		COMMAND_ID_HANDLER(IDC_NOW, OnNow)
		COMMAND_ID_HANDLER(IDC_HERE, OnHere)
		COMMAND_ID_HANDLER(IDC_APPLY, OnApply)
		COMMAND_HANDLER(IDC_LATDEG, EN_CHANGE, OnLocationChanged)
		COMMAND_HANDLER(IDC_LATMIN, EN_CHANGE, OnLocationChanged)
		COMMAND_HANDLER(IDC_LONDEG, EN_CHANGE, OnLocationChanged)
		COMMAND_HANDLER(IDC_LONMIN, EN_CHANGE, OnLocationChanged)
		COMMAND_HANDLER(IDC_NORTH, BN_CLICKED, OnLocationChanged)
		COMMAND_HANDLER(IDC_SOUTH, BN_CLICKED, OnLocationChanged)
		COMMAND_HANDLER(IDC_EAST, BN_CLICKED, OnLocationChanged)
		COMMAND_HANDLER(IDC_WEST, BN_CLICKED, OnLocationChanged)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitView)
		MESSAGE_HANDLER(WM_HERE_RESULT, OnHereResult)
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

	void UpdateLocationControls();
	void ApplyLocationFromControls();

	LRESULT OnInitView(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnHouseSystemChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDateChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnTimeChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnHarmonicChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnHere(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnHereResult(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnApply(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLocationChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CComboBox m_ctlHouseSystem;
	CDateTimePickerCtrl m_ctlDate, m_ctlTime;
	CListViewCtrl m_ctlPlanets, m_ctlHouses;
	CUpDownCtrl m_ctlHarmonicSpin;
	ChartData* m_Data{ nullptr };
	std::vector<PlanetPosition> m_Planets;
	CFont m_Font;
	CWindow m_NotifyWnd;
	bool m_UpdatingLocationControls{ false };
	bool m_LocationPending{ false };
	bool m_LocationEdited{ false };
};

