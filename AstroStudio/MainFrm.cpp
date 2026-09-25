// MainFrm.cpp : implmentation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "EphemerisView.h"
#include "AboutDlg.h"
#include "MainFrm.h"
#include "Helpers.h"
#include "ToolbarHelper.h"
#include "ChartView.h"
#include "NewChartDlg.h"
#include "ChartFile.h"
#include <filesystem>
#include "TimeZones.h"
#include "NetworkHelper.h"
#include "AppSettings.h"
#include "AspectOptionsDlg.h"
#include "WheelOptionsDlg.h"
#include "ChartColorsDlg.h"
#include "AnalysisDlg.h"
#include "AnalysisView.h"
#include <WTLHelper.h>

#define WINDOW_MENU_POSITION	6

namespace {
	// where the recent files list is kept (under HKEY_CURRENT_USER), and how many it holds
	constexpr PCWSTR RecentFilesKey = LR"(Software\AstroStudio)";
	constexpr int MaxRecentFiles = 8;

	// Owned by the thread pool callback until it is posted to the frame, which
	// then takes ownership - the same hand-off CChartDetailsView::OnHere uses.
	struct LocationRequest {
		HWND Wnd;
		ChartInfo Info;
	};
}

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
	if (CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
		return TRUE;

	return m_view.PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle() {
	UIUpdateToolBar();
	return FALSE;
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	//
	// The lookup fills its own ChartInfo and posts it back, rather than writing
	// straight into m_DefaultChartInfo and signalling an event for the UI thread
	// to block on. Nothing waits for it now: charts open immediately with a
	// placeholder location and are filled in when it lands.
	//
	auto request = new LocationRequest{ m_hWnd };
	m_LocationPending = true;
	if (!::TrySubmitThreadpoolCallback([](auto, auto ctx) {
		auto req = static_cast<LocationRequest*>(ctx);
		auto success = NetworkHelper::FillInfoFromLocal(req->Info);
		if (!::PostMessage(req->Wnd, WM_LOCATION_READY, (WPARAM)success, (LPARAM)req))
			delete req;
		}, request, nullptr)) {
		delete request;
		m_LocationPending = false;
	}

	ATLVERIFY(Helpers::LoadAstroFont(IDR_FONT));
	InitMenu(GetMenu());
	UIAddMenu(GetMenu());

	ToolBarButtonInfo buttons[] = {
		{ ID_FILE_OPEN, IDI_OPEN },
		{ ID_FILE_SAVE, IDI_SAVE },
		{ 0 },
		{ ID_TOOL_EPHEMERIS, IDI_EPHEMERIS },
		{ 0 },
		{ ID_NEW_CHART, IDI_CHART, 0, L"New" },
		{ ID_NEW_CHARTFORNOW, IDI_CHARTNOW, 0, L"Now" },
	};
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	auto tb = ToolbarHelper::CreateAndInitToolBar(m_hWnd, buttons, _countof(buttons));
	UIAddToolBar(tb);
	AddSimpleReBarBand(tb);

	CreateSimpleStatusBar();

	//m_view.m_bTabCloseButton = FALSE;
	m_hWndClient = m_view.Create(m_hWnd, rcDefault, nullptr, 
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_BORDER);
	if (AppSettings::Get().AlwaysOnTop()) {
		SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		UISetCheck(ID_OPTIONS_ALWAYSONTOP, 1);
	}
	UISetCheck(ID_VIEW_STATUS_BAR, 1);
	if (!AppSettings::Get().ViewStatusBar()) {
		::ShowWindow(m_hWndStatusBar, SW_HIDE);
		UISetCheck(ID_VIEW_STATUS_BAR, 0);
	}
	// stepping applies to chart pages only; a chart enables these while it is the active page
	UIEnable(ID_CHART_STEP_BACK, FALSE);
	UIEnable(ID_CHART_STEP_FORWARD, FALSE);
	UIEnable(ID_CHART_AUTOSTEP, FALSE);
	UIEnable(ID_CHART_LIVE, FALSE);
	UIEnable(ID_CHART_TRANSITS, FALSE);
	UIEnable(ID_CHART_OVERLAY, FALSE);
	UIEnable(ID_CHART_ANALYSIS, FALSE);
	for (UINT id : { ID_CHART_DERIVED_SOLARRETURN, ID_CHART_DERIVED_LUNARRETURN, ID_CHART_DERIVED_SOLARARC, ID_CHART_DERIVED_COMPOSITE, ID_CHART_DERIVED_DAVISON })
		UIEnable(id, FALSE);
	UIEnable(ID_CHART_OVERLAY_NONE, FALSE);
	UIEnable(ID_CHART_OVERLAY_PROGRESSED, FALSE);
	UIEnable(ID_CHART_OVERLAY_SOLARARC, FALSE);
	UIEnable(ID_CHART_OVERLAY_SYNASTRY, FALSE);
	UIEnable(ID_FILE_SAVE, FALSE);
	UIEnable(ID_FILE_SAVE_AS, FALSE);
	UIEnable(ID_FILE_EXPORT, FALSE);
	UISetCheck(ID_OPTIONS_DARKMODE, WTLHelper::IsDarkMode());

	CImageList images;
	images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 8, 4);
	UINT icons[] = {
		IDI_EPHEMERIS, IDI_CHART, IDI_EVENT,
	};
	for(auto icon : icons)
		images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
	m_view.SetImageList(images);

	auto pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	CMenuHandle menuMain = GetMenu();
	m_view.SetWindowMenu(menuMain.GetSubMenu(WINDOW_MENU_POSITION));

	// the recent files list fills in at the "(empty)" item of the File menu's Recent Files submenu
	m_Recent.SetMaxEntries(MaxRecentFiles);
	m_Recent.SetMaxItemLength(60);
	CMenuHandle menuFile = menuMain.GetSubMenu(0);
	for (int i = 0; i < menuFile.GetMenuItemCount(); i++) {
		CMenuHandle sub = menuFile.GetSubMenu(i);
		if (sub.m_hMenu && sub.GetMenuState(ID_FILE_MRU_FIRST, MF_BYCOMMAND) != (UINT)-1) {
			m_Recent.SetMenuHandle(sub);
			break;
		}
	}
	m_Recent.ReadFromRegistry(RecentFilesKey);
	UIEnable(ID_FILE_MRU_FIRST, m_Recent.m_arrDocs.GetSize() > 0);

	PostMessage(WM_COMMAND, ID_TOOL_EPHEMERIS);

	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	// the settings the views and the frame have been keeping up to date go to the Registry now
	WINDOWPLACEMENT wp{ sizeof(wp) };
	if (GetWindowPlacement(&wp))
		AppSettings::Get().MainWindowPlacement(wp);
	AppSettings::Get().Save();

	auto pLoop = _Module.GetMessageLoop();
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	bHandled = FALSE;
	return 1;
}

