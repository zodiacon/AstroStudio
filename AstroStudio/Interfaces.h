#pragma once

#include "ChartData.h"
#include "DerivedCharts.h"

struct IView;
struct ChartInfo;

// what a chart can show around itself at a moment (see IView::ShowMoment)
enum class MomentKind { Transits, Progressions, SolarArc };

// a chart open in a tab, as a copy
struct OpenChart {
	CString Name;
	ChartData Data;
	IView* View{ nullptr };		// the tab it was copied from (to tell whether it is still open), if it came from one
	bool Active{ false };		// that tab is the one showing
};

// what a tab has to say in the frame's status bar panes (empty text = nothing in that pane)
struct StatusInfo {
	CString Name;		// the chart or analysis
	CString Time;		// its moment (or range), in its own zone
	CString Place;
	CString Details;	// house system, harmonic, Live/Auto, the overlay ...
};

struct IMainFrame abstract {
	virtual HWND GetHwnd() const = 0;
	virtual BOOL TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y) = 0;
	virtual CUpdateUIBase& GetUI() = 0;
	// Opens a chart in a new tab. filePath is the file it came from, if any.
	virtual IView* AddChartView(ChartData data, PCWSTR title = nullptr, PCWSTR filePath = nullptr) = 0;
	// Puts a chart file at the top of the File menu's recent files list (after it was opened or saved).
	virtual void AddRecentFile(PCWSTR path) = 0;
	// changes the text of a view's tab
	virtual void SetViewTitle(IView* view, PCWSTR title) = 0;
	// brings a view's tab to the front
	virtual void ActivateView(IView* view) = 0;
	virtual ChartInfo& DefaultChartInfo() = 0;

	// Asks for the details of a new chart (the dialog opens with `initial`, or with the current time and
	// the default location if null) and opens it. Returns null if the dialog was cancelled.
	// The house system the dialog starts with is the last one used, unless one is given.
	virtual IView* NewChartWithDialog(ChartInfo const* initial = nullptr, HouseSystem const* houseSystem = nullptr) = 0;
	// Opens a chart worked out from others (a composite, a Davison chart, a solar arc chart) in a new tab, read-only: its details
	// are shown but can't be edited and the planets are kept as they are (never recalculated). With the recipe it was made from
	// it can be saved (as that recipe: see ChartFile), and filePath is the file it came from, if it did. Returns the new view.
	virtual IView* AddDerivedChartView(ChartData data, PCWSTR title, DerivedRecipe const* recipe = nullptr, PCWSTR filePath = nullptr) = 0;
	virtual BOOL AddToolBarToUI(HWND) = 0;

	// True while the startup geolocation lookup is still running, so a new
	// "chart for now" knows its location is a placeholder.
	virtual bool IsLocationPending() const = 0;

	// Copies of the charts open in tabs, in tab order, leaving out `except` (the asker).
	virtual std::vector<OpenChart> OpenCharts(IView* except = nullptr) = 0;

	// Asks what to analyse (the analysis dialog, starting on this chart - or on the one showing, if none is given) and opens the
	// result in a new tab.
	virtual void NewAnalysis(IView* chart = nullptr) = 0;
};

struct IView {
	virtual void PageActivated(bool active) {}
	virtual bool ProcessCommand(UINT cmd) {
		return false;
	}
	// Called before the view is closed - one tab, all of them, or the program exiting. A view with unsaved
	// work asks the user here; returning false cancels the closing.
	virtual bool CanClose() {
		return true;
	}
	// The text font (AppSettings::TextFont) was changed with Options > Font.
	virtual void TextFontChanged() {}
	// The aspect settings (AspectOptions::Current) changed: a view that shows aspects works them out again.
	virtual void AspectSettingsChanged() {}
	// the option to have the Part of Fortune among a chart's planets (AppSettings::ShowPartOfFortune) was switched
	virtual void PartOfFortuneChanged() {}
	// what the chart wheel draws (WheelOptions::Current) or the colours it draws in (ChartColors::Current) changed
	virtual void WheelOptionsChanged() {}
	// changed since it was last saved or opened (what the project pane marks with a *); a view with nothing to save says no
	virtual bool IsModified() const {
		return false;
	}
	// the file the view's document lives in, or null
	virtual PCWSTR FilePath() const {
		return nullptr;
	}
	// Shows the moment (UT) around the chart - the sky then, or the chart progressed or moved by the solar arc to it - as an overlay.
	// False if the view is not a chart, or can't show that (a read-only chart has nothing to progress).
	virtual bool ShowMoment(DateTime const& ut, MomentKind kind) {
		return false;
	}
	// The chart the view shows, as a copy with its name (the tab's text), if it is a chart.
	virtual bool GetChart(OpenChart& chart) const {
		return false;
	}
	// What the status bar's panes show while this view's tab is the active one; false (the default) leaves them empty. The frame asks
	// a few times a second, so it should be cheap and reflect the view's current state.
	virtual bool GetStatusInfo(StatusInfo& info) const {
		return false;
	}
};

const UINT WM_RECALC = WM_APP + 1;

//
// Posted from the geolocation thread pool callback to the frame.
// lParam is a heap LocationRequest* the handler takes ownership of.
//
const UINT WM_LOCATION_READY = WM_APP + 2;

//
// Sent by the frame to every open page once the above lands, so views waiting
// on a location can fill it in. wParam is non-zero if the lookup succeeded.
// Broadcast by window handle rather than by view pointer, so a page closed in
// the meantime simply isn't there to receive it.
//
const UINT WM_LOCATION_UPDATED = WM_APP + 3;

enum class Recalc {
	All,
	Houses = 1,
	Planets = 2,
};

