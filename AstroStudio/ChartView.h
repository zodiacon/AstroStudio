#pragma once

#include "ChartData.h"
#include "ChartOverlay.h"
#include <optional>
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
	// A chart worked out from others (see IMainFrame::AddDerivedChartView): shown as it is, never recalculated, read-only.
	// With the recipe it was made from (a chart that is made anew from it is saved as it).
	void DerivedChart(ChartData data, DerivedRecipe const* recipe = nullptr);
	void ChartForNow();
	ChartData const& Chart() const;

	BOOL PreTranslateMessage(MSG* pMsg);
	void PageActivated(bool active) override;
	// Sets the tab text (title, until the chart is saved under a name) and the file the chart lives in, if any.
	void SetFile(PCWSTR title, PCWSTR filePath);
	PCWSTR FilePath() const override;
	// asks whether to save unsaved changes
	bool CanClose() override;
	void AspectSettingsChanged() override;
	void WheelOptionsChanged() override;
	void TextFontChanged() override;
	bool GetChart(OpenChart& chart) const override;
	bool ShowMoment(DateTime const& ut, MomentKind kind) override;

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
		COMMAND_ID_HANDLER(ID_CHART_TRANSITS, OnOverlay)
		COMMAND_ID_HANDLER(ID_CHART_OVERLAY_NONE, OnOverlay)
		COMMAND_ID_HANDLER(ID_CHART_OVERLAY_PROGRESSED, OnOverlay)
		COMMAND_ID_HANDLER(ID_CHART_OVERLAY_SOLARARC, OnOverlay)
		COMMAND_ID_HANDLER(ID_CHART_OVERLAY_SYNASTRY, OnOverlay)
		NOTIFY_CODE_HANDLER(TBN_DROPDOWN, OnOverlayDropDown)
		COMMAND_ID_HANDLER(ID_CHART_DERIVED_SOLARRETURN, OnDerived)
		COMMAND_ID_HANDLER(ID_CHART_DERIVED_LUNARRETURN, OnDerived)
		COMMAND_ID_HANDLER(ID_CHART_ANALYSIS, OnAnalysis)
		COMMAND_ID_HANDLER(ID_CHART_DERIVED_SOLARARC, OnDerived)
		COMMAND_ID_HANDLER(ID_CHART_DERIVED_COMPOSITE, OnDerived)
		COMMAND_ID_HANDLER(ID_CHART_DERIVED_DAVISON, OnDerived)
		COMMAND_HANDLER(IDC_STEPCOUNT, CBN_SELCHANGE, OnStepSettingChanged)
		COMMAND_HANDLER(IDC_STEPUNIT, CBN_SELCHANGE, OnStepSettingChanged)
		COMMAND_HANDLER(IDC_STEPINTERVAL, CBN_SELCHANGE, OnIntervalChanged)
		CHAIN_MSG_MAP(CVirtualListView)
		CHAIN_MSG_MAP(BaseFrame)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_CHART_STEP_BACK, OnStep)
		COMMAND_ID_HANDLER(ID_CHART_STEP_FORWARD, OnStep)
		COMMAND_ID_HANDLER(ID_CHART_AUTOSTEP, OnAutoStep)
		COMMAND_ID_HANDLER(ID_CHART_LIVE, OnLive)
		COMMAND_ID_HANDLER(ID_CHART_TRANSITS, OnOverlay)
		COMMAND_ID_HANDLER(ID_CHART_OVERLAY_NONE, OnOverlay)
		COMMAND_ID_HANDLER(ID_CHART_OVERLAY_PROGRESSED, OnOverlay)
		COMMAND_ID_HANDLER(ID_CHART_OVERLAY_SOLARARC, OnOverlay)
		COMMAND_ID_HANDLER(ID_CHART_OVERLAY_SYNASTRY, OnOverlay)
		COMMAND_ID_HANDLER(ID_CHART_DERIVED_SOLARRETURN, OnDerived)
		COMMAND_ID_HANDLER(ID_CHART_DERIVED_LUNARRETURN, OnDerived)
		COMMAND_ID_HANDLER(ID_CHART_ANALYSIS, OnAnalysis)
		COMMAND_ID_HANDLER(ID_CHART_DERIVED_SOLARARC, OnDerived)
		COMMAND_ID_HANDLER(ID_CHART_DERIVED_COMPOSITE, OnDerived)
		COMMAND_ID_HANDLER(ID_CHART_DERIVED_DAVISON, OnDerived)
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

	// Overlay: the wheel shows a second set of planets around the chart, with their aspects to it: the planets of a second
	// moment (transits), the chart progressed to a moment, or another open chart (synastry). A moment starts as the current
	// time. The chart keeps its own time; what moves the time - Step, Auto, Live - moves the overlay's while it is shown, unless
	// it is a synastry overlay, which has no time of its own.
	// Shows an overlay of a kind (a progressed one by the method); false if there was nothing to show (no other chart).
	bool ShowOverlay(OverlayKind kind, ProgressionMethod method = ProgressionMethod::Secondary);
	void HideOverlay();
	// works the overlay's planets (unless it is a synastry overlay's), aspects and caption out again and shows it
	void UpdateOverlay();
	// the overlay's time is what Step, Auto and Live move (rather than the chart's)
	bool OverlayHasTime() const {
		return m_Overlay && m_Overlay->FollowsTime();
	}
	// What Step, Auto and Live move: the chart's time - which a read-only chart doesn't have to change - or an overlay's.
	bool CanMoveTime() const {
		return !m_ReadOnly || OverlayHasTime();
	}
	// asks which of the other open charts to use (a popup at the cursor if several); false if there is none or the user cancelled
	bool PickOtherChart(OpenChart& chart, PCWSTR what);
	// New Derived Chart: the chart of the return of the Sun or the Moon to its natal place, opened by the New Chart dialog
	void NewReturnChart(Planet planet);
	// ...a chart with every point moved by the solar arc to a date asked for...
	void NewSolarArcChart();
	// ...and a composite or Davison chart of this and another open chart
	void NewPairChart(bool davison);
	LRESULT OnDerived(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// Chart > Analysis: the analysis dialog, on this chart
	LRESULT OnAnalysis(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// the menu's and the toolbar's marks for the overlay shown
	void UpdateOverlayUI();
	void StopTimeIfFixed();
	LRESULT OnOverlay(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// the toolbar's Overlay button: a menu of the same choices as the Chart menu's, under the button
	LRESULT OnOverlayDropDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
	LRESULT OnStep(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAutoStep(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnIntervalChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// keeps what the step count, unit and interval boxes show for the next chart and the next run
	LRESULT OnStepSettingChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	void SaveStepSettings();
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	void DisplayPlanets(CDCHandle dc, int x, int y) const;
	void DisplayHouses(CDCHandle dc, int x, int y) const;

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRecalc(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnForwardMsg(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnLocationUpdated(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	// works out the chart's aspects by the current aspect settings and gives them to everything that shows them
	void UpdateAspects();
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
	bool m_ReadOnly{ false };
	std::optional<DerivedRecipe> m_Recipe;		// what a read-only chart was made from, if that is known
	std::optional<ChartOverlay> m_Overlay;
	CString m_FilePath, m_Title;
	bool m_Modified{ false };
	int m_NotModifying{ 0 };		// while above 0, changes (system updates, auto step ticks) don't count as edits
	bool m_PageActive{ false };
};
