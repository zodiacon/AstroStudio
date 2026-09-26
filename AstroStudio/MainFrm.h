// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Interfaces.h"
#include "ProjectView.h"
#include <CustomSplitterWindow.h>
#include <memory>
#include <atlmisc.h>
#include <NativeCustomTabView.h>
#include <TabViewHelper.h>
#include "resource.h"

// the projects opened lately, in a submenu of the Project menu (as the recent files are in the File menu's)
class CRecentProjectList : public CRecentDocumentListBase<CRecentProjectList, MAX_PATH, ID_PROJECT_MRU_FIRST, ID_PROJECT_MRU_LAST> {
};

class CMainFrame :
	public CFrameWindowImpl<CMainFrame>,
	public CAutoUpdateUI<CMainFrame>,
	public IMainFrame,
	public IProjectHost,
	public CMessageFilter, 
	public CIdleHandler {
public:
	DECLARE_FRAME_WND_CLASS(L"AstroStudioMainWindow", IDR_MAINFRAME)

	BOOL PreTranslateMessage(MSG* pMsg) override;
	BOOL OnIdle() override;

protected:
	BEGIN_MSG_MAP(CMainFrame)
		NOTIFY_CODE_HANDLER(TBVN_PAGEACTIVATED, OnPageActivated)
		COMMAND_TABVIEW_HANDLER(m_Tabs, 1)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, OnViewStatusBar)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER(ID_WINDOW_CLOSE, OnWindowClose)
		COMMAND_ID_HANDLER(ID_TOOL_EPHEMERIS, OnToolEphemeris)
		COMMAND_ID_HANDLER(ID_TOOL_ANALYSIS, OnToolAnalysis)
		COMMAND_ID_HANDLER(ID_NEW_CHART, OnNewChart)
		COMMAND_ID_HANDLER(ID_NEW_CHARTFORNOW, OnNewChartNow)
		COMMAND_ID_HANDLER(ID_FILE_OPEN, OnFileOpen)
		COMMAND_ID_HANDLER(ID_FILE_PRINT_SETUP, OnPrintSetup)
		COMMAND_RANGE_HANDLER(ID_FILE_MRU_FIRST, ID_FILE_MRU_LAST, OnFileRecent)
		COMMAND_RANGE_HANDLER(ID_PROJECT_NEW, ID_PROJECT_AUTOOPEN, OnProjectCommand)
		COMMAND_RANGE_HANDLER(ID_PROJECT_MRU_FIRST, ID_PROJECT_MRU_LAST, OnProjectRecent)
		MESSAGE_HANDLER(WM_OPEN_LAST_PROJECT, OnOpenLastProject)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		COMMAND_ID_HANDLER(ID_OPTIONS_DARKMODE, OnToggleDarkMode)
		COMMAND_ID_HANDLER(ID_OPTIONS_ASPECTS, OnAspectOptions)
		COMMAND_ID_HANDLER(ID_OPTIONS_WHEEL, OnWheelOptions)
		COMMAND_ID_HANDLER(ID_OPTIONS_MIDPOINTS, OnMidpointOptions)
		COMMAND_ID_HANDLER(ID_OPTIONS_FORTUNE, OnPartOfFortune)
		COMMAND_ID_HANDLER(ID_OPTIONS_COLORS, OnChartColors)
		COMMAND_ID_HANDLER(ID_OPTIONS_ALWAYSONTOP, OnAlwaysOnTop)
		COMMAND_ID_HANDLER(ID_OPTIONS_FONT, OnFont)
		MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
		MESSAGE_HANDLER(WM_LOCATION_READY, OnLocationReady)
		COMMAND_ID_HANDLER(ID_WINDOW_CLOSE_ALL, OnWindowCloseAll)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_RANGE_HANDLER(ID_WINDOW_TABFIRST, ID_WINDOW_TABLAST, OnWindowActivate)
		CHAIN_MSG_MAP(CAutoUpdateUI)
		CHAIN_MSG_MAP(CFrameWindowImpl)
	END_MSG_MAP()

