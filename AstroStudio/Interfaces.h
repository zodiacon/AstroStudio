#pragma once

#include "ChartData.h"

struct IView;
struct ChartInfo;

struct IMainFrame abstract {
	virtual HWND GetHwnd() const = 0;
	virtual BOOL TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y) = 0;
	virtual CUpdateUIBase& GetUI() = 0;
	virtual IView* AddChartView(ChartData data, PCWSTR title = nullptr) = 0;
	virtual ChartInfo& DefaultChartInfo() = 0;
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

