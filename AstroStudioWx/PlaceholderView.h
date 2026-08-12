#pragma once

#include "Interfaces.h"

//
// Temporary stand-in so the phase 1 shell is actually exercisable: tabs can be
// opened, switched, closed and listed, command routing to the active page can
// be demonstrated, and the Window menu has something to enumerate.
//
// Replaced by the real views:
//   phase 2  CChartView      -> splitter + nested notebook
//   phase 5  CEphemerisView  -> wxListCtrl in virtual mode
//
class PlaceholderView : public wxPanel, public IView {
public:
	PlaceholderView(wxWindow* parent, wxString const& title, wxString const& note);

	// IView
	void PageActivated(bool active) override;
	bool ProcessCommand(int id) override;
	void UpdateUI(wxUpdateUIEvent& event) override;

private:
	wxStaticText* m_State{};
	int m_CopyCount{ 0 };
};