private:
	void InitMenu(HMENU menu);
	// The view behind a tab, or null. (The tab view keeps each page's data as a message map; asking it for
	// an IView with dynamic_cast is safe for every kind of page.)
	IView* ViewOfPage(int page) const;
	int PageOfView(IView* view) const;
	// shows a tab and tells the pages, which SetActivePage alone doesn't
	void ActivatePage(int page);
	// asks every view whether it can close (they may ask the user to save)
	bool CanCloseAll();
	// opens a file of any kind the program has: a chart, an analysis (in a tab) or a project (in the pane)
	bool OpenChartFile(PCWSTR path);
	// keeps the menu and the saved list in step
	void RecentFilesChanged();
	BOOL AddToolBarToUI(HWND) override;

	// Inherited via IMainFrame
	HWND GetHwnd() const override;
	BOOL TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y) override;
	CUpdateUIBase& GetUI() override;
	IView* AddChartView(ChartData data, PCWSTR title = nullptr, PCWSTR filePath = nullptr) override;
	void AddRecentFile(PCWSTR path) override;
	void SetViewTitle(IView* view, PCWSTR title) override;
	void ActivateView(IView* view) override;
	ChartInfo& DefaultChartInfo() override;
	IView* NewChartWithDialog(ChartInfo const* initial = nullptr, HouseSystem const* houseSystem = nullptr) override;
	IView* AddDerivedChartView(ChartData data, PCWSTR title, DerivedRecipe const* recipe = nullptr, PCWSTR filePath = nullptr) override;
	bool IsLocationPending() const override;
	std::vector<OpenChart> OpenCharts(IView* except = nullptr) override;
	void NewAnalysis(IView* chart = nullptr) override;

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnGetMinMaxInfo(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToolEphemeris(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToolAnalysis(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWindowClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWindowCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWindowActivate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPageActivated(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnFileRecent(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPrintSetup(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnNewChart(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNewChartNow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToggleDarkMode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAspectOptions(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWheelOptions(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnMidpointOptions(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPartOfFortune(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnChartColors(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAlwaysOnTop(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLocationReady(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	// ---- projects (MainFrmProject.cpp): one at a time, shown in the pane on the left of the tabs
	static constexpr UINT WM_OPEN_LAST_PROJECT = WM_APP + 31;
	LRESULT OnProjectCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnProjectRecent(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOpenLastProject(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	// runs a command of the Project menu (or of the tree's menu) on a node of the tree
	void ProjectCommand(UINT id, ProjectNode const& node);
	void NewProject();
	void OpenProjectDialog();
	// Opens a project, closing the one that is open (which may ask to save it); false if that was cancelled or the file is no good.
	bool OpenProject(PCWSTR path);
	// Closes the project: asks whether to save what changed in it, remembers what is open, and hides the pane. False if cancelled.
	// leaving: the program is closing, so the project is the one to open again at the next start.
	bool CloseProject(bool leaving = false);
	bool SaveProject();
	bool SaveProjectAs();
	void AddCurrentTab(ProjectNode const& node);
	void AddFilesDialog(ProjectNode const& node);
	void AddFiles(std::vector<std::filesystem::path> const& files, ProjectNode const& target);
	void NewGroup(ProjectNode const& node);
	void ShowProjectProperties(ProjectNode const& node);
	void LocateItem(std::wstring const& id);
	void RemoveNode(ProjectNode const& node);
	// puts the tabs that are open into the project's session, and opens the tabs of the session
	void CaptureSession();
	void RestoreSession();
	// the tab that shows an item's file, or null
	IView* ViewForItem(ProjectItem const& item) const;
	void ShowProjectPane(bool show);
	// what the Project menu allows, the window's title and the marks in the pane
	void UpdateProjectUI();
	void ProjectListChanged();
	static std::wstring GroupOf(Project const& project, ProjectNode const& node);

	// IProjectHost
	void ProjectOpenItem(std::wstring const& id) override;
	void ProjectContextMenu(ProjectNode const& node, CPoint screen) override;
	void ProjectRename(ProjectNode const& node, std::wstring const& text) override;
	void ProjectDrop(ProjectNode const& moved, ProjectNode const& target) override;
	void ProjectAddFiles(std::vector<std::filesystem::path> const& files, ProjectNode const& target) override;
	void ProjectKey(UINT key, ProjectNode const& node) override;
	bool ProjectItemOpen(std::wstring const& id) const override;
	bool ProjectItemModified(std::wstring const& id) const override;

	CCustomSplitterWindow m_Splitter;
	CProjectView m_ProjectView;
	std::unique_ptr<Project> m_Project;
	CRecentProjectList m_RecentProjects;
	bool m_PaneVisible{ false };
	// ---- the status bar's panes: the active tab's name, moment, place and details (IView::GetStatusInfo), refreshed by a timer
	static constexpr UINT_PTR StatusTimer = 1;
	LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
	void UpdateStatusPanes();
	void SetStatusPane(int id, CString const& text, int minWidth);
	CMultiPaneStatusBarCtrl m_Status;
	CString m_PaneText[4];

	CNativeCustomTabView m_Tabs;
	ChartInfo m_DefaultChartInfo{};
	CRecentDocumentList m_Recent;
	int m_CurrentPage{ -1 };
	bool m_LocationPending{ false };
};
