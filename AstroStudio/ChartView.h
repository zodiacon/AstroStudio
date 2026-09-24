#pragma once

#include "ChartData.h"
#include <FrameView.h>
#include "Interfaces.h"
#include <VirtualListView.h>
#include "ChartDetailsView.h"
#include "GraphicChartView.h"
#include "AspectGridWnd.h"
#include "AspectListView.h"
#include "TimeStep.h"
#include "ChartFile.h"
#include <CustomSplitterWindow.h>
#include <NativeCustomTabView.h>
#include <atlscrl.h>
#include <WTLHelper.h>

class CChartView :
	public CFrameView<CChartView, IMainFrame>,
	public IView,
	public CVirtualListView<CChartView> {
public:
	CChartView(IMainFrame* frame);
	void Chart(ChartData data);
	void ChartForNow();
	ChartData const& Chart() const;

	BOOL PreTranslateMessage(MSG* pMsg);
	void PageActivated(bool active) override;
	// Sets the tab text (title, until the chart is saved under a name) and the file the chart lives in, if any.
	void SetFile(PCWSTR title, PCWSTR filePath);
	PCWSTR FilePath() const override;
	// asks whether to save unsaved changes
	bool CanClose() override;

	BEGIN_MSG_MAP(CChartView)
		MESSAGE_HANDLER(WM_RECALC, OnRecalc)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_FORWARDMSG, OnForwardMsg)
		MESSAGE_HANDLER(WTLHelper::ThemeChangedMessage, OnThemeChanged)
		MESSAGE_HANDLER(WM_LOCATION_UPDATED, OnLocationUpdated)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		COMMAND_ID_HANDLER(ID_CHART_STEP_BACK, OnStep)
		COMMAND_ID_HANDLER(ID_CHART_STEP_FORWARD, OnStep)
		COMMAND_ID_HANDLER(ID_CHART_AUTOSTEP, OnAutoStep)
		COMMAND_ID_HANDLER(ID_CHART_LIVE, OnLive)
		COMMAND_ID_HANDLER(ID_CHART_TRANSITS, OnTransits)
		COMMAND_HANDLER(IDC_STEPINTERVAL, CBN_SELCHANGE, OnIntervalChanged)
		CHAIN_MSG_MAP(CVirtualListView)
		CHAIN_MSG_MAP(BaseFrame)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_CHART_STEP_BACK, OnStep)
		COMMAND_ID_HANDLER(ID_CHART_STEP_FORWARD, OnStep)
		COMMAND_ID_HANDLER(ID_CHART_AUTOSTEP, OnAutoStep)
		COMMAND_ID_HANDLER(ID_CHART_LIVE, OnLive)
		COMMAND_ID_HANDLER(ID_CHART_TRANSITS, OnTransits)
		COMMAND_ID_HANDLER(ID_FILE_SAVE, OnSave)
		COMMAND_ID_HANDLER(ID_FILE_SAVE_AS, OnSave)
		COMMAND_ID_HANDLER(ID_FILE_EXPORT, OnExport)
	END_MSG_MAP()

private:
	// Writes the chart to its file, asking for a name first if it has none (or saveAs). False if it wasn't saved.
	bool Save(bool saveAs);
	void SetModified(bool modified);
	// The tab text: the title, marked with * when the chart came from a file (or was saved to one) and has changed since.
	void UpdateTitle();
	LRESULT OnSave(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// File > Export: the chart wheel as a PNG picture
	LRESULT OnExport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	void CreateStepToolBar();
	// Moves the chart's time by the toolbar's step count and unit; direction is 1 (forward) or -1 (back).
	// Returns false if it couldn't (no chart yet, or the result is outside the years the ephemeris covers).
	bool StepTime(int direction);
	// Auto step runs its timer only while it is switched on and this chart is the page showing.
	void UpdateAutoStepTimer();
	void SetAutoStep(bool on);
	// Live keeps the chart at the current time (the timer, at the interval of the drop-down, sets it to now). It shares
	// the timer with auto step and excludes it; the user changing anything in the details view ends it.
	void SetLive(bool on);
	void TickLive();
	// what the toolbar and menu show for the stepping commands: none of them applies while the time moves by itself
	void UpdateStepUI();
	LRESULT OnLive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	// Transits: the wheel shows the planets of a second moment (m_TransitData, at first the current time) around the
	// chart, with their aspects to it. The chart keeps its own time; what moves the time - Step, Auto, Live - moves the
	// transits' while they are shown.
	void SetTransits(bool on);
	void UpdateTransits();
	LRESULT OnTransits(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnStep(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAutoStep(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnIntervalChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	void DisplayPlanets(CDCHandle dc, int x, int y) const;
	void DisplayHouses(CDCHandle dc, int x, int y) const;

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRecalc(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnForwardMsg(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnLocationUpdated(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	void UpdateAspectGridScrollSize();
	void UpdateAspectGridScrollBarTheme();

	ChartData m_Data;
	AstroCalculator m_Calc;
	// Set when this chart was opened before the geolocation lookup finished, so
	// its location is a placeholder awaiting WM_LOCATION_UPDATED.
	bool m_AwaitingLocation{ false };
	CCustomSplitterWindow m_Splitter;
	CNativeCustomTabView m_DetailsTabs;
	CChartDetailsView m_DetailsView;
	CScrollContainer m_AspectGridScroll;
	CAspectGridWnd m_AspectGrid;
	CAspectListView m_AspectList;
	CGraphicChartView m_ChartDrawing;
	// the "Step: [count] [unit]" controls, which sit inside the toolbar
	CStatic m_StepLabel;
	CComboBox m_StepCount, m_StepUnit, m_StepInterval;
	bool m_AutoStep{ false };
	bool m_Live{ false };
	bool m_Transits{ false };
	ChartData m_TransitData;
	CString m_FilePath, m_Title;
	bool m_Modified{ false };
	int m_NotModifying{ 0 };		// while above 0, changes (system updates, auto step ticks) don't count as edits
	bool m_PageActive{ false };
};
