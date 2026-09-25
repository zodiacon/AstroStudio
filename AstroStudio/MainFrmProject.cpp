// The part of the main frame that has to do with projects: the Project menu, the pane on the left, and keeping the project and the tabs
// in step.

#include "pch.h"
#include "resource.h"
#include "MainFrm.h"
#include "Project.h"
#include "ProjectItemDlg.h"
#include "AppSettings.h"
#include "AspectOptions.h"
#include "ChartColors.h"
#include "Helpers.h"
#include <WTLHelper.h>
#include <atldlgs.h>
#include <filesystem>
#include <shellapi.h>

namespace fs = std::filesystem;

namespace {
	constexpr PCWSTR RecentProjectsKey = LR"(Software\AstroStudio\Projects)";
	constexpr int MaxRecentProjects = 8;
	constexpr wchar_t AddFilter[] = L"Charts, analyses and settings (*.chart;*.analysis;*.aspects;*.colors)\0*.chart;*.analysis;*.aspects;*.colors\0All files (*.*)\0*.*\0";

	bool SamePath(fs::path const& a, fs::path const& b) {
		std::error_code error;
		return _wcsicmp(fs::absolute(a, error).lexically_normal().c_str(), fs::absolute(b, error).lexically_normal().c_str()) == 0;
	}

	bool Linkable(fs::path const& path) {
		return Project::TypeOfFile(path) != ProjectItemType::Other;
	}

	std::wstring ParentOf(std::wstring const& group) {
		auto slash = group.rfind(L'/');
		return slash == std::wstring::npos ? std::wstring() : group.substr(0, slash);
	}

	std::wstring LastPart(std::wstring const& group) {
		auto slash = group.rfind(L'/');
		return slash == std::wstring::npos ? group : group.substr(slash + 1);
	}

	int Ask(HWND owner, CString const& message, UINT type = MB_YESNO | MB_ICONQUESTION) {
		return AtlMessageBox(owner, (PCWSTR)message, L"Astro Studio", type);
	}
}

//
// the pane, the menu and the window's title
//

void CMainFrame::ShowProjectPane(bool show) {
	if (m_PaneVisible && !show)
		AppSettings::Get().ProjectPaneWidth(m_Splitter.GetSplitterPos());
	m_PaneVisible = show;
	m_Splitter.SetSinglePaneMode(show ? SPLIT_PANE_NONE : SPLIT_PANE_RIGHT);
	if (show)
		m_Splitter.SetSplitterPos(std::max(120, AppSettings::Get().ProjectPaneWidth()));
	UpdateProjectUI();
}

void CMainFrame::UpdateProjectUI() {
	if (!m_ProjectView.m_hWnd)
		return;		// (the window is still being made)
	bool open = m_Project != nullptr;
	for (UINT id : { ID_PROJECT_SAVE, ID_PROJECT_SAVEAS, ID_PROJECT_CLOSE, ID_PROJECT_ADDCURRENT, ID_PROJECT_ADDFILES, ID_PROJECT_NEWGROUP, ID_PROJECT_PANE })
		UIEnable(id, open);
	UISetCheck(ID_PROJECT_PANE, open && m_PaneVisible);
	UIEnable(ID_PROJECT_MRU_FIRST, m_RecentProjects.m_arrDocs.GetSize() > 0);

	// the project's name comes before the program's
	CString title = L"Astro Studio";
	if (open) {
		title = CString(m_Project->Name().c_str()) + (m_Project->Dirty() ? L" *" : L"") + L" - Astro Studio";
		m_ProjectView.UpdateStates();
	}
	CString current;
	GetWindowText(current);
	if (current != title)
		SetWindowText(title);
}

void CMainFrame::ProjectListChanged() {
	m_ProjectView.Refresh();
	UpdateProjectUI();
}

std::wstring CMainFrame::GroupOf(Project const& project, ProjectNode const& node) {
	if (node.Type == ProjectNode::Kind::Group)
		return node.Key;
	if (node.Type == ProjectNode::Kind::Item)
		if (auto item = project.Find(node.Key))
			return item->Group;
	return {};
}

