#pragma once

//
// Icon loading for the toolbar and the notebook tabs.
//
// The WTL build built a 16x16 CImageList by hand (CMainFrame::OnCreate) and
// let ToolbarHelper size the toolbar images. wxBitmapBundle does that job
// properly instead: wxIconBundle's MSW constructor pulls *every* size present
// in the .ico resource, and the bundle then hands wx whichever one matches the
// current monitor's scaling. No manual 16/24/32 juggling, and it stays sharp
// when the window moves to a different-DPI display.
//
namespace Resources {
	// name is one of the ICON_* strings in resource.h.
	wxBitmapBundle Icon(wxString const& name);
}
