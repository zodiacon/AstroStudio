#pragma once

#include "ChartData.h"

struct IView;
struct ChartInfo;

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
	virtual IView* NewChartWithDialog(ChartInfo const* initial = nullptr) = 0;
	virtual BOOL AddToolBarToUI(HWND) = 0;

	// True while the startup geolocation lookup is still running, so a new
	// "chart for now" knows its location is a placeholder.
	virtual bool IsLocationPending() const = 0;
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
	// the file the view's document lives in, or null
	virtual PCWSTR FilePath() const {
		return nullptr;
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