//
// commands
//

LRESULT CMainFrame::OnProjectCommand(WORD, WORD id, HWND, BOOL&) {
	ProjectCommand(id, m_ProjectView.Selected());
	return 0;
}

void CMainFrame::ProjectCommand(UINT id, ProjectNode const& node) {
	switch (id) {
		case ID_PROJECT_NEW: NewProject(); return;
		case ID_PROJECT_OPEN: OpenProjectDialog(); return;
		case ID_PROJECT_SAVE: SaveProject(); return;
		case ID_PROJECT_SAVEAS: SaveProjectAs(); return;
		case ID_PROJECT_CLOSE: CloseProject(); return;
		case ID_PROJECT_AUTOOPEN: {
			bool on = AppSettings::Get().OpenLastProject() == 0;
			AppSettings::Get().OpenLastProject(on ? 1 : 0);
			UISetCheck(ID_PROJECT_AUTOOPEN, on);
			return;
		}
	}
	if (!m_Project)
		return;
	switch (id) {
		case ID_PROJECT_PANE:
			ShowProjectPane(!m_PaneVisible);
			break;
		case ID_PROJECT_ADDCURRENT:
			AddCurrentTab(node);
			break;
		case ID_PROJECT_ADDFILES:
			AddFilesDialog(node);
			break;
		case ID_PROJECT_NEWGROUP:
			NewGroup(node);
			break;
		case ID_PROJECT_OPENITEM:
			if (node.Type == ProjectNode::Kind::Item)
				ProjectOpenItem(node.Key);
			break;
		case ID_PROJECT_RENAME:
			if (node.Type != ProjectNode::Kind::None)
				m_ProjectView.BeginRename(node);
			break;
		case ID_PROJECT_PROPERTIES:
			ShowProjectProperties(node);
			break;
		case ID_PROJECT_LOCATE:
			if (node.Type == ProjectNode::Kind::Item)
				LocateItem(node.Key);
			break;
		case ID_PROJECT_REMOVE:
		case ID_PROJECT_REMOVEGROUP:
			RemoveNode(node);
			break;
		case ID_PROJECT_EXPLORER:
			if (node.Type == ProjectNode::Kind::Item)
				if (auto item = m_Project->Find(node.Key)) {
					std::wstring parameters = L"/select,\"" + m_Project->Resolve(*item).wstring() + L"\"";
					::ShellExecuteW(m_hWnd, L"open", L"explorer.exe", parameters.c_str(), nullptr, SW_SHOWNORMAL);
				}
			break;
		case ID_PROJECT_REFRESH:
			m_Project->Refresh();
			ProjectListChanged();
			break;
	}
}

//
// the project as a whole
//

