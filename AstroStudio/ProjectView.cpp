#include "pch.h"
#include "ProjectView.h"
#include "resource.h"
#include "Helpers.h"
#include <IconHelper.h>
#include <shellapi.h>
#include <algorithm>
#include <optional>

namespace {
	enum Image { ImageRoot, ImageGroup, ImageChart, ImageAnalysis, ImageAspects, ImageColors, ImageOther };

	int ImageOf(ProjectItemType type) {
		switch (type) {
			case ProjectItemType::Chart: return ImageChart;
			case ProjectItemType::Analysis: return ImageAnalysis;
			case ProjectItemType::AspectSet: return ImageAspects;
			case ProjectItemType::ColorSet: return ImageColors;
			default: return ImageOther;
		}
	}

	std::wstring ParentOf(std::wstring const& group) {
		auto slash = group.rfind(L'/');
		return slash == std::wstring::npos ? std::wstring() : group.substr(0, slash);
	}

	std::wstring LastPart(std::wstring const& group) {
		auto slash = group.rfind(L'/');
		return slash == std::wstring::npos ? group : group.substr(slash + 1);
	}

	bool SameText(std::wstring const& a, std::wstring const& b) {
		return _wcsicmp(a.c_str(), b.c_str()) == 0;
	}

	bool SameNode(ProjectNode const& a, ProjectNode const& b) {
		return a.Type == b.Type && SameText(a.Key, b.Key);
	}
}

void CProjectView::SetProject(Project const* project) {
	m_Project = project;
	m_First = true;
	Refresh();
}

ProjectNode CProjectView::NodeOf(HTREEITEM item) const {
	if (!item)
		return {};
	auto index = static_cast<size_t>(m_Tree.GetItemData(item));
	return index < m_Nodes.size() ? m_Nodes[index] : ProjectNode{};
}

ProjectNode CProjectView::Selected() const {
	return m_Tree.m_hWnd ? NodeOf(m_Tree.GetSelectedItem()) : ProjectNode{};
}

CString CProjectView::TextOf(ProjectNode const& node) const {
	CString text;
	switch (node.Type) {
		case ProjectNode::Kind::Root:
			text = m_Project->Name().c_str();
			if (m_Project->Dirty())
				text += L" *";
			break;
		case ProjectNode::Kind::Group:
			text = LastPart(node.Key).c_str();
			break;
		case ProjectNode::Kind::Item:
			if (auto item = m_Project->Find(node.Key)) {
				text = item->Name.c_str();
				if (m_Host && m_Host->ProjectItemModified(node.Key))
					text += L" *";
				if (item->Missing)
					text += L"  (missing)";
			}
			break;
		default:
			break;
	}
	return text;
}

HTREEITEM CProjectView::Insert(HTREEITEM parent, ProjectNode node, int image) {
	auto text = TextOf(node);
	m_Nodes.push_back(node);
	auto item = m_Tree.InsertItem(TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM, text, image, image, 0, 0,
		static_cast<LPARAM>(m_Nodes.size() - 1), parent, TVI_LAST);
	m_Items.push_back(item);
	return item;
}

void CProjectView::AddChildren(HTREEITEM parent, std::wstring const& group) {
	// the groups in it first, then the items
	for (auto const& g : m_Project->Groups())
		if (!g.empty() && SameText(ParentOf(g), group)) {
			auto node = Insert(parent, { ProjectNode::Kind::Group, g }, ImageGroup);
			AddChildren(node, g);
		}
	for (auto const& item : m_Project->Items())
		if (SameText(item.Group, group))
			Insert(parent, { ProjectNode::Kind::Item, item.Id }, ImageOf(item.Type));
}

