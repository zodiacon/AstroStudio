#pragma once

#include "Interfaces.h"

class wxAuiNotebook;
class wxAuiNotebookEvent;

//
// Replacement for CMainFrame (AstroStudio\MainFrm.h).
//
// The WTL class inherited from five bases - CFrameWindowImpl, CAutoUpdateUI,
// IMainFrame, CMessageFilter and CIdleHandler - because the frame had to run
// the toolbar UI-update loop and the message pump by hand. wxFrame does all of
// that, so this is a plain single-inheritance class.
//
class MainFrame : public wxFrame, public IMainFrame {
public:
	MainFrame();

	// IMainFrame
	IView* AddChartView(ChartData data, wxString const& title = {}) override;
	ChartInfo const& DefaultChartInfo() const override;

private:
	void BuildMenuBar();
	void BuildToolBar();
	void RebuildWindowMenu();

	void AddView(wxWindow* view, wxString const& title, wxString const& iconName);
	IView* ActiveView() const;
	IView* ViewAt(int page) const;

	void OnAbout(wxCommandEvent& e);
	void OnEphemeris(wxCommandEvent& e);
	void OnNewChart(wxCommandEvent& e);
	void OnNewChartForNow(wxCommandEvent& e);
	void OnWindowClose(wxCommandEvent& e);
	void OnWindowCloseAll(wxCommandEvent& e);
	void OnWindowActivate(wxCommandEvent& e);
	void OnToggleStatusBar(wxCommandEvent& e);
	void OnToggleAlwaysOnTop(wxCommandEvent& e);
	void OnToggleDarkMode(wxCommandEvent& e);
	void OnForwardToView(wxCommandEvent& e);

	void OnPageChanged(wxAuiNotebookEvent& e);
	void OnPageClosed(wxAuiNotebookEvent& e);
	void OnUpdateFrameItem(wxUpdateUIEvent& e);
	void OnUpdateViewItem(wxUpdateUIEvent& e);

	wxAuiNotebook* m_Notebook{};
	wxMenu* m_WindowMenu{};
	int m_CurrentPage{ wxNOT_FOUND };
	int m_ChartCounter{ 0 };
	bool m_DarkMode{ true };
	ChartInfo m_DefaultChartInfo{};
};
