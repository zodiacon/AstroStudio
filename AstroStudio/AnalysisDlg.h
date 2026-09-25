#pragma once

#include <DialogHelper.h>
#include "resource.h"
#include "Interfaces.h"
#include "Analysis.h"
#include "DateBoxes.h"

// The "Analysis" dialog: which chart, what is compared with what, the range of dates, the planets and aspects, and the kinds of
// event to list.
class CAnalysisDlg :
	public CDialogImpl<CAnalysisDlg>,
	public CDialogHelper<CAnalysisDlg> {
public:
	enum { IDD = IDD_ANALYSIS };

	// The charts to choose from (which must outlive the dialog), the one it opens on, and the settings it opens with.
	void Init(std::vector<OpenChart> const* charts, int chart, AnalysisSettings const& settings) {
		m_Charts = charts;
		m_Chart = chart;
		m_Settings = settings;
	}
	// valid after DoModal returned IDOK
	int Chart() const {
		return m_Chart;
	}
	AnalysisSettings const& Settings() const {
		return m_Settings;
	}

	// what an analysis starts out as: transits to the natal chart for the next twelve months, with the usual planets and the
	// aspects and orbs of Options > Aspects (transit / overlay)
	static AnalysisSettings Defaults();

	BEGIN_MSG_MAP(CAnalysisDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		NOTIFY_HANDLER(IDC_AN_TYPE, LVN_ITEMCHANGED, OnTypesChanged)
		COMMAND_HANDLER(IDC_AN_CHART, CBN_SELCHANGE, OnChartChanged)
		COMMAND_HANDLER(IDC_AN_PRESET, CBN_SELCHANGE, OnPreset)
		COMMAND_HANDLER(IDC_AN_FROM_MONTH, CBN_SELCHANGE, OnFromChanged)
		COMMAND_HANDLER(IDC_AN_FROM_YEAR, EN_KILLFOCUS, OnFromChanged)
		COMMAND_HANDLER(IDC_AN_TO_MONTH, CBN_SELCHANGE, OnToChanged)
		COMMAND_HANDLER(IDC_AN_TO_YEAR, EN_KILLFOCUS, OnToChanged)
		COMMAND_HANDLER(IDC_AN_FROM_DAY, CBN_SELCHANGE, OnDatesEdited)
		COMMAND_HANDLER(IDC_AN_TO_DAY, CBN_SELCHANGE, OnDatesEdited)
		COMMAND_ID_HANDLER(IDC_AN_MOVERS_ALL, OnCheckAll)
		COMMAND_ID_HANDLER(IDC_AN_MOVERS_STD, OnCheckAll)
		COMMAND_ID_HANDLER(IDC_AN_TARGETS_ALL, OnCheckAll)
		COMMAND_ID_HANDLER(IDC_AN_TARGETS_STD, OnCheckAll)
		COMMAND_ID_HANDLER(IDC_AN_ASPECTS_ALL, OnCheckAll)
		COMMAND_ID_HANDLER(IDC_AN_ASPECTS_MAJOR, OnCheckAll)
		COMMAND_ID_HANDLER(IDC_AN_ASPECTS, OnEventKindChanged)
		NOTIFY_HANDLER(IDC_AN_MOVERS, LVN_ITEMCHANGED, OnMoversChanged)
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTypesChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnChartChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPreset(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFromChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDatesEdited(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCheckAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEventKindChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnMoversChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

	// the analyses that are ticked, in the order they are listed
	std::vector<AnalysisType> SelectedTypes() const;
	// the dates were changed by hand: no preset any more
	void DatesEdited();
	// what depends on the type: which options apply, and what the targets are
	void UpdateType();
	// the note under the dates: the Moon being left out, and what the range comes to
	void UpdateNote();
	// reads the dates; false (after saying what is wrong) if they are not usable
	bool ReadDates(DateTime& from, DateTime& to);
	bool Fail(UINT control, PCWSTR message);
	// the checked planets of a list
	static std::vector<Planet> Checked(CListViewCtrl& list);
	// a rough guess at how many seconds the analysis will take, for warning about the long ones
	static double EstimateSeconds(AnalysisSettings const& settings);

	std::vector<OpenChart> const* m_Charts{ nullptr };
	int m_Chart{ 0 };
	AnalysisSettings m_Settings;
	CComboBox m_ChartBox, m_PresetBox;
	CListViewCtrl m_Types, m_Movers, m_Targets, m_Aspects;
	CDateBoxes m_From, m_To;
	bool m_Loading{ true };
};