void CProjectView::Refresh() {
	if (!m_Tree.m_hWnd)
		return;
	// what to keep: the groups that were open (all of them at first) and what was selected
	std::set<std::wstring> open;
	bool rootOpen = true;
	if (!m_First) {
		for (size_t i = 0; i < m_Items.size(); i++) {
			bool expanded = (m_Tree.GetItemState(m_Items[i], TVIS_EXPANDED) & TVIS_EXPANDED) != 0;
			if (m_Nodes[i].Type == ProjectNode::Kind::Group && expanded) {
				auto key = m_Nodes[i].Key;
				std::ranges::transform(key, key.begin(), ::towlower);
				open.insert(key);
			}
			else if (m_Nodes[i].Type == ProjectNode::Kind::Root)
				rootOpen = expanded;
		}
	}
	auto selected = Selected();

	m_Tree.SetRedraw(FALSE);
	m_Tree.DeleteAllItems();
	m_Nodes.clear();
	m_Items.clear();
	if (m_Project) {
		auto root = Insert(TVI_ROOT, { ProjectNode::Kind::Root, L"" }, ImageRoot);
		AddChildren(root, L"");
		for (size_t i = 0; i < m_Items.size(); i++) {
			auto key = m_Nodes[i].Key;
			std::ranges::transform(key, key.begin(), ::towlower);
			if (m_Nodes[i].Type == ProjectNode::Kind::Group && (m_First || open.contains(key)))
				m_Tree.Expand(m_Items[i], TVE_EXPAND);
		}
		if (m_First || rootOpen)
			m_Tree.Expand(root, TVE_EXPAND);
		m_First = false;
		UpdateStates();
		if (selected.Type != ProjectNode::Kind::None)
			Select(selected);
		else
			m_Tree.SelectItem(root);
	}
	m_Tree.SetRedraw(TRUE);
}

void CProjectView::UpdateStates() {
	if (!m_Tree.m_hWnd || !m_Project)
		return;
	for (size_t i = 0; i < m_Items.size(); i++) {
		auto const& node = m_Nodes[i];
		auto text = TextOf(node);
		CString current;
		m_Tree.GetItemText(m_Items[i], current);
		if (current != text)
			m_Tree.SetItemText(m_Items[i], text);
		bool bold = node.Type == ProjectNode::Kind::Root || (node.Type == ProjectNode::Kind::Item && m_Host && m_Host->ProjectItemOpen(node.Key));
		m_Tree.SetItemState(m_Items[i], bold ? TVIS_BOLD : 0, TVIS_BOLD);
	}
}

void CProjectView::Select(ProjectNode const& node) {
	for (size_t i = 0; i < m_Nodes.size(); i++)
		if (SameNode(m_Nodes[i], node)) {
			m_Tree.SelectItem(m_Items[i]);
			return;
		}
}

void CProjectView::BeginRename(ProjectNode const& node) {
	for (size_t i = 0; i < m_Nodes.size(); i++)
		if (SameNode(m_Nodes[i], node)) {
			m_Tree.SelectItem(m_Items[i]);
			m_Tree.SetFocus();
			m_Tree.EditLabel(m_Items[i]);
			return;
		}
}

void CProjectView::ApplyTextFont() {
	if (Helpers::UserTextFont(m_TextFont) && m_Tree.m_hWnd)
		m_Tree.SetFont(m_TextFont);
}

LRESULT CProjectView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_Tree.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | TVS_HASBUTTONS | TVS_LINESATROOT |
		TVS_SHOWSELALWAYS | TVS_EDITLABELS | TVS_FULLROWSELECT  | TVS_NOHSCROLL, 0, IDC_PROJECT_TREE);
	m_Tree.SetExtendedStyle(TVS_EX_DOUBLEBUFFER | TVS_EX_FADEINOUTEXPANDOS, TVS_EX_DOUBLEBUFFER | TVS_EX_FADEINOUTEXPANDOS);
	ModifyStyleEx(0, WS_EX_ACCEPTFILES);

	// the tree's font is the one the system uses for its windows
	NONCLIENTMETRICS metrics{ sizeof(metrics) };
	if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
		m_TextFont.CreateFontIndirect(&metrics.lfMessageFont);
		m_Tree.SetFont(m_TextFont);
	}

	// icons: the project and the groups are folders; a chart, an analysis, a set of aspects and a set of colours have theirs
	m_Images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 8, 4);
	HICON folder = IconHelper::GetStockIcon(SIID_FOLDER);
	m_Images.AddIcon(IconHelper::Load(IDI_PROJECT, 16));
	m_Images.AddIcon(folder ? folder : IconHelper::Load(IDI_CHART, 16));
	for (auto icon : { IDI_CHART, IDI_EVENT, IDI_OPTIONS, IDI_COLORWHEEL })
		m_Images.AddIcon(IconHelper::Load(icon, 16));
	HICON other = IconHelper::GetStockIcon(SIID_DOCNOASSOC);
	m_Images.AddIcon(other ? other : IconHelper::Load(IDI_CHART, 16));
	m_Tree.SetImageList(m_Images, TVSIL_NORMAL);

	ApplyTextFont();
	Refresh();
	return 0;
}