LRESULT CMainFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT CMainFrame::OnToolEphemeris(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto pView = new CEphemerisView(this);
	pView->Create(m_view, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0);
	m_view.AddPage(pView->m_hWnd, L"Ephemeris", 0, pView);

	return 0;
}

LRESULT CMainFrame::OnToolAnalysis(WORD, WORD, HWND, BOOL&) {
	NewAnalysis();
	return 0;
}

void CMainFrame::NewAnalysis(IView* chart) {
	auto charts = OpenCharts();
	if (charts.empty()) {
		AtlMessageBox(m_hWnd, L"Open a chart first: an analysis compares the sky with a chart.", L"Analysis", MB_ICONINFORMATION);
		return;
	}
	// starts on the chart it was asked for, or else the one that is showing, if there is one
	int index = 0;
	for (int i = 0; i < static_cast<int>(charts.size()); i++)
		if (chart ? charts[i].View == chart : charts[i].Active)
			index = i;

	CAnalysisDlg dlg;
	dlg.Init(&charts, index, CAnalysisDlg::Defaults());
	if (dlg.DoModal(m_hWnd) != IDOK)
		return;

	auto pView = new CAnalysisView(this);
	pView->Create(m_view, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0);
	m_view.AddPage(pView->m_hWnd, L"Analysis", 2, pView);
	pView->Analyse(charts[dlg.Chart()], dlg.Settings());
}

LRESULT CMainFrame::OnNewChart(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	NewChartWithDialog();
	return 0;
}

LRESULT CMainFrame::OnNewChartNow(WORD, WORD, HWND, BOOL&) {
	auto pView = new CChartView(this);
	pView->Create(m_view, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0);
	pView->ChartForNow();
	m_view.AddPage(pView->m_hWnd, L"NewChart", 1, pView);
	pView->SetFile(L"NewChart", nullptr);

	return 0;
}

//
// The geolocation lookup finished. Adopt the result, then let every open page
// know - a chart created while it was still running shows "<Locating...>" and
// fills itself in here.
//
LRESULT CMainFrame::OnLocationReady(UINT, WPARAM wParam, LPARAM lParam, BOOL&) {
	std::unique_ptr<LocationRequest> request(reinterpret_cast<LocationRequest*>(lParam));

	m_LocationPending = false;
	if (wParam)
		m_DefaultChartInfo = request->Info;

	for (int i = 0; i < m_view.GetPageCount(); i++)
		::SendMessage(m_view.GetPageHWND(i), WM_LOCATION_UPDATED, wParam, 0);

	return 0;
}

