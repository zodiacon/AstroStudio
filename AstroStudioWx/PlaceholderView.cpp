#include "pch.h"
#include "PlaceholderView.h"

PlaceholderView::PlaceholderView(wxWindow* parent, wxString const& title, wxString const& note)
	: wxPanel(parent) {
	auto sizer = new wxBoxSizer(wxVERTICAL);

	auto heading = new wxStaticText(this, wxID_ANY, title);
	auto font = heading->GetFont();
	font.SetPointSize(font.GetPointSize() + 6);
	font.SetWeight(wxFONTWEIGHT_BOLD);
	heading->SetFont(font);

	m_State = new wxStaticText(this, wxID_ANY, wxEmptyString);

	sizer->AddStretchSpacer();
	sizer->Add(heading, wxSizerFlags().Center().Border(wxALL, FromDIP(6)));
	sizer->Add(new wxStaticText(this, wxID_ANY, note),
		wxSizerFlags().Center().Border(wxALL, FromDIP(6)));
	sizer->Add(m_State, wxSizerFlags().Center().Border(wxALL, FromDIP(6)));
	sizer->AddStretchSpacer();

	SetSizer(sizer);
}

void PlaceholderView::PageActivated(bool active) {
	// The WTL version drove this from TBVN_PAGEACTIVATED plus the m_CurrentPage
	// bookkeeping in CMainFrame::OnPageActivated; here it is just the notebook
	// telling the frame its selection changed.
	m_State->SetLabel(active ? "page activated" : "page deactivated");
	Layout();
}

bool PlaceholderView::ProcessCommand(int id) {
	// Demonstrates the frame -> active view command hop that replaces WTL's
	// CViewBase::ProcessCommand re-dispatch into the message map.
	if (id == wxID_COPY) {
		m_State->SetLabel(wxString::Format("handled Edit/Copy (%d)", ++m_CopyCount));
		Layout();
		return true;
	}
	return false;
}

void PlaceholderView::UpdateUI(wxUpdateUIEvent& event) {
	// Only this page knows whether Copy means anything for it. Under WTL this
	// was CViewBase::UpdateUI writing into the frame's shared CUpdateUIBase on
	// idle; wx delivers the query to us directly.
	if (event.GetId() == wxID_COPY)
		event.Enable(true);
}

