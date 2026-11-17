#include "pch.h"
#include "ChartAnalysisView.h"
#include "AppSettings.h"

ChartAnalysisOptions CChartAnalysisView::Options() const {
	ChartAnalysisOptions options;
	options.Rulers = AppSettings::Get().ChartAnalysisModernRulers() ? RulerSystem::Modern : RulerSystem::Traditional;
	options.Weighted = AppSettings::Get().ChartAnalysisWeighted() != 0;
	return options;
}

void CChartAnalysisView::SetChartData(ChartData const* chart, AspectSettings const& settings) {
	m_Data = chart;
	m_Settings = settings;
	Rebuild();
}

std::vector<int> CChartAnalysisView::PathOf(HTREEITEM item) const {
	std::vector<int> path;
	while (item) {
		int index = 0;
		for (auto sibling = m_Tree.GetPrevSiblingItem(item); sibling; sibling = m_Tree.GetPrevSiblingItem(sibling))
			index++;
		path.insert(path.begin(), index);
		item = m_Tree.GetParentItem(item);
	}
	return path;
}

HTREEITEM CChartAnalysisView::ItemAt(std::vector<int> const& path) const {
	HTREEITEM item = nullptr;
	for (int index : path) {
		item = item ? m_Tree.GetChildItem(item) : m_Tree.GetRootItem();
		for (int i = 0; i < index && item; i++)
			item = m_Tree.GetNextSiblingItem(item);
		if (!item)
			return nullptr;
	}
	return item;
}

void CChartAnalysisView::Rebuild() {
	m_Sections.clear();
	if (!m_Tree.m_hWnd)
		return;
	// (the item selected, and the top one, come back after the tree is made again - Step and Live remake it all the time)
	std::vector<int> selected, top;
	if (auto item = m_Tree.GetSelectedItem())
		selected = PathOf(item);
	if (auto item = m_Tree.GetFirstVisibleItem())
		top = PathOf(item);

	m_Rebuilding = true;
	m_Tree.SetRedraw(FALSE);
	m_Tree.DeleteAllItems();
	m_ItemPlanets.clear();
	if (m_Data && !m_Data->AllPlanets().empty()) {
		auto options = Options();
		auto result = ChartAnalysis::Run(*m_Data, AspectCalculator(m_Settings), options);
		m_Sections = AnalysisReport::Build(*m_Data, result, options);

		std::function<void(AnalysisReport::Item const&, HTREEITEM)> insert = [&](AnalysisReport::Item const& item, HTREEITEM parent) {
			auto node = m_Tree.InsertItem(TVIF_TEXT | TVIF_PARAM, item.Text, 0, 0, 0, 0, static_cast<LPARAM>(m_ItemPlanets.size()), parent, TVI_LAST);
			m_ItemPlanets.push_back(item.Planets);
			for (auto const& child : item.Children)
				insert(child, node);
			m_Tree.Expand(node);
		};
		for (auto const& section : m_Sections) {
			auto root = m_Tree.InsertItem(TVIF_TEXT | TVIF_PARAM | TVIF_STATE, section.Title, 0, 0, TVIS_BOLD, TVIS_BOLD,
				static_cast<LPARAM>(m_ItemPlanets.size()), TVI_ROOT, TVI_LAST);
			m_ItemPlanets.push_back({});
			for (auto const& item : section.Items)
				insert(item, root);
			m_Tree.Expand(root);
		}
		if (auto item = ItemAt(top))
			m_Tree.SelectSetFirstVisible(item);
		if (auto item = ItemAt(selected))
			m_Tree.SelectItem(item);
	}
	m_Tree.SetRedraw(TRUE);
	m_Rebuilding = false;

	// (the wheel follows the item now selected - another one, or none, if the tree changed)
	if (IsWindowVisible())
		HighlightSelected();
}

void CChartAnalysisView::HighlightSelected() {
	if (!Highlight)
		return;
	auto item = m_Tree.m_hWnd ? m_Tree.GetSelectedItem() : nullptr;
	auto index = item ? static_cast<size_t>(m_Tree.GetItemData(item)) : m_ItemPlanets.size();
	Highlight(index < m_ItemPlanets.size() ? m_ItemPlanets[index] : std::vector<Planet>());
}

LRESULT CChartAnalysisView::OnShowWindow(UINT, WPARAM wParam, LPARAM, BOOL& handled) {
	handled = FALSE;
	if (wParam)
		HighlightSelected();
	else if (Highlight)
		Highlight({});
	return 0;
}