bool CMainFrame::IsLocationPending() const {
	return m_LocationPending;
}

LRESULT CMainFrame::OnAlwaysOnTop(WORD, WORD, HWND, BOOL&) {
	bool top = (GetExStyle() & WS_EX_TOPMOST) == 0;
	SetWindowPos(top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	UISetCheck(ID_OPTIONS_ALWAYSONTOP, top);
	AppSettings::Get().AlwaysOnTop(top ? 1 : 0);
	return 0;
}

LRESULT CMainFrame::OnFont(WORD, WORD, HWND, BOOL&) {
	// The font of the text that isn't astrological symbols (those are always the symbol font): Consolas until the user
	// picks another. lfHeight is kept in tenths of a point; the font dialog wants device units.
	LOGFONT lf = AppSettings::Get().TextFont();
	if (lf.lfFaceName[0] == 0) {
		lf = {};
		wcscpy_s(lf.lfFaceName, L"Consolas");
		lf.lfWeight = FW_NORMAL;
		lf.lfHeight = 100;
	}
	LOGFONT shown = lf;
	shown.lfHeight = -MulDiv(lf.lfHeight, CClientDC(m_hWnd).GetDeviceCaps(LOGPIXELSY), 720);

	CFontDialog dlg(&shown, CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL | CF_NOVERTFONTS, nullptr, m_hWnd);
	if (!WTLHelper::InvokeFontDialog(dlg))
		return 0;

	LOGFONT chosen{};
	dlg.GetCurrentFont(&chosen);
	chosen.lfHeight = std::clamp(dlg.GetSize(), 70, 180);
	AppSettings::Get().TextFont(chosen);
	for (int i = 0; i < m_view.GetPageCount(); i++)
		if (auto view = ViewOfPage(i))
			view->TextFontChanged();
	return 0;
}

LRESULT CMainFrame::OnChartColors(WORD, WORD, HWND, BOOL&) {
	// what the charts show follows Apply; Cancel puts back what was there
	auto show = [this](ChartColors const& colors) {
		ChartColors::Current() = colors;
		for (int i = 0; i < m_view.GetPageCount(); i++)
			if (auto view = ViewOfPage(i))
				view->WheelOptionsChanged();
	};
	CChartColorsDlg dlg;
	dlg.Init(ChartColors::Current(), WTLHelper::IsDarkMode(), show);
	if (dlg.DoModal(m_hWnd) != IDOK)
		return 0;

	show(dlg.GetColors());
	ChartColors::StoreInSettings();
	return 0;
}

LRESULT CMainFrame::OnWheelOptions(WORD, WORD, HWND, BOOL&) {
	CWheelOptionsDlg dlg;
	dlg.SetOptions(WheelOptions::Current());
	if (dlg.DoModal(m_hWnd) != IDOK)
		return 0;

	WheelOptions::Current() = dlg.GetOptions();
	WheelOptions::StoreInSettings();
	for (int i = 0; i < m_view.GetPageCount(); i++)
		if (auto view = ViewOfPage(i))
			view->WheelOptionsChanged();
	return 0;
}

LRESULT CMainFrame::OnAspectOptions(WORD, WORD, HWND, BOOL&) {
	CAspectOptionsDlg dlg;
	dlg.SetOptions(AspectOptions::Current());
	if (dlg.DoModal(m_hWnd) != IDOK)
		return 0;

	AspectOptions::Current() = dlg.GetOptions();
	AspectOptions::StoreInSettings();
	// every chart works its aspects out again
	for (int i = 0; i < m_view.GetPageCount(); i++)
		if (auto view = ViewOfPage(i))
			view->AspectSettingsChanged();
	return 0;
}

LRESULT CMainFrame::OnToggleDarkMode(WORD, WORD, HWND, BOOL&) {
	WTLHelper::SwitchToMode(WTLHelper::IsDarkMode() ? DarkModeKind::Classic : DarkModeKind::Dark, m_hWnd);
	AppSettings::Get().DarkMode(WTLHelper::IsDarkMode() ? 1 : 0);
	InitMenu(GetMenu());
	DrawMenuBar();
	UISetCheck(ID_OPTIONS_DARKMODE, WTLHelper::IsDarkMode());
	return 0;
}

IView* CMainFrame::AddChartView(ChartData data, PCWSTR title, PCWSTR filePath) {
	auto pView = new CChartView(this);
	pView->Create(m_view, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0);
	m_view.AddPage(pView->m_hWnd, title ? title : L"Chart", 1, pView);
	pView->Chart(std::move(data));
	pView->SetFile(title ? title : L"Chart", filePath);

	return pView;
}

LRESULT CMainFrame::OnFileOpen(WORD, WORD, HWND, BOOL&) {
	CSimpleFileDialog dlg(TRUE, ChartFile::Extension, nullptr, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ENABLESIZING, ChartFile::Filter, m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if(ok)
		OpenChartFile(dlg.m_szFileName);
	return 0;
}

bool CMainFrame::OpenChartFile(PCWSTR path) {
	// a chart that is already open is shown, not opened a second time
	for (int i = 0; i < m_view.GetPageCount(); i++) {
		auto view = ViewOfPage(i);
		if (view && view->FilePath() && _wcsicmp(view->FilePath(), path) == 0) {
			ActivatePage(i);
			AddRecentFile(path);
			return true;
		}
	}

	ChartData data;
	std::wstring error;
	if (!ChartFile::Load(path, data, error)) {
		CString message;
		message.Format(L"%s could not be opened:\n\n%s", path, error.c_str());
		AtlMessageBox(m_hWnd, (PCWSTR)message, L"Astro Studio", MB_ICONWARNING);
		return false;
	}
	auto title = std::filesystem::path(path).stem().wstring();
	AddChartView(std::move(data), title.c_str(), path);
	AddRecentFile(path);
	return true;
}

void CMainFrame::AddRecentFile(PCWSTR path) {
	m_Recent.AddToList(path);
	RecentFilesChanged();
}

void CMainFrame::RecentFilesChanged() {
	// with nothing in the list the menu shows a disabled "(empty)"; the menu updater would enable it again
	UIEnable(ID_FILE_MRU_FIRST, m_Recent.m_arrDocs.GetSize() > 0);
	m_Recent.WriteToRegistry(RecentFilesKey);
}

LRESULT CMainFrame::OnFileRecent(WORD, WORD wID, HWND, BOOL&) {
	CString path;
	if (!m_Recent.GetFromList(wID, path))
		return 0;

	if (::GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES) {
		// gone (moved, deleted, a drive that isn't there): say so and drop it from the list
		CString message;
		message.Format(L"%s could not be found.\n\nIt has been removed from the recent files.", (PCWSTR)path);
		AtlMessageBox(m_hWnd, (PCWSTR)message, L"Astro Studio", MB_ICONWARNING);
		m_Recent.RemoveFromList(wID);
		RecentFilesChanged();
		return 0;
	}
	OpenChartFile(path);
	return 0;
}

LRESULT CMainFrame::OnClose(UINT, WPARAM, LPARAM, BOOL& bHandled) {
	// leaving the program: every chart with unsaved changes gets to ask first
	bHandled = CanCloseAll() ? FALSE : TRUE;
	return 0;
}

IView* CMainFrame::AddDerivedChartView(ChartData data, PCWSTR title) {
	auto pView = new CChartView(this);
	pView->Create(m_view, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0);
	m_view.AddPage(pView->m_hWnd, title, 1, pView);
	pView->DerivedChart(std::move(data));
	pView->SetFile(title, nullptr);
	return pView;
}

IView* CMainFrame::NewChartWithDialog(ChartInfo const* initial, HouseSystem const* houseSystem) {
	ChartInfo info;
	if (initial) {
		info = *initial;
	}
	else {
		info = m_DefaultChartInfo;
		info.Time = DateTime::Now();
		info.TimeZone = TimeZones::Machine();
	}

	CNewChartDlg dlg;
	dlg.SetChartInfo(info);
	dlg.SetHouseSystem(houseSystem ? *houseSystem : static_cast<HouseSystem>(AppSettings::Get().LastHouseSystem()));
	if (dlg.DoModal(m_hWnd) != IDOK)
		return nullptr;
	AppSettings::Get().LastHouseSystem(static_cast<int>(dlg.GetHouseSystem()));

	auto title = dlg.GetTitle();
	return AddChartView(Helpers::CreateChartData(dlg.GetChartInfo(), dlg.GetHouseSystem()), title.IsEmpty() ? L"Chart" : (PCWSTR)title);
}

ChartInfo& CMainFrame::DefaultChartInfo() {
	return m_DefaultChartInfo;
}

LRESULT CMainFrame::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	AppSettings::Get().ViewStatusBar(bVisible ? 1 : 0);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}

LRESULT CMainFrame::OnWindowClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int nActivePage = m_view.GetActivePage();
	if (nActivePage != -1) {
		if (auto view = ViewOfPage(nActivePage); view && !view->CanClose())
			return 0;		// the user cancelled
		m_view.RemovePage(nActivePage);
	}
	else
		::MessageBeep((UINT)-1);

	return 0;
}

