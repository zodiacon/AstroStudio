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
#include "TimeZones.h"
#include "NetworkHelper.h"
#include <WTLHelper.h>

#define WINDOW_MENU_POSITION	5

namespace {
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
	//AddMenu(GetMenu());
	InitMenu(GetMenu());
	UIAddMenu(GetMenu());

	ToolBarButtonInfo buttons[] = {
		{ ID_TOOL_EPHEMERIS, IDI_EPHEMERIS },
		{ 0 },
		{ ID_NEW_CHART, IDI_CHART },
		{ ID_NEW_CHARTFORNOW, IDI_CHARTNOW },
	};
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	auto tb = ToolbarHelper::CreateAndInitToolBar(m_hWnd, buttons, _countof(buttons));
	UIAddToolBar(tb);
	AddSimpleReBarBand(tb);

	CreateSimpleStatusBar();

	m_view.m_bTabCloseButton = FALSE;
	m_hWndClient = m_view.Create(m_hWnd, rcDefault, nullptr, 
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);
	UISetCheck(ID_OPTIONS_DARKMODE, WTLHelper::IsDarkMode());

	CImageList images;
	images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 8, 4);
	UINT icons[] = {
		IDI_EPHEMERIS, IDI_CHART,
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

	PostMessage(WM_COMMAND, ID_TOOL_EPHEMERIS);

	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
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

LRESULT CMainFrame::OnNewChart(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	NewChartWithDialog();
	return 0;
}

LRESULT CMainFrame::OnNewChartNow(WORD, WORD, HWND, BOOL&) {
	auto pView = new CChartView(this);
	pView->Create(m_view, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0);
	pView->ChartForNow();
	m_view.AddPage(pView->m_hWnd, L"NewChart", 1, pView);

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

LRESULT CMainFrame::OnToggleDarkMode(WORD, WORD, HWND, BOOL&) {
	WTLHelper::SwitchToMode(WTLHelper::IsDarkMode() ? DarkModeKind::Classic : DarkModeKind::Dark, m_hWnd);
	InitMenu(GetMenu());
	DrawMenuBar();
	UISetCheck(ID_OPTIONS_DARKMODE, WTLHelper::IsDarkMode());
	return 0;
}

IView* CMainFrame::AddChartView(ChartData data, PCWSTR title) {
	auto pView = new CChartView(this);
	pView->Create(m_view, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0);
	m_view.AddPage(pView->m_hWnd, title ? title : L"Chart", 1, pView);
	pView->Chart(std::move(data));

	return pView;
}

IView* CMainFrame::NewChartWithDialog(ChartInfo const* initial) {
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
	if (dlg.DoModal(m_hWnd) != IDOK)
		return nullptr;

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
	if (nActivePage != -1)
		m_view.RemovePage(nActivePage);
	else
		::MessageBeep((UINT)-1);

	return 0;
}

LRESULT CMainFrame::OnWindowCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	m_view.RemoveAllPages();

	return 0;
}

LRESULT CMainFrame::OnWindowActivate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int nPage = wID - ID_WINDOW_TABFIRST;
	m_view.SetActivePage(nPage);

	return 0;
}

LRESULT CMainFrame::OnPageActivated(int, LPNMHDR hdr, BOOL&) {
	auto page = static_cast<int>(hdr->idFrom);
	if (m_CurrentPage >= 0 && m_CurrentPage < m_view.GetPageCount()) {
		((IView*)(CChartView*)m_view.GetPageData(m_CurrentPage))->PageActivated(false);
	}
	if (page >= 0) {
		auto view = (IView*)(CChartView*)m_view.GetPageData(page);
		ATLASSERT(view);
		view->PageActivated(true);
	}
	m_CurrentPage = page;

	return 0;
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

