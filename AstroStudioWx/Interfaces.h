#pragma once

#include "ChartData.h"

//
// The wx counterpart of AstroStudio\Interfaces.h.
//
// Most of the WTL contract had no reason to survive:
//
//   IMainFrame::GetHwnd()          - views are wxWindows, they don't need it
//   IMainFrame::GetUI()            - CAutoUpdateUI is replaced by wxEVT_UPDATE_UI,
//                                    which is per-window, so there is no shared
//                                    UI-state object to hand out
//   IMainFrame::AddToolBarToUI()   - same reason
//   IMainFrame::TrackPopupMenu()   - wxWindow::PopupMenu is available on every
//                                    window already
//   WM_RECALC / WM_HERE_RESULT     - custom window messages become ordinary
//                                    method calls and wxThreadEvents
//
// What remains is IView. It is a mixin: views derive from a wx window class
// *and* from IView, so the frame recovers the interface from a notebook page
// with dynamic_cast rather than the parallel GetPageData() bookkeeping
// CNativeCustomTabView needed.
//
// IMainFrame is back as of phase 2, but with two methods instead of six - see
// its declaration below.
//
struct IView {
	virtual ~IView() = default;

	// The notebook selection moved to or away from this page.
	// Replaces IView::PageActivated + TBVN_PAGEACTIVATED.
	virtual void PageActivated(bool active) {}

	// A frame-level command that the frame did not handle itself. Return true
	// if the view consumed it. Replaces CViewBase::ProcessCommand, which
	// re-dispatched WM_COMMAND into the view's message map.
	virtual bool ProcessCommand(int id) {
		return false;
	}

	// Lets the active page contribute enable/check state for menu and toolbar
	// items it owns. Replaces CViewBase::UpdateUI(CUpdateUIBase&) and the
	// idle-driven UIUpdateToolBar plumbing.
	virtual void UpdateUI(wxUpdateUIEvent& event) {}
};

//
// What a view is allowed to ask of the frame. The WTL version had six methods;
// four of them existed only to work around WTL plumbing and are gone (see
// above). These two are real collaboration.
//
struct IMainFrame {
	virtual ~IMainFrame() = default;

	// Open a new chart tab for an already-populated ChartData.
	virtual IView* AddChartView(ChartData data, wxString const& title = {}) = 0;

	// Birth data used to seed a "chart for now": location, timezone. The WTL
	// build fills this from a background geolocation lookup started in
	// CMainFrame::OnCreate; that arrives in phase 6.
	virtual ChartInfo const& DefaultChartInfo() const = 0;
};

//
// Replaces the WM_RECALC (WM_APP + 1) custom message. CChartView posted it to
// itself with the enum in wParam; here it is just an argument to a method.
//
enum class Recalc {
	All,
	Houses,
	Planets,
};