LRESULT CMainFrame::OnWindowCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (!CanCloseAll())
		return 0;		// the user cancelled
	m_view.RemoveAllPages();

	return 0;
}

LRESULT CMainFrame::OnWindowActivate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	ActivatePage(wID - ID_WINDOW_TABFIRST);
	return 0;
}

LRESULT CMainFrame::OnPageActivated(int, LPNMHDR hdr, BOOL&) {
	auto page = static_cast<int>(hdr->idFrom);
	if (auto previous = ViewOfPage(m_CurrentPage))
		previous->PageActivated(false);
	if (auto view = ViewOfPage(page))
		view->PageActivated(true);
	m_CurrentPage = page;

	return 0;
}

IView* CMainFrame::ViewOfPage(int page) const {
	if (page < 0 || page >= m_view.GetPageCount())
		return nullptr;
	return dynamic_cast<IView*>(static_cast<CMessageMap*>(m_view.GetPageData(page)));
}

int CMainFrame::PageOfView(IView* view) const {
	for (int i = 0; i < m_view.GetPageCount(); i++)
		if (ViewOfPage(i) == view)
			return i;
	return -1;
}

void CMainFrame::ActivatePage(int page) {
	m_view.SetActivePage(page);

	// SetActivePage doesn't announce the change (clicking a tab does), so the pages would not hear that
	// they were shown or hidden; pass on the same notification a click sends.
	NMHDR nmhdr{};
	nmhdr.hwndFrom = m_view;
	nmhdr.idFrom = page;
	nmhdr.code = TBVN_PAGEACTIVATED;
	BOOL handled = TRUE;
	OnPageActivated(0, &nmhdr, handled);
}

