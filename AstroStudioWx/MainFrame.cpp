#include "pch.h"
#include "MainFrame.h"

#include "ChartView.h"
#include "EphemerisView.h"
#include "Interfaces.h"
#include "Resources.h"
#include "resource.h"
#include "AstroHelpers.h"

#include <wx/aboutdlg.h>
#include <wx/aui/auibook.h>

MainFrame::MainFrame()
	: wxFrame(nullptr, wxID_ANY, "AstroStudio", wxDefaultPosition, wxSize(1100, 800)) {
	SetIcon(wxICON(appicon));

	// Carried over from CMainFrame::OnGetMinMaxInfo, which clamped the frame to
	// 600x600. FromDIP makes it a real 600x600 at any scaling factor.
	SetMinSize(FromDIP(wxSize(600, 600)));

	InitDefaultChartInfo();

	BuildMenuBar();
	BuildToolBar();

	CreateStatusBar(2);
	SetStatusText("Ready", 0);
	SetStatusText(wxVERSION_STRING, 1);

	// wxAUI_NB_DEFAULT_STYLE would put a close button on the active tab;
	// CMainFrame set m_bTabCloseButton = FALSE, so closing stays a Window menu
	// command and the flags are spelled out instead.
	m_Notebook = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxAUI_NB_TOP | wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS);

	m_Notebook->Bind(wxEVT_AUINOTEBOOK_PAGE_CHANGED, &MainFrame::OnPageChanged, this);
	m_Notebook->Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, &MainFrame::OnPageClosed, this);

	Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);
	Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(true); }, wxID_EXIT);
	Bind(wxEVT_MENU, &MainFrame::OnEphemeris, this, ID_TOOL_EPHEMERIS);
	Bind(wxEVT_MENU, &MainFrame::OnNewChart, this, ID_NEW_CHART);
	Bind(wxEVT_MENU, &MainFrame::OnNewChartForNow, this, ID_NEW_CHARTFORNOW);
	Bind(wxEVT_MENU, &MainFrame::OnWindowClose, this, ID_WINDOW_CLOSE);
	Bind(wxEVT_MENU, &MainFrame::OnWindowCloseAll, this, ID_WINDOW_CLOSE_ALL);
	Bind(wxEVT_MENU, &MainFrame::OnToggleStatusBar, this, ID_VIEW_STATUS_BAR);
	Bind(wxEVT_MENU, &MainFrame::OnToggleAlwaysOnTop, this, ID_OPTIONS_ALWAYSONTOP);
	Bind(wxEVT_MENU, &MainFrame::OnToggleDarkMode, this, ID_OPTIONS_DARKMODE);

	// One Bind for the whole dynamic Window-menu range, the direct equivalent
	// of WTL's COMMAND_RANGE_HANDLER(ID_WINDOW_TABFIRST, ID_WINDOW_TABLAST, ...).
	Bind(wxEVT_MENU, &MainFrame::OnWindowActivate, this, ID_WINDOW_TABFIRST, ID_WINDOW_TABLAST);

	// Commands the frame owns no state for: hand them to the active page.
	Bind(wxEVT_MENU, &MainFrame::OnForwardToView, this, wxID_UNDO);
	Bind(wxEVT_MENU, &MainFrame::OnForwardToView, this, wxID_CUT);
	Bind(wxEVT_MENU, &MainFrame::OnForwardToView, this, wxID_COPY);
	Bind(wxEVT_MENU, &MainFrame::OnForwardToView, this, wxID_PASTE);

	//
	// Replaces CAutoUpdateUI + CIdleHandler::OnIdle + UIUpdateToolBar.
	//
	// These MUST be bound per id. wxUpdateUIEvent derives from wxCommandEvent,
	// so it propagates up from every child window, and wx also sends one to
	// each window itself on idle. A catch-all Bind with a default
	// "e.Enable(false)" branch therefore does not just grey out menu items - it
	// disables the notebook, the toolbar, the status bar and the frame itself,
	// leaving a window that renders correctly but beeps at every click.
	//
	Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateFrameItem, this, ID_VIEW_STATUS_BAR);
	Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateFrameItem, this, ID_OPTIONS_DARKMODE);
	Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateFrameItem, this, ID_OPTIONS_ALWAYSONTOP);
	Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateFrameItem, this, ID_WINDOW_CLOSE);
	Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateFrameItem, this, ID_WINDOW_CLOSE_ALL);

	// Menu items present in IDR_MAINFRAME but implemented in neither build.
	// Typed as int[] because the wxID_* constants are enumerators while
	// ID_OPTIONS_FONT is a macro, so a bare braced list has no common type.
	static constexpr int unimplemented[] = {
		wxID_OPEN, wxID_SAVE, wxID_SAVEAS, wxID_PRINT, wxID_PREVIEW,
		wxID_PRINT_SETUP, ID_OPTIONS_FONT,
	};
	for (auto id : unimplemented)
		Bind(wxEVT_UPDATE_UI, [](wxUpdateUIEvent& e) { e.Enable(false); }, id);

	// Commands the active page owns.
	static constexpr int viewOwned[] = { wxID_UNDO, wxID_CUT, wxID_COPY, wxID_PASTE };
	for (auto id : viewOwned)
		Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateViewItem, this, id);

	Centre();

	// CMainFrame did this with PostMessage(WM_COMMAND, ID_TOOL_EPHEMERIS) at
	// the end of OnCreate.
	CallAfter([this] {
		wxCommandEvent dummy(wxEVT_MENU, ID_TOOL_EPHEMERIS);
		OnEphemeris(dummy);
		});
}

