#pragma once

#include "Project.h"
#include <filesystem>
#include <set>
#include <vector>

// What a node of the project tree stands for.
struct ProjectNode {
	enum class Kind { None, Root, Group, Item };
	Kind Type{ Kind::None };
	std::wstring Key;		// the group's path, or the item's id (empty for the root)
};

// What the project tree asks of the program that owns the project.
struct IProjectHost {
	// an item was double clicked (or Enter was pressed on it)
	virtual void ProjectOpenItem(std::wstring const& id) = 0;
	// the right button was pressed on a node (or on nothing: Kind::None); the screen position for the menu
	virtual void ProjectContextMenu(ProjectNode const& node, CPoint screen) = 0;
	// the user edited a node's name in place; the host changes the project (the tree is rebuilt afterwards either way)
	virtual void ProjectRename(ProjectNode const& node, std::wstring const& text) = 0;
	// a node was dragged onto another (the root counts as a target)
	virtual void ProjectDrop(ProjectNode const& moved, ProjectNode const& target) = 0;
	// files were dropped on the tree, on a node or on nothing
	virtual void ProjectAddFiles(std::vector<std::filesystem::path> const& files, ProjectNode const& target) = 0;
	// a key was pressed on the selected node: VK_F2 (rename), VK_DELETE, VK_F5 (look at the files again)
	virtual void ProjectKey(UINT key, ProjectNode const& node) = 0;
	// for the look of an item: is a tab showing its file, has that changed and not been saved
	virtual bool ProjectItemOpen(std::wstring const& id) const = 0;
	virtual bool ProjectItemModified(std::wstring const& id) const = 0;
};

// The pane on the left of the window with the open project: a tree of the project (its name), the groups and the items in them, in the
// order the project has them. It shows what the project says and reports what the user does; the project itself is changed by the host,
// which then calls Refresh.
class CProjectView : public CWindowImpl<CProjectView> {
public:
	DECLARE_WND_CLASS_EX(L"AstroStudioProjectView", CS_DBLCLKS, COLOR_WINDOW)

	void Init(IProjectHost* host) {
		m_Host = host;
	}
	// null: an empty pane
	void SetProject(Project const* project);
	// Builds the tree again from the project, keeping which groups were open and what was selected.
	void Refresh();
	// Brings the names (marks for unsaved changes, missing files) and the bold of the open items up to date without rebuilding.
	void UpdateStates();
	ProjectNode Selected() const;
	void Select(ProjectNode const& node);
	// puts the node's name in an edit box; what is typed comes back through IProjectHost::ProjectRename
	void BeginRename(ProjectNode const& node);
	// puts the text font the user chose with Options > Font on the tree (nothing if they have not chosen one)
	void ApplyTextFont();

	BEGIN_MSG_MAP(CProjectView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
		MESSAGE_HANDLER(WM_CAPTURECHANGED, OnCaptureChanged)
		MESSAGE_HANDLER(WM_DROPFILES, OnDropFiles)
		MESSAGE_HANDLER(WM_REFRESH, OnRefresh)
		NOTIFY_HANDLER(IDC_PROJECT_TREE, NM_DBLCLK, OnDoubleClick)
		NOTIFY_HANDLER(IDC_PROJECT_TREE, NM_RCLICK, OnRightClick)
		NOTIFY_HANDLER(IDC_PROJECT_TREE, TVN_KEYDOWN, OnKeyDown)
		NOTIFY_HANDLER(IDC_PROJECT_TREE, TVN_BEGINLABELEDIT, OnBeginLabelEdit)
		NOTIFY_HANDLER(IDC_PROJECT_TREE, TVN_ENDLABELEDIT, OnEndLabelEdit)
		NOTIFY_HANDLER(IDC_PROJECT_TREE, TVN_BEGINDRAG, OnBeginDrag)
		NOTIFY_HANDLER(IDC_PROJECT_TREE, NM_CUSTOMDRAW, OnCustomDraw)
	END_MSG_MAP()

private:
	static constexpr int IDC_PROJECT_TREE = 7101;
	static constexpr UINT WM_REFRESH = WM_APP + 30;

	LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSetFocus(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnMouseMove(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnLButtonUp(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnCaptureChanged(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDropFiles(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnRefresh(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDoubleClick(int, LPNMHDR, BOOL&);
	LRESULT OnRightClick(int, LPNMHDR, BOOL&);
	LRESULT OnKeyDown(int, LPNMHDR, BOOL&);
	LRESULT OnBeginLabelEdit(int, LPNMHDR, BOOL&);
	LRESULT OnEndLabelEdit(int, LPNMHDR, BOOL&);
	LRESULT OnBeginDrag(int, LPNMHDR, BOOL&);
	LRESULT OnCustomDraw(int, LPNMHDR, BOOL&);

	ProjectNode NodeOf(HTREEITEM item) const;
	HTREEITEM ItemAt(CPoint clientOfTree) const;
	// the text of a node as the tree shows it
	CString TextOf(ProjectNode const& node) const;
	void AddChildren(HTREEITEM parent, std::wstring const& group);
	HTREEITEM Insert(HTREEITEM parent, ProjectNode node, int image);
	void CancelDrag();
	// can `moved` be dropped on `target`: not on itself, nor a group into its own subtree
	bool CanDrop(ProjectNode const& moved, ProjectNode const& target) const;

	IProjectHost* m_Host{ nullptr };
	Project const* m_Project{ nullptr };
	CTreeViewCtrl m_Tree;
	CImageList m_Images;
	CFont m_TextFont;
	std::vector<ProjectNode> m_Nodes;			// what the items' lParam count as indexes into
	std::vector<HTREEITEM> m_Items;				// ... and the tree item of each
	bool m_First{ true };
	// dragging: the node picked up
	std::optional<ProjectNode> m_Dragged;
};
