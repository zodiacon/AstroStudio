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
};

struct IView {
	virtual void PageActivated(bool active) {}
	virtual bool ProcessCommand(UINT cmd) {
		return false;
	}
};

const UINT WM_RECALC = WM_APP + 1;

enum class Recalc {
	All,
	Houses = 1,
	Planets = 2,
};

