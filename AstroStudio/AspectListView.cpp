#include "pch.h"
#include "AspectListView.h"
#include "Helpers.h"
#include "DefaultFont.h"
#include "SortHelper.h"
#include <DarkMode/DmlibColor.h>
#include <algorithm>

CAspectListView::CAspectListView(IMainFrame* frame) : CFrameView(frame) {
}

CString CAspectListView::GetColumnText(HWND h, int row, int col) const {
	auto& aspect = m_Aspects[row];
	switch (GetColumnManager(h)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Planet1: return Helpers::GetPlanetName(aspect.Planet1.Planet);
		case ColumnType::Planet2: return Helpers::GetPlanetName(aspect.Planet2.Planet);
		case ColumnType::Aspect: return Helpers::GetAspectName(aspect.Type);
		case ColumnType::Orb: {
			CString s;
			s.Format(L"%.2f%c", aspect.Orb, 0xb0);
			return s;
		}
		case ColumnType::Applying: return aspect.Applying ? L"A" : L"S";
	}
	return CString();
}

void CAspectListView::DoSort(SortInfo const* si) {
	auto compare = [&](auto& a1, auto& a2) {
		switch (GetColumnManager(m_List)->GetColumnTag<ColumnType>(si->SortColumn)) {
			case ColumnType::Planet1: return SortHelper::Sort(a1.Planet1.Planet, a2.Planet1.Planet, si->SortAscending);
			case ColumnType::Planet2: return SortHelper::Sort(a1.Planet2.Planet, a2.Planet2.Planet, si->SortAscending);
			case ColumnType::Aspect: return SortHelper::Sort(a1.Type, a2.Type, si->SortAscending);
			case ColumnType::Orb: return SortHelper::Sort(a1.Orb, a2.Orb, si->SortAscending);
			case ColumnType::Applying: return SortHelper::Sort(a1.Applying, a2.Applying, si->SortAscending);
		}
		return false;
	};
	std::ranges::sort(m_Aspects, compare);
}

DWORD CAspectListView::OnPrePaint(int, LPNMCUSTOMDRAW) noexcept {
	return CDRF_NOTIFYITEMDRAW;
}

DWORD CAspectListView::OnItemPrePaint(int, LPNMCUSTOMDRAW) noexcept {
	return CDRF_NOTIFYSUBITEMDRAW;
}

DWORD CAspectListView::OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) const noexcept {
	auto lv = (LPNMLVCUSTOMDRAW)cd;
	auto col = GetColumnManager(m_List)->GetColumnTag<ColumnType>(lv->iSubItem);
	auto& aspect = m_Aspects[(int)cd->dwItemSpec];

	switch (col) {
		case ColumnType::Planet1:
			DrawGlyphAndName(cd, DefaultFont::Get().GetPlanetGlyphAsString(aspect.Planet1.Planet), Helpers::GetPlanetName(aspect.Planet1.Planet));
			return CDRF_SKIPDEFAULT;
		case ColumnType::Planet2:
			DrawGlyphAndName(cd, DefaultFont::Get().GetPlanetGlyphAsString(aspect.Planet2.Planet), Helpers::GetPlanetName(aspect.Planet2.Planet));
			return CDRF_SKIPDEFAULT;
		case ColumnType::Aspect:
			DrawGlyphAndName(cd, DefaultFont::Get().GetAspectGlyphAsString(aspect.Type), Helpers::GetAspectName(aspect.Type));
			return CDRF_SKIPDEFAULT;
		default:
			::SelectObject(cd->hdc, m_List.GetFont());
			return CDRF_NEWFONT;
	}
}

void CAspectListView::DrawGlyphAndName(LPNMCUSTOMDRAW cd, PCWSTR glyph, PCWSTR name) const {
	//
	// the glyph (HamburgSymbols) and its name (standard font) can't share a single font,
	// so this cell is drawn manually instead of letting the control draw the text
	//
	auto lv = (LPNMLVCUSTOMDRAW)cd;
	CDCHandle dc(cd->hdc);
	CRect rc;
	m_List.GetSubItemRect((int)cd->dwItemSpec, lv->iSubItem, LVIR_BOUNDS, &rc);

	// cd->uItemState isn't reliable for LVS_OWNERDATA lists (a known comctl32 limitation),
	// so ask the control for the real selection state instead
	bool selected = (m_List.GetItemState((int)cd->dwItemSpec, LVIS_SELECTED) & LVIS_SELECTED) != 0;

	COLORREF backColor = selected ? ::GetSysColor(COLOR_HIGHLIGHT) : m_List.GetBkColor();
	COLORREF textColor = selected ? ::GetSysColor(COLOR_HIGHLIGHTTEXT) : m_List.GetTextColor();

	CBrush back;
	back.CreateSolidBrush(backColor);
	dc.FillRect(&rc, back);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(textColor);

	CRect r(rc);
	r.left += 4;

	dc.SelectFont(m_Font);
	CSize sz;
	dc.GetTextExtent(glyph, (int)wcslen(glyph), &sz);
	dc.DrawText(glyph, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	r.left += sz.cx + 6;

	dc.SelectFont(m_List.GetFont());
	dc.DrawText(name, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void CAspectListView::SetAspects(std::vector<AspectData> aspects) noexcept {
	m_Aspects = std::move(aspects);
}

void CAspectListView::Refresh() {
	Sort(GetSortInfo(m_List));
	m_List.SetItemCountEx((int)m_Aspects.size(), LVSICF_NOSCROLL);
	if (m_List.GetItemCount() > 0)
		m_List.RedrawItems(0, m_List.GetItemCount() - 1);
}

LRESULT CAspectListView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_OWNERDATA | LVS_REPORT | LVS_SHOWSELALWAYS);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

	LOGFONT lf;
	CFontHandle(m_List.GetFont()).GetLogFont(lf);
	wcscpy_s(lf.lfFaceName, L"HamburgSymbols");
	m_Font.CreateFontIndirect(&lf);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Planet 1", LVCFMT_LEFT, 90, ColumnType::Planet1);
	cm->AddColumn(L"Planet 2", LVCFMT_LEFT, 90, ColumnType::Planet2);
	cm->AddColumn(L"Aspect", LVCFMT_LEFT, 110, ColumnType::Aspect);
	cm->AddColumn(L"Orb", LVCFMT_RIGHT | LVCFMT_FIXED_WIDTH, 60, ColumnType::Orb);
	cm->AddColumn(L"A/S", LVCFMT_LEFT, 50, ColumnType::Applying);
	cm->UpdateColumns();

	DarkMode::setDarkWndNotifySafe(m_hWnd);

	return 0;
}