//
// The menu is hand-built: wx cannot load a Win32 RC menu, so IDR_MAINFRAME is
// transcribed here. Accelerator text after '\t' is parsed by wx, which
// registers the accelerator for us - no separate ACCELERATORS resource.
//
void MainFrame::BuildMenuBar() {
	auto newMenu = new wxMenu;
	newMenu->Append(ID_NEW_CHART, "&Chart...");
	newMenu->Append(ID_NEW_CHARTFORNOW, "Chart for &Now");

	auto fileMenu = new wxMenu;
	fileMenu->AppendSubMenu(newMenu, "&New");
	fileMenu->Append(wxID_OPEN, "&Open...\tCtrl+O");
	fileMenu->Append(wxID_SAVE, "&Save\tCtrl+S");
	fileMenu->Append(wxID_SAVEAS, "Save &As...");
	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_PRINT, "&Print...\tCtrl+P");
	fileMenu->Append(wxID_PREVIEW, "Print Pre&view");
	fileMenu->Append(wxID_PRINT_SETUP, "P&rint Setup...");
	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_EXIT, "E&xit");

	auto editMenu = new wxMenu;
	editMenu->Append(wxID_UNDO, "&Undo\tCtrl+Z");
	editMenu->AppendSeparator();
	editMenu->Append(wxID_CUT, "Cu&t\tCtrl+X");
	editMenu->Append(wxID_COPY, "&Copy\tCtrl+C");
	editMenu->Append(wxID_PASTE, "&Paste\tCtrl+V");

	auto viewMenu = new wxMenu;
	viewMenu->AppendCheckItem(ID_VIEW_STATUS_BAR, "&Status Bar");

	auto toolMenu = new wxMenu;
	toolMenu->Append(ID_TOOL_EPHEMERIS, "&Ephemeris");

	auto optionsMenu = new wxMenu;
	optionsMenu->AppendCheckItem(ID_OPTIONS_ALWAYSONTOP, "&Always On Top");
	optionsMenu->Append(ID_OPTIONS_FONT, "&Font...");
	optionsMenu->AppendCheckItem(ID_OPTIONS_DARKMODE, "&Dark Mode");

	m_WindowMenu = new wxMenu;
	m_WindowMenu->Append(ID_WINDOW_CLOSE, "&Close\tCtrl+F4");
	m_WindowMenu->Append(ID_WINDOW_CLOSE_ALL, "Close &All");

	auto helpMenu = new wxMenu;
	helpMenu->Append(wxID_ABOUT, "&About AstroStudio...");

	auto bar = new wxMenuBar;
	bar->Append(fileMenu, "&File");
	bar->Append(editMenu, "&Edit");
	bar->Append(viewMenu, "&View");
	bar->Append(toolMenu, "&Tool");
	bar->Append(optionsMenu, "&Options");
	bar->Append(m_WindowMenu, "&Window");
	bar->Append(helpMenu, "&Help");
	SetMenuBar(bar);
}

void MainFrame::BuildToolBar() {
	// Same three buttons and separator as the ToolBarButtonInfo array in
	// CMainFrame::OnCreate.
	auto tb = CreateToolBar(wxTB_FLAT | wxTB_HORIZONTAL);
	tb->AddTool(ID_TOOL_EPHEMERIS, "Ephemeris", Resources::Icon(ICON_EPHEMERIS), "Ephemeris");
	tb->AddSeparator();
	tb->AddTool(ID_NEW_CHART, "New Chart", Resources::Icon(ICON_CHART), "New chart");
	tb->AddTool(ID_NEW_CHARTFORNOW, "Chart for Now", Resources::Icon(ICON_CHARTNOW), "Chart for now");
	tb->Realize();
}