LRESULT CChartAnalysisView::OnSelChanged(int, LPNMHDR, BOOL&) {
	if (!m_Rebuilding && IsWindowVisible())
		HighlightSelected();
	return 0;
}

LRESULT CChartAnalysisView::OnChoice(WORD, WORD, HWND, BOOL&) {
	AppSettings::Get().ChartAnalysisModernRulers(m_Rulers.GetCurSel() == 1 ? 1 : 0);
	AppSettings::Get().ChartAnalysisWeighted(m_Weighted.GetCheck() == BST_CHECKED ? 1 : 0);
	Rebuild();
	return 0;
}

void CChartAnalysisView::ApplyTextFont() {
	m_HasTextFont = Helpers::UserTextFont(m_TextFont, DpiHelper::GetDpi(m_hWnd));
	if (m_Tree.m_hWnd)
		m_Tree.SetFont(m_HasTextFont ? m_TextFont.m_hFont : m_UiFont.m_hFont);
}

void CChartAnalysisView::ApplyUiFont() {
	// the strip's controls in the font the system uses for its windows, at the window's DPI
	Helpers::MessageFont(m_UiFont, DpiHelper::GetDpi(m_hWnd));
	for (CWindow* control : { (CWindow*)&m_RulersLabel, (CWindow*)&m_Rulers, (CWindow*)&m_Weighted, (CWindow*)&m_Tree })
		control->SetFont(m_UiFont);
}

void CChartAnalysisView::OnDpiChanged(UINT) {
	ApplyUiFont();
	ApplyTextFont();
	Layout();
}

LRESULT CChartAnalysisView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_RulersLabel.Create(m_hWnd, rcDefault, L"Rulers:", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE);
	m_Rulers.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 0, IDC_CA_RULERS);
	m_Weighted.Create(m_hWnd, rcDefault, L"Weighted (Sun, Moon, Asc, MC count twice)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, IDC_CA_WEIGHTED);
	m_Tree.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | TVS_HASLINES | TVS_HASBUTTONS |
		TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP, 0, IDC_CA_TREE);
	m_Rulers.AddString(L"Traditional");
	m_Rulers.AddString(L"Modern (Uranus, Neptune, Pluto)");
	m_Rulers.SetCurSel(AppSettings::Get().ChartAnalysisModernRulers() ? 1 : 0);
	m_Weighted.SetCheck(AppSettings::Get().ChartAnalysisWeighted() ? BST_CHECKED : BST_UNCHECKED);
	ApplyUiFont();
	ApplyTextFont();
	Layout();
	Rebuild();
	return 0;
}

void CChartAnalysisView::Layout() {
	if (!m_Tree.m_hWnd)
		return;
	CRect rc;
	GetClientRect(&rc);
	auto px = [this](int value) { return DpiHelper::Scale(value, m_hWnd); };
	int x = px(8), y = px(4), h = px(24);
	m_RulersLabel.MoveWindow(x, y, px(44), h);
	m_Rulers.MoveWindow(x + px(46), y, px(210), px(200));
	m_Weighted.MoveWindow(x + px(270), y, px(280), h);
	int strip = px(32);
	m_Tree.MoveWindow(0, strip, rc.Width(), std::max<int>(0, rc.Height() - strip));
}

LRESULT CChartAnalysisView::OnEraseBkgnd(UINT, WPARAM wParam, LPARAM, BOOL&) {
	CRect rc;
	GetClientRect(&rc);
	::FillRect(reinterpret_cast<HDC>(wParam), &rc, ::GetSysColorBrush(COLOR_3DFACE));
	return 1;
}

LRESULT CChartAnalysisView::OnCtlColorStatic(UINT, WPARAM wParam, LPARAM, BOOL&) {
	auto dc = reinterpret_cast<HDC>(wParam);
	::SetBkColor(dc, ::GetSysColor(COLOR_3DFACE));
	::SetTextColor(dc, ::GetSysColor(COLOR_WINDOWTEXT));
	return reinterpret_cast<LRESULT>(::GetSysColorBrush(COLOR_3DFACE));
}

LRESULT CChartAnalysisView::OnSize(UINT, WPARAM, LPARAM, BOOL&) {
	Layout();
	return 0;
}

LRESULT CChartAnalysisView::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
	if (m_Tree.m_hWnd)
		m_Tree.SetFocus();
	return 0;
}
