#include "pch.h"
#include "AspectListView.h"
#include "Helpers.h"
#include "DefaultFont.h"
#include "SortHelper.h"
#include <DarkMode/DmlibColor.h>
#include <WTLHelper.h>
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
		case ColumnType::Planet1Pos: return Helpers::FormatLongitude(aspect.Planet1.Longitude, FormatOptions::ShowSeconds | FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph);
		case ColumnType::Planet2Pos: return Helpers::FormatLongitude(aspect.Planet2.Longitude, FormatOptions::ShowSeconds | FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph);
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
			case ColumnType::Planet1Pos: return SortHelper::Sort(a1.Planet1.Longitude, a2.Planet1.Longitude, si->SortAscending);
			case ColumnType::Planet2Pos: return SortHelper::Sort(a1.Planet2.Longitude, a2.Planet2.Longitude, si->SortAscending);
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

	// every column is fully custom-painted (never leaving anything to the control's own
	// default draw) so behavior - in particular, whether hovering highlights a row - is
	// consistent across all of them; letting some columns fall back to default painting
	// while others are custom-drawn is what caused the inconsistent hover look
	switch (col) {
		case ColumnType::Planet1:
			DrawGlyphAndName(cd, DefaultFont::Get().GetPlanetGlyphAsString(aspect.Planet1.Planet), Helpers::GetPlanetName(aspect.Planet1.Planet));
			break;
		case ColumnType::Planet2:
			DrawGlyphAndName(cd, DefaultFont::Get().GetPlanetGlyphAsString(aspect.Planet2.Planet), Helpers::GetPlanetName(aspect.Planet2.Planet));
			break;
		case ColumnType::Aspect:
			DrawGlyphAndName(cd, DefaultFont::Get().GetAspectGlyphAsString(aspect.Type), Helpers::GetAspectName(aspect.Type));
			break;
		case ColumnType::Planet1Pos:
			// the sign glyph and the degree/minute/second digits share one HamburgSymbols
			// string (see Helpers::FormatLongitude), unlike the name columns above, so a
			// single font suffices here
			DrawCell(cd, Helpers::FormatLongitude(aspect.Planet1.Longitude, FormatOptions::ShowSeconds | FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph),
				m_Font.m_hFont, GetElementColor(aspect.Planet1.Longitude.Sign()));
			break;
		case ColumnType::Planet2Pos:
			DrawCell(cd, Helpers::FormatLongitude(aspect.Planet2.Longitude, FormatOptions::ShowSeconds | FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph),
				m_Font.m_hFont, GetElementColor(aspect.Planet2.Longitude.Sign()));
			break;
		case ColumnType::Orb: {
			CString s;
			s.Format(L"%.2f%c", aspect.Orb, 0xb0);
			DrawCell(cd, s, m_List.GetFont());
			break;
		}
		case ColumnType::Applying:
			DrawCell(cd, aspect.Applying ? L"A" : L"S", m_List.GetFont());
			break;
	}
	return CDRF_SKIPDEFAULT;
}

void CAspectListView::GetCellColors(LPNMCUSTOMDRAW cd, COLORREF backColorOverride, COLORREF& backColor, COLORREF& textColor) const {
	// cd->uItemState isn't reliable for LVS_OWNERDATA lists (a known comctl32 limitation),
	// so ask the control for the real selection state instead
	bool selected = (m_List.GetItemState((int)cd->dwItemSpec, LVIS_SELECTED) & LVIS_SELECTED) != 0;
	backColor = selected ? ::GetSysColor(COLOR_HIGHLIGHT)
		: (backColorOverride != CLR_INVALID ? backColorOverride : m_List.GetBkColor());
	textColor = selected ? ::GetSysColor(COLOR_HIGHLIGHTTEXT) : m_List.GetTextColor();
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

	COLORREF backColor, textColor;
	GetCellColors(cd, CLR_INVALID, backColor, textColor);

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

void CAspectListView::DrawCell(LPNMCUSTOMDRAW cd, PCWSTR text, HFONT font, COLORREF backColorOverride) const {
	auto lv = (LPNMLVCUSTOMDRAW)cd;
	CDCHandle dc(cd->hdc);
	CRect rc;
	m_List.GetSubItemRect((int)cd->dwItemSpec, lv->iSubItem, LVIR_BOUNDS, &rc);

	COLORREF backColor, textColor;
	GetCellColors(cd, backColorOverride, backColor, textColor);

	CBrush back;
	back.CreateSolidBrush(backColor);
	dc.FillRect(&rc, back);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(textColor);

	CRect r(rc);
	r.left += 4;
	dc.SelectFont(font);
	dc.DrawText(text, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

COLORREF CAspectListView::GetElementColor(ZodiacSign sign) {
	// fire, earth, air, water
	static const COLORREF lightColors[] = {
		RGB(255, 69, 0),		// OrangeRed
		RGB(250, 250, 210),		// LightGoldenrodYellow
		RGB(144, 238, 144),		// LightGreen
		RGB(173, 216, 230),		// LightBlue
	};
	static const COLORREF darkColors[] = {
		RGB(140, 50, 20),
		RGB(120, 105, 30),
		RGB(40, 100, 55),
		RGB(40, 80, 130),
	};
	auto const& colors = WTLHelper::IsDarkMode() ? darkColors : lightColors;
	return colors[int(sign) % 4];
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
	cm->AddColumn(L"P1", LVCFMT_LEFT, 90, ColumnType::Planet1);
	cm->AddColumn(L"P1 Pos", LVCFMT_LEFT, 100, ColumnType::Planet1Pos);
	cm->AddColumn(L"P2", LVCFMT_LEFT, 90, ColumnType::Planet2);
	cm->AddColumn(L"P2 Pos", LVCFMT_LEFT, 100, ColumnType::Planet2Pos);
	cm->AddColumn(L"Aspect", LVCFMT_LEFT, 110, ColumnType::Aspect);
	cm->AddColumn(L"Orb", LVCFMT_RIGHT | LVCFMT_FIXED_WIDTH, 60, ColumnType::Orb);
	cm->AddColumn(L"A/S", LVCFMT_CENTER | LVCFMT_FIXED_WIDTH, 50, ColumnType::Applying);
	cm->UpdateColumns();

	DarkMode::setDarkWndNotifySafe(m_hWnd);

	return 0;
}