//
// CMainFrame handed the Window popup to CNativeCustomTabView via
// SetWindowMenu() and let it maintain the tab list. wxAuiNotebook has no such
// hook, so the list is rebuilt whenever the page set changes. It is a handful
// of items, so rebuilding is cheaper than tracking deltas.
//
void MainFrame::RebuildWindowMenu() {
	if (!m_WindowMenu)
		return;

	while (m_WindowMenu->GetMenuItemCount() > 2)
		m_WindowMenu->Destroy(m_WindowMenu->FindItemByPosition(2));

	auto count = static_cast<int>(m_Notebook->GetPageCount());
	if (count == 0)
		return;

	m_WindowMenu->AppendSeparator();
	for (int i = 0; i < count && ID_WINDOW_TABFIRST + i <= ID_WINDOW_TABLAST; i++) {
		auto item = m_WindowMenu->AppendRadioItem(ID_WINDOW_TABFIRST + i,
			wxString::Format("&%d %s", i + 1, m_Notebook->GetPageText(i)));
		if (i == m_Notebook->GetSelection())
			item->Check();
	}
}

void MainFrame::AddView(wxWindow* view, wxString const& title, wxString const& iconName) {
	m_Notebook->AddPage(view, title, true, Resources::Icon(iconName));
	RebuildWindowMenu();
}

IView* MainFrame::ViewAt(int page) const {
	if (page < 0 || page >= static_cast<int>(m_Notebook->GetPageCount()))
		return nullptr;

	// The notebook stores wxWindow*; views mix in IView, so one dynamic_cast
	// replaces CNativeCustomTabView's parallel GetPageData()/SetPageData()
	// bookkeeping.
	return dynamic_cast<IView*>(m_Notebook->GetPage(page));
}

IView* MainFrame::ActiveView() const {
	return ViewAt(m_Notebook->GetSelection());
}

void MainFrame::OnEphemeris(wxCommandEvent&) {
	AddView(new EphemerisView(m_Notebook, this), "Ephemeris", ICON_EPHEMERIS);
}

void MainFrame::OnNewChart(wxCommandEvent&) {
	// CMainFrame::OnNewChart created an empty CChartView and left it blank
	// until the details form supplied birth data. With that form still a phase
	// 4 stub, seeding from "now" is the only way to get a populated chart, so
	// both commands do the same thing for the moment.
	auto view = new ChartView(m_Notebook, this);
	AddView(view, wxString::Format("Chart %d", ++m_ChartCounter), ICON_CHART);
	view->ChartForNow();
}

void MainFrame::OnNewChartForNow(wxCommandEvent&) {
	// The WTL version blocks the UI thread here for up to 3 seconds waiting on
	// the geolocation lookup started in OnCreate (MainFrm.cpp:126). Phase 6
	// replaces that with a wxThreadEvent continuation; there is nothing to wait
	// for yet.
	auto view = new ChartView(m_Notebook, this);
	AddView(view, wxString::Format("Chart %d", ++m_ChartCounter), ICON_CHARTNOW);
	view->ChartForNow();
}

IView* MainFrame::AddChartView(ChartData data, wxString const& title) {
	auto view = new ChartView(m_Notebook, this);
	AddView(view, title.empty() ? wxString::Format("Chart %d", ++m_ChartCounter) : title,
		ICON_CHART);
	view->SetChart(std::move(data));
	return view;
}

ChartInfo const& MainFrame::DefaultChartInfo() const {
	return m_DefaultChartInfo;
}

//
// Phase 6 fills this from NetworkHelper::FillInfoFromLocal on a background
// thread, as CMainFrame::OnCreate does. Until then it is Greenwich rather than
// a zeroed ChartInfo.
//
// That is not just cosmetic. A zeroed ChartInfo puts the chart at 0N 0E, and
// on the equator every quadrant house system degenerates to the same division
// - Koch, Placidus and Campanus return byte-identical cusps there - so
// changing the House System combo appears to do nothing at all.
//
void MainFrame::InitDefaultChartInfo() {
	m_DefaultChartInfo.Latitude = 51.4779;		// Royal Observatory, Greenwich
	m_DefaultChartInfo.Longitude = -0.0015;
	m_DefaultChartInfo.Elevation = 0;
	m_DefaultChartInfo.City = L"Greenwich";
	m_DefaultChartInfo.Country = L"United Kingdom";
	m_DefaultChartInfo.Time = DateTime::Now();
}