LRESULT CProjectView::OnSize(UINT, WPARAM, LPARAM, BOOL&) {
	CRect rc;
	GetClientRect(&rc);
	if (m_Tree.m_hWnd)
		m_Tree.MoveWindow(0, 0, rc.Width(), rc.Height());
	return 0;
}

LRESULT CProjectView::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&) {
	return 1;
}

LRESULT CProjectView::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
	if (m_Tree.m_hWnd)
		m_Tree.SetFocus();
	return 0;
}

HTREEITEM CProjectView::ItemAt(CPoint pt) const {
	UINT flags = 0;
	auto item = m_Tree.HitTest(pt, &flags);
	return (flags & (TVHT_ONITEM | TVHT_ONITEMRIGHT)) ? item : nullptr;
}

LRESULT CProjectView::OnDoubleClick(int, LPNMHDR, BOOL& handled) {
	CPoint pt;
	::GetCursorPos(&pt);
	m_Tree.ScreenToClient(&pt);
	auto node = NodeOf(ItemAt(pt));
	if (node.Type != ProjectNode::Kind::Item || !m_Host) {
		handled = FALSE;		// (a group opens or closes)
		return 0;
	}
	m_Host->ProjectOpenItem(node.Key);
	return 1;
}

LRESULT CProjectView::OnRightClick(int, LPNMHDR, BOOL&) {
	CPoint screen;
	::GetCursorPos(&screen);
	CPoint pt = screen;
	m_Tree.ScreenToClient(&pt);
	auto item = ItemAt(pt);
	if (item)
		m_Tree.SelectItem(item);
	if (m_Host)
		m_Host->ProjectContextMenu(NodeOf(item), screen);
	return 1;
}

LRESULT CProjectView::OnKeyDown(int, LPNMHDR pnmh, BOOL&) {
	auto key = reinterpret_cast<NMTVKEYDOWN*>(pnmh)->wVKey;
	auto node = Selected();
	if (!m_Host)
		return 0;
	if (key == VK_RETURN && node.Type == ProjectNode::Kind::Item)
		m_Host->ProjectOpenItem(node.Key);
	else if (key == VK_F2 || key == VK_DELETE || key == VK_F5)
		m_Host->ProjectKey(key, node);
	return 0;
}

LRESULT CProjectView::OnBeginLabelEdit(int, LPNMHDR pnmh, BOOL&) {
	auto info = reinterpret_cast<NMTVDISPINFO*>(pnmh);
	auto node = NodeOf(info->item.hItem);
	// the edit box has the name itself, not the marks the tree adds to it
	CString plain;
	if (node.Type == ProjectNode::Kind::Root)
		plain = m_Project->Name().c_str();
	else if (node.Type == ProjectNode::Kind::Group)
		plain = LastPart(node.Key).c_str();
	else if (auto item = m_Project->Find(node.Key))
		plain = item->Name.c_str();
	else
		return TRUE;		// (nothing to edit)
	if (HWND edit = m_Tree.GetEditControl())
		::SetWindowText(edit, plain);
	return FALSE;
}

LRESULT CProjectView::OnEndLabelEdit(int, LPNMHDR pnmh, BOOL&) {
	auto info = reinterpret_cast<NMTVDISPINFO*>(pnmh);
	if (info->item.pszText && m_Host)
		m_Host->ProjectRename(NodeOf(info->item.hItem), info->item.pszText);
	PostMessage(WM_REFRESH);		// (whatever happened, the tree shows what the project says)
	return FALSE;
}

LRESULT CProjectView::OnRefresh(UINT, WPARAM, LPARAM, BOOL&) {
	Refresh();
	return 0;
}

//
// drag and drop
//