std::vector<OpenChart> CMainFrame::OpenCharts(IView* except) {
	std::vector<OpenChart> charts;
	for (int i = 0; i < m_view.GetPageCount(); i++)
		if (auto view = ViewOfPage(i); view && view != except)
			if (OpenChart chart; view->GetChart(chart)) {
				chart.View = view;
				chart.Active = i == m_view.GetActivePage();
				charts.push_back(std::move(chart));
			}
	return charts;
}

void CMainFrame::SetViewTitle(IView* view, PCWSTR title) {
	if (int page = PageOfView(view); page >= 0)
		m_view.SetPageTitle(page, title);
}

void CMainFrame::ActivateView(IView* view) {
	if (int page = PageOfView(view); page >= 0 && page != m_view.GetActivePage())
		ActivatePage(page);
}

bool CMainFrame::CanCloseAll() {
	for (int i = 0; i < m_view.GetPageCount(); i++)
		if (auto view = ViewOfPage(i); view && !view->CanClose())
			return false;
	return true;
}

void CMainFrame::InitMenu(HMENU menu) {
	MenuItemData commands[] = {
		{ ID_EDIT_COPY, IDI_COPY },
		{ ID_EDIT_PASTE, IDI_PASTE },
		{ ID_EDIT_CUT, IDI_CUT },
		{ ID_OPTIONS_ALWAYSONTOP, IDI_PIN },
		{ ID_TOOL_EPHEMERIS, IDI_EPHEMERIS },
		{ ID_NEW_CHART, IDI_CHART },
		{ ID_NEW_CHARTFORNOW, IDI_CHARTNOW },
		{ ID_FILE_OPEN, IDI_OPEN },
		{ ID_FILE_SAVE, IDI_SAVE },
		{ ID_FILE_SAVE_AS, IDI_SAVEAS },
		{ ID_FILE_PRINT, IDI_PRINT },
	};
	WTLHelper::InitMenu(menu, commands, _countof(commands));
}

BOOL CMainFrame::AddToolBarToUI(HWND hwnd) {
	return UIAddToolBar(hwnd);
}

HWND CMainFrame::GetHwnd() const {
	return m_hWnd;
}

BOOL CMainFrame::TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y) {
	InitMenu(hMenu);
	return ::TrackPopupMenu(hMenu, flags, x, y, 0, m_hWnd, nullptr);
}

CUpdateUIBase& CMainFrame::GetUI() {
	return *this;
}

LRESULT CMainFrame::OnGetMinMaxInfo(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	auto mmi = reinterpret_cast<MINMAXINFO*>(lParam);
	mmi->ptMinTrackSize.x = mmi->ptMinTrackSize.y = 600;
	return 0;
}