void MainFrame::OnWindowClose(wxCommandEvent&) {
	auto page = m_Notebook->GetSelection();
	if (page == wxNOT_FOUND) {
		wxBell();
		return;
	}
	m_Notebook->DeletePage(page);
	m_CurrentPage = m_Notebook->GetSelection();
	RebuildWindowMenu();
}

void MainFrame::OnWindowCloseAll(wxCommandEvent&) {
	m_Notebook->DeleteAllPages();
	m_CurrentPage = wxNOT_FOUND;
	RebuildWindowMenu();
}

void MainFrame::OnWindowActivate(wxCommandEvent& e) {
	m_Notebook->SetSelection(e.GetId() - ID_WINDOW_TABFIRST);
}

void MainFrame::OnToggleStatusBar(wxCommandEvent&) {
	auto sb = GetStatusBar();
	sb->Show(!sb->IsShown());
	// Re-runs the frame layout so the notebook takes back the freed space;
	// the WTL version called UpdateLayout() for the same reason.
	SendSizeEvent();
}

void MainFrame::OnToggleAlwaysOnTop(wxCommandEvent&) {
	// ID_OPTIONS_ALWAYSONTOP was in the WTL menu but never handled; wired up
	// here because wx makes it a one-liner.
	SetWindowStyleFlag(GetWindowStyleFlag() ^ wxSTAY_ON_TOP);
}

void MainFrame::OnToggleDarkMode(wxCommandEvent&) {
	m_DarkMode = !m_DarkMode;

	auto result = wxTheApp->SetAppearance(m_DarkMode
		? wxApp::Appearance::Dark
		: wxApp::Appearance::Light);

	if (result == wxApp::AppearanceResult::CannotChange) {
		m_DarkMode = !m_DarkMode;
		SetStatusText("Appearance can only be changed at startup", 0);
	}
	else {
		SetStatusText(m_DarkMode ? "Dark mode on" : "Dark mode off", 0);
	}
}

void MainFrame::OnForwardToView(wxCommandEvent& e) {
	if (auto view = ActiveView(); view && view->ProcessCommand(e.GetId()))
		return;
	e.Skip();
}

void MainFrame::OnPageChanged(wxAuiNotebookEvent& e) {
	// CMainFrame::OnPageActivated did exactly this pairing by hand, tracking
	// m_CurrentPage to deliver the deactivation.
	if (auto previous = ViewAt(m_CurrentPage))
		previous->PageActivated(false);

	m_CurrentPage = e.GetSelection();

	if (auto current = ViewAt(m_CurrentPage))
		current->PageActivated(true);

	RebuildWindowMenu();
	e.Skip();
}

void MainFrame::OnPageClosed(wxAuiNotebookEvent& e) {
	e.Skip();
	CallAfter([this] {
		m_CurrentPage = m_Notebook->GetSelection();
		RebuildWindowMenu();
		});
}

//
// The frame's own UI state: the WTL equivalent was UIAddMenu/UIAddToolBar plus
// UISetCheck calls scattered through the command handlers, applied on idle by
// UIUpdateToolBar.
//
void MainFrame::OnUpdateFrameItem(wxUpdateUIEvent& e) {
	switch (e.GetId()) {
		case ID_VIEW_STATUS_BAR:
			e.Check(GetStatusBar() && GetStatusBar()->IsShown());
			break;

		case ID_OPTIONS_DARKMODE:
			e.Check(m_DarkMode);
			break;

		case ID_OPTIONS_ALWAYSONTOP:
			e.Check((GetWindowStyleFlag() & wxSTAY_ON_TOP) != 0);
			break;

		case ID_WINDOW_CLOSE:
		case ID_WINDOW_CLOSE_ALL:
			e.Enable(m_Notebook && m_Notebook->GetPageCount() > 0);
			break;
	}
}

//
// Commands the frame has no state for. Disabled unless the active page claims
// them - the wx counterpart of CViewBase::UpdateUI(CUpdateUIBase&).
//
void MainFrame::OnUpdateViewItem(wxUpdateUIEvent& e) {
	e.Enable(false);
	if (auto view = ActiveView())
		view->UpdateUI(e);
}

void MainFrame::OnAbout(wxCommandEvent&) {
	wxAboutDialogInfo info;
	info.SetName("AstroStudio");
	info.SetVersion("0.1 (wxWidgets port)");
	info.SetDescription(wxString::Format(
		"Astrology charting built on the Swiss Ephemeris.\n\nBuilt with %s",
		wxVERSION_STRING));
	info.SetCopyright("(C) Pavel Yosifovich");
	wxAboutBox(info, this);
}