bool CProjectView::CanDrop(ProjectNode const& moved, ProjectNode const& target) const {
	if (target.Type == ProjectNode::Kind::None || SameNode(moved, target))
		return false;
	if (moved.Type == ProjectNode::Kind::Group) {
		if (target.Type == ProjectNode::Kind::Item)
			return false;
		// not into itself: "Clients" onto "Clients/Smith"
		if (target.Type == ProjectNode::Kind::Group && (SameText(target.Key, moved.Key) ||
			(target.Key.size() > moved.Key.size() && SameText(target.Key.substr(0, moved.Key.size()), moved.Key) && target.Key[moved.Key.size()] == L'/')))
			return false;
	}
	return true;
}

LRESULT CProjectView::OnBeginDrag(int, LPNMHDR pnmh, BOOL&) {
	auto node = NodeOf(reinterpret_cast<NMTREEVIEW*>(pnmh)->itemNew.hItem);
	if (node.Type == ProjectNode::Kind::Item || node.Type == ProjectNode::Kind::Group) {
		m_Dragged = node;
		SetCapture();
	}
	return 0;
}

LRESULT CProjectView::OnMouseMove(UINT, WPARAM, LPARAM lParam, BOOL&) {
	if (!m_Dragged)
		return 0;
	CPoint pt(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	ClientToScreen(&pt);
	m_Tree.ScreenToClient(&pt);
	auto item = ItemAt(pt);
	auto target = item ? NodeOf(item) : ProjectNode{};
	bool valid = item && CanDrop(*m_Dragged, target);
	m_Tree.SelectDropTarget(valid ? item : nullptr);
	::SetCursor(::LoadCursor(nullptr, valid ? IDC_ARROW : IDC_NO));
	return 0;
}

LRESULT CProjectView::OnLButtonUp(UINT, WPARAM, LPARAM lParam, BOOL&) {
	if (!m_Dragged)
		return 0;
	CPoint pt(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	ClientToScreen(&pt);
	m_Tree.ScreenToClient(&pt);
	auto moved = *m_Dragged;
	auto target = NodeOf(ItemAt(pt));
	CancelDrag();
	if (m_Host && CanDrop(moved, target))
		m_Host->ProjectDrop(moved, target);
	return 0;
}

LRESULT CProjectView::OnCaptureChanged(UINT, WPARAM, LPARAM, BOOL&) {
	CancelDrag();
	return 0;
}

void CProjectView::CancelDrag() {
	bool was = m_Dragged.has_value();
	m_Dragged.reset();
	if (was) {
		m_Tree.SelectDropTarget(nullptr);
		if (::GetCapture() == m_hWnd)
			::ReleaseCapture();
	}
}

LRESULT CProjectView::OnDropFiles(UINT, WPARAM wParam, LPARAM, BOOL&) {
	auto drop = reinterpret_cast<HDROP>(wParam);
	POINT where{};
	::DragQueryPoint(drop, &where);
	auto target = NodeOf(ItemAt(where));
	std::vector<std::filesystem::path> files;
	UINT count = ::DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
	for (UINT i = 0; i < count; i++) {
		WCHAR path[MAX_PATH]{};
		if (::DragQueryFileW(drop, i, path, MAX_PATH))
			files.emplace_back(path);
	}
	::DragFinish(drop);
	if (m_Host && !files.empty())
		m_Host->ProjectAddFiles(files, target);
	return 0;
}

LRESULT CProjectView::OnCustomDraw(int, LPNMHDR pnmh, BOOL& handled) {
	auto draw = reinterpret_cast<NMTVCUSTOMDRAW*>(pnmh);
	switch (draw->nmcd.dwDrawStage) {
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
		case CDDS_ITEMPREPAINT: {
			auto index = static_cast<size_t>(draw->nmcd.lItemlParam);
			if (m_Project && index < m_Nodes.size() && m_Nodes[index].Type == ProjectNode::Kind::Item)
				if (auto item = m_Project->Find(m_Nodes[index].Key); item && item->Missing)
					draw->clrText = ::GetSysColor(COLOR_GRAYTEXT);		// a file that isn't there
			return CDRF_DODEFAULT;
		}
	}
	handled = FALSE;
	return 0;
}