void CMainFrame::OpenProjectDialog() {
	CSimpleFileDialog dlg(TRUE, Project::Extension, nullptr, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ENABLESIZING, Project::Filter, m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (ok)
		OpenProject(dlg.m_szFileName);
}

LRESULT CMainFrame::OnProjectRecent(WORD, WORD id, HWND, BOOL&) {
	CString path;		// (the CString overload: the one with a plain buffer is deprecated and does nothing)
	if (!m_RecentProjects.GetFromList(id, path))
		return 0;
	std::error_code error;
	if (!fs::exists((PCWSTR)path, error)) {
		CString message;
		message.Format(L"%s does not exist any more.", (PCWSTR)path);
		Ask(m_hWnd, message, MB_OK | MB_ICONWARNING);
		m_RecentProjects.RemoveFromList(id);
		m_RecentProjects.WriteToRegistry(RecentProjectsKey);
		UpdateProjectUI();
		return 0;
	}
	OpenProject(path);
	return 0;
}

LRESULT CMainFrame::OnOpenLastProject(UINT, WPARAM, LPARAM, BOOL&) {
	auto path = AppSettings::Get().LastProject();
	std::error_code error;
	if (!path.empty() && !m_Project && fs::exists(path, error))
		OpenProject(path.c_str());
	return 0;
}

static void RememberProject(CRecentProjectList& list, PCWSTR path) {
	list.AddToList(path);
	list.WriteToRegistry(RecentProjectsKey);
	AppSettings::Get().LastProject(path);
}

void CMainFrame::NewProject() {
	// a name and, if wanted, a description: where the project is kept is asked when it is first saved
	CProjectItemDlg dlg;
	dlg.InitProject(L"New Project", L"", L"", L"New Project");
	if (dlg.DoModal(m_hWnd) != IDOK || !CloseProject())
		return;

	auto project = std::make_unique<Project>();
	auto name = dlg.Name();
	while (!name.empty() && iswspace(name.back()))
		name.pop_back();
	project->Name(name.empty() ? L"New Project" : name);
	project->Description(dlg.Notes());
	m_Project = std::move(project);
	m_ProjectView.SetProject(m_Project.get());
	ShowProjectPane(true);

	// the charts that are open (from files) are the natural start
	std::vector<fs::path> open;
	for (int i = 0; i < m_view.GetPageCount(); i++)
		if (auto view = ViewOfPage(i); view && view->FilePath() && Linkable(view->FilePath()))
			open.emplace_back(view->FilePath());
	if (!open.empty()) {
		CString message;
		message.Format(L"Add the %d open file%s to the project?", static_cast<int>(open.size()), open.size() == 1 ? L"" : L"s");
		if (Ask(m_hWnd, message) == IDYES)
			AddFiles(open, {});
	}
	ProjectListChanged();
}

bool CMainFrame::OpenProject(PCWSTR path) {
	std::error_code error;
	auto full = fs::absolute(path, error).lexically_normal();
	if (m_Project && SamePath(m_Project->FilePath(), full)) {
		ShowProjectPane(true);
		return true;
	}

	// (read before the open one is closed, so that a project that can't be read costs nothing)
	auto project = std::make_unique<Project>();
	std::wstring problem;
	if (!project->Load(full, problem)) {
		CString message;
		message.Format(L"%s could not be opened:\n\n%s", full.c_str(), problem.c_str());
		Ask(m_hWnd, message, MB_OK | MB_ICONWARNING);
		return false;
	}
	if (!CloseProject())
		return false;
	m_Project = std::move(project);
	RememberProject(m_RecentProjects, full.c_str());
	m_ProjectView.SetProject(m_Project.get());
	ShowProjectPane(true);
	RestoreSession();
	ProjectListChanged();
	return true;
}

bool CMainFrame::CloseProject(bool leaving) {
	if (!m_Project)
		return true;
	bool discard = false;
	if (m_Project->Dirty()) {
		CString message;
		message.Format(L"Save the changes to the project %s?", m_Project->Name().c_str());
		switch (Ask(m_hWnd, message, MB_YESNOCANCEL | MB_ICONQUESTION)) {
			case IDCANCEL:
				return false;
			case IDYES:
				if (!SaveProject())
					return false;
				break;
			default:
				discard = true;
				break;
		}
	}
	if (!discard) {
		// what is open is kept with the project without asking
		CaptureSession();
		if (m_Project->NeedsSave() && !m_Project->FilePath().empty()) {
			std::wstring error;
			m_Project->Save(error);
		}
	}
	if (!leaving)
		AppSettings::Get().LastProject(std::wstring());
	if (m_PaneVisible)
		AppSettings::Get().ProjectPaneWidth(m_Splitter.GetSplitterPos());
	m_Project.reset();
	m_ProjectView.SetProject(nullptr);
	ShowProjectPane(false);
	return true;
}

bool CMainFrame::SaveProject() {
	if (!m_Project)
		return false;
	if (m_Project->FilePath().empty())
		return SaveProjectAs();
	CaptureSession();
	std::wstring error;
	if (!m_Project->Save(error)) {
		CString message;
		message.Format(L"The project could not be saved:\n\n%s", error.c_str());
		Ask(m_hWnd, message, MB_OK | MB_ICONWARNING);
		return false;
	}
	UpdateProjectUI();
	return true;
}

bool CMainFrame::SaveProjectAs() {
	if (!m_Project)
		return false;
	CSimpleFileDialog dlg(FALSE, Project::Extension, CString(m_Project->Name().c_str()), OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING,
		Project::Filter, m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (!ok)
		return false;
	CaptureSession();
	std::wstring error;
	if (!m_Project->SaveAs(dlg.m_szFileName, error)) {
		CString message;
		message.Format(L"The project could not be saved to %s:\n\n%s", dlg.m_szFileName, error.c_str());
		Ask(m_hWnd, message, MB_OK | MB_ICONWARNING);
		return false;
	}
	RememberProject(m_RecentProjects, dlg.m_szFileName);
	UpdateProjectUI();
	return true;
}

//
// what is open
//

IView* CMainFrame::ViewForItem(ProjectItem const& item) const {
	if (!m_Project)
		return nullptr;
	auto path = m_Project->Resolve(item);
	for (int i = 0; i < m_view.GetPageCount(); i++)
		if (auto view = ViewOfPage(i); view && view->FilePath() && SamePath(view->FilePath(), path))
			return view;
	return nullptr;
}

void CMainFrame::CaptureSession() {
	if (!m_Project)
		return;
	ProjectSession session;
	int active = m_view.GetActivePage();
	for (int i = 0; i < m_view.GetPageCount(); i++) {
		auto view = ViewOfPage(i);
		if (!view || !view->FilePath())
			continue;
		if (auto item = m_Project->FindByPath(view->FilePath())) {
			session.Open.push_back(item->Id);
			if (i == active)
				session.Active = item->Id;
		}
	}
	m_Project->Session(std::move(session));
}

void CMainFrame::RestoreSession() {
	if (!m_Project)
		return;
	auto session = m_Project->Session();
	for (auto const& id : session.Open)
		if (auto item = m_Project->Find(id); item && !item->Missing &&
			(item->Type == ProjectItemType::Chart || item->Type == ProjectItemType::Analysis))
			OpenChartFile(m_Project->Resolve(*item).c_str());
	if (auto item = m_Project->Find(session.Active))
		if (auto view = ViewForItem(*item))
			ActivateView(view);
}

bool CMainFrame::ProjectItemOpen(std::wstring const& id) const {
	auto item = m_Project ? m_Project->Find(id) : nullptr;
	return item && ViewForItem(*item) != nullptr;
}

bool CMainFrame::ProjectItemModified(std::wstring const& id) const {
	auto item = m_Project ? m_Project->Find(id) : nullptr;
	auto view = item ? ViewForItem(*item) : nullptr;
	return view && view->IsModified();
}

//
// the items
//

void CMainFrame::ProjectOpenItem(std::wstring const& id) {
	auto item = m_Project ? m_Project->Find(id) : nullptr;
	if (!item)
		return;
	auto path = m_Project->Resolve(*item);
	std::error_code error;
	if (!fs::exists(path, error)) {
		CString message;
		message.Format(L"%s could not be found:\n\n%s\n\nLook for it?", item->Name.c_str(), path.c_str());
		if (Ask(m_hWnd, message) != IDYES)
			return;
		LocateItem(id);
		item = m_Project->Find(id);
		if (!item || item->Missing)
			return;
		path = m_Project->Resolve(*item);
	}
	else if (item->Missing) {
		m_Project->Refresh();		// (it was, but is there again)
		ProjectListChanged();
	}

	switch (item->Type) {
		case ProjectItemType::Chart:
		case ProjectItemType::Analysis:
			OpenChartFile(path.c_str());
			break;
		case ProjectItemType::AspectSet: {
			CString message;
			message.Format(L"Use the aspect settings in %s?", item->Name.c_str());
			if (Ask(m_hWnd, message) != IDYES)
				break;
			AspectOptions loaded;
			std::wstring problem;
			if (!loaded.Load(path.c_str(), problem)) {
				message.Format(L"%s could not be read:\n\n%s", path.c_str(), problem.c_str());
				Ask(m_hWnd, message, MB_OK | MB_ICONWARNING);
				break;
			}
			AspectOptions::Current() = loaded;
			AspectOptions::StoreInSettings();
			for (int i = 0; i < m_view.GetPageCount(); i++)
				if (auto view = ViewOfPage(i))
					view->AspectSettingsChanged();
			break;
		}
		case ProjectItemType::ColorSet: {
			CString message;
			message.Format(L"Use the colours in %s for the chart wheel?", item->Name.c_str());
			if (Ask(m_hWnd, message) != IDYES)
				break;
			auto loaded = ChartColors::Current();
			std::wstring problem;
			if (!loaded.Load(path.c_str(), problem)) {
				message.Format(L"%s could not be read:\n\n%s", path.c_str(), problem.c_str());
				Ask(m_hWnd, message, MB_OK | MB_ICONWARNING);
				break;
			}
			ChartColors::Current() = loaded;
			ChartColors::StoreInSettings();
			for (int i = 0; i < m_view.GetPageCount(); i++)
				if (auto view = ViewOfPage(i))
					view->WheelOptionsChanged();
			break;
		}
		default:
			::ShellExecuteW(m_hWnd, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
			break;
	}
	UpdateProjectUI();
}

void CMainFrame::AddCurrentTab(ProjectNode const& node) {
	int page = m_view.GetActivePage();
	auto view = ViewOfPage(page);
	if (!view) {
		Ask(m_hWnd, L"There is no tab to add.", MB_OK | MB_ICONINFORMATION);
		return;
	}
	if (!view->FilePath())
		view->ProcessCommand(ID_FILE_SAVE);		// (a chart that has never been saved has no file to link: this asks where to put it)
	auto path = view->FilePath();
	if (!path || !Linkable(path)) {
		if (!path)
			Ask(m_hWnd, L"This tab has no file to link to the project.", MB_OK | MB_ICONINFORMATION);
		return;
	}
	auto item = m_Project->Add(path, GroupOf(*m_Project, node));
	ProjectListChanged();
	if (item)
		m_ProjectView.Select({ ProjectNode::Kind::Item, item->Id });
}

void CMainFrame::AddFilesDialog(ProjectNode const& node) {
	CMultiFileDialog dlg(nullptr, nullptr, OFN_EXPLORER | OFN_ENABLESIZING | OFN_FILEMUSTEXIST, AddFilter, m_hWnd);
	dlg.m_ofn.lpstrTitle = L"Add Files to the Project";
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (!ok)
		return;
	std::vector<fs::path> files;
	WCHAR path[MAX_PATH]{};
	if (dlg.GetFirstPathName(path, MAX_PATH)) {
		do {
			files.emplace_back(path);
		} while (dlg.GetNextPathName(path, MAX_PATH));
	}
	AddFiles(files, node);
}

void CMainFrame::AddFiles(std::vector<fs::path> const& files, ProjectNode const& target) {
	if (!m_Project)
		return;
	// a project file dropped on the pane is opened, not linked
	if (files.size() == 1 && _wcsicmp(files[0].extension().c_str(), (std::wstring(L".") + Project::Extension).c_str()) == 0) {
		OpenProject(files[0].c_str());
		return;
	}
	auto group = GroupOf(*m_Project, target);
	int skipped = 0;
	ProjectItem const* last = nullptr;
	for (auto const& file : files) {
		if (!Linkable(file)) {
			skipped++;
			continue;
		}
		last = m_Project->Add(file, group);
	}
	if (skipped) {
		CString message;
		message.Format(L"%d file%s not a chart, an analysis or a settings file and %s not added.", skipped, skipped == 1 ? L" is" : L"s are", skipped == 1 ? L"was" : L"were");
		Ask(m_hWnd, message, MB_OK | MB_ICONINFORMATION);
	}
	ProjectListChanged();
	if (last)
		m_ProjectView.Select({ ProjectNode::Kind::Item, last->Id });
}

void CMainFrame::NewGroup(ProjectNode const& node) {
	auto base = GroupOf(*m_Project, node);
	std::wstring name;
	for (int n = 1; ; n++) {
		name = (base.empty() ? L"" : base + L"/") + L"New Group" + (n > 1 ? L" " + std::to_wstring(n) : L"");
		auto const& groups = m_Project->Groups();
		if (std::ranges::none_of(groups, [&](auto const& g) { return _wcsicmp(g.c_str(), name.c_str()) == 0; }))
			break;
	}
	m_Project->AddGroup(name);
	ProjectListChanged();
	m_ProjectView.BeginRename({ ProjectNode::Kind::Group, name });
}

void CMainFrame::ShowProjectProperties(ProjectNode const& node) {
	CProjectItemDlg dlg;
	if (node.Type == ProjectNode::Kind::Item) {
		auto item = m_Project->Find(node.Key);
		if (!item)
			return;
		dlg.InitItem(item->Name, item->Tags, item->Notes, m_Project->Resolve(*item).wstring());
		if (dlg.DoModal(m_hWnd) != IDOK)
			return;
		m_Project->Rename(node.Key, dlg.Name());
		m_Project->SetTags(node.Key, dlg.Tags());
		m_Project->SetNotes(node.Key, dlg.Notes());
	}
	else if (node.Type == ProjectNode::Kind::Root || node.Type == ProjectNode::Kind::None) {
		dlg.InitProject(m_Project->Name(), m_Project->Description(), m_Project->FilePath().wstring());
		if (dlg.DoModal(m_hWnd) != IDOK)
			return;
		if (!dlg.Name().empty())
			m_Project->Name(dlg.Name());
		m_Project->Description(dlg.Notes());
	}
	else
		return;
	ProjectListChanged();
}

void CMainFrame::LocateItem(std::wstring const& id) {
	auto item = m_Project ? m_Project->Find(id) : nullptr;
	if (!item)
		return;
	// first where the project and the other files are, then the user says
	std::vector<fs::path> folders{ m_Project->FilePath().parent_path() };
	for (auto const& other : m_Project->Items())
		if (!other.Missing) {
			auto folder = m_Project->Resolve(other).parent_path();
			if (std::ranges::none_of(folders, [&](auto const& f) { return SamePath(f, folder); }))
				folders.push_back(folder);
		}
	if (m_Project->Locate(id, folders)) {
		ProjectListChanged();
		return;
	}
	CSimpleFileDialog dlg(TRUE, nullptr, CString(m_Project->Resolve(*item).filename().c_str()), OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ENABLESIZING,
		AddFilter, m_hWnd);
	dlg.m_ofn.lpstrTitle = L"Locate the File";
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (ok)
		m_Project->Relink(id, dlg.m_szFileName);
	ProjectListChanged();
}

void CMainFrame::RemoveNode(ProjectNode const& node) {
	if (node.Type == ProjectNode::Kind::Item) {
		auto item = m_Project->Find(node.Key);
		if (!item)
			return;
		CString message;
		message.Format(L"Take %s out of the project?\n\nThe file is not deleted.", item->Name.c_str());
		if (Ask(m_hWnd, message) != IDYES)
			return;
		m_Project->Remove(node.Key);
	}
	else if (node.Type == ProjectNode::Kind::Group) {
		CString message;
		message.Format(L"Remove the group %s from the project?\n\nWhat is in it moves up to the group above.", LastPart(node.Key).c_str());
		if (Ask(m_hWnd, message) != IDYES)
			return;
		m_Project->RemoveGroup(node.Key);
	}
	else
		return;
	ProjectListChanged();
}

//
// IProjectHost: what the tree asks
//

void CMainFrame::ProjectContextMenu(ProjectNode const& node, CPoint screen) {
	if (!m_Project)
		return;
	CMenu menu;
	menu.CreatePopupMenu();
	auto add = [&](UINT id, PCWSTR text) { menu.AppendMenu(MF_STRING, id, text); };
	auto separator = [&] { menu.AppendMenu(MF_SEPARATOR); };
	switch (node.Type) {
		case ProjectNode::Kind::Item: {
			add(ID_PROJECT_OPENITEM, L"&Open");
			add(ID_PROJECT_RENAME, L"&Rename\tF2");
			add(ID_PROJECT_PROPERTIES, L"&Properties...");
			separator();
			if (auto item = m_Project->Find(node.Key); item && item->Missing)
				add(ID_PROJECT_LOCATE, L"&Locate...");
			add(ID_PROJECT_EXPLORER, L"Show in &Explorer");
			add(ID_PROJECT_REMOVE, L"Re&move from Project\tDel");
			menu.SetMenuDefaultItem(ID_PROJECT_OPENITEM);
			break;
		}
		case ProjectNode::Kind::Group:
			add(ID_PROJECT_NEWGROUP, L"New &Group");
			add(ID_PROJECT_ADDCURRENT, L"Add &Current Tab");
			add(ID_PROJECT_ADDFILES, L"Add &Files...");
			separator();
			add(ID_PROJECT_RENAME, L"&Rename\tF2");
			add(ID_PROJECT_REMOVEGROUP, L"Re&move Group\tDel");
			break;
		default:
			add(ID_PROJECT_ADDCURRENT, L"Add &Current Tab");
			add(ID_PROJECT_ADDFILES, L"Add &Files...");
			add(ID_PROJECT_NEWGROUP, L"New &Group");
			separator();
			add(ID_PROJECT_PROPERTIES, L"&Properties...");
			add(ID_PROJECT_REFRESH, L"Look for the &Files Again\tF5");
			separator();
			add(ID_PROJECT_SAVE, L"&Save Project");
			add(ID_PROJECT_CLOSE, L"C&lose Project");
			break;
	}
	InitMenu(menu);
	UINT command = static_cast<UINT>(::TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen.x, screen.y, 0, m_hWnd, nullptr));
	if (command)
		ProjectCommand(command, node);
}

void CMainFrame::ProjectRename(ProjectNode const& node, std::wstring const& text) {
	if (!m_Project)
		return;
	if (node.Type == ProjectNode::Kind::Root) {
		if (!Project::NormalizeGroup(text).empty())
			m_Project->Name(text);
	}
	else if (node.Type == ProjectNode::Kind::Item) {
		m_Project->Rename(node.Key, text);
	}
	else if (node.Type == ProjectNode::Kind::Group) {
		// (a name is a name, not a path)
		std::wstring name = text;
		std::ranges::replace(name, L'/', L'-');
		std::ranges::replace(name, L'\\', L'-');
		name = Project::NormalizeGroup(name);
		if (!name.empty()) {
			auto parent = ParentOf(node.Key);
			if (!m_Project->RenameGroup(node.Key, (parent.empty() ? L"" : parent + L"/") + name))
				Ask(m_hWnd, L"A group of that name is already there.", MB_OK | MB_ICONINFORMATION);
		}
	}
	ProjectListChanged();
}

void CMainFrame::ProjectDrop(ProjectNode const& moved, ProjectNode const& target) {
	if (!m_Project)
		return;
	if (moved.Type == ProjectNode::Kind::Item) {
		if (target.Type == ProjectNode::Kind::Item) {
			// before the item it was dropped on, in that item's group
			if (auto other = m_Project->Find(target.Key))
				m_Project->Move(moved.Key, other->Group, other->Id);
		}
		else
			m_Project->Move(moved.Key, target.Type == ProjectNode::Kind::Group ? target.Key : std::wstring(), {});
	}
	else if (moved.Type == ProjectNode::Kind::Group) {
		auto destination = target.Type == ProjectNode::Kind::Group ? target.Key : std::wstring();
		auto name = LastPart(moved.Key);
		if (!m_Project->RenameGroup(moved.Key, (destination.empty() ? L"" : destination + L"/") + name))
			Ask(m_hWnd, L"A group of that name is already there.", MB_OK | MB_ICONINFORMATION);
	}
	ProjectListChanged();
}

void CMainFrame::ProjectAddFiles(std::vector<fs::path> const& files, ProjectNode const& target) {
	AddFiles(files, target);
}

void CMainFrame::ProjectKey(UINT key, ProjectNode const& node) {
	if (!m_Project)
		return;
	if (key == VK_F2 && node.Type != ProjectNode::Kind::None)
		m_ProjectView.BeginRename(node);
	else if (key == VK_DELETE)
		RemoveNode(node);
	else if (key == VK_F5)
		ProjectCommand(ID_PROJECT_REFRESH, node);
}
