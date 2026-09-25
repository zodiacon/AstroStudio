#include "pch.h"
#include "PartListView.h"
#include "AspectListView.h"
#include "Helpers.h"
#include "DefaultFont.h"
#include "SortHelper.h"
#include <WTLHelper.h>
#include <algorithm>

CPartListView::CPartListView(IMainFrame* frame) : CFrameView(frame) {
}

void CPartListView::SetChartData(ChartData const* chart, AspectSettings const& settings) {
	m_Rows.clear();
	if (chart && !chart->AllPlanets().empty()) {
		auto parts = ArabicParts::Calculate(*chart);
		// the user's choice of aspects and planets, but every orb one degree
		auto tight = settings;
		tight.AspectOrb.fill(PartAspectOrb);
		tight.PlanetOrbAdd.fill(0);
		auto aspects = ArabicParts::Aspects(parts, chart->AllPlanets(), AspectCalculator(tight));
		for (size_t i = 0; i < parts.size(); i++)
			m_Rows.push_back({ parts[i], static_cast<int>(i), {}, CString() });
		for (auto const& aspect : aspects) {
			auto& row = m_Rows[aspect.Part];
			CString text;
			text.Format(L"%s %s %.2f%c", Helpers::GetAspectName(aspect.Type), Helpers::GetPlanetName(aspect.Planet.Planet), aspect.Orb, 0xb0);
			row.AspectText += (row.AspectText.IsEmpty() ? L"" : L", ") + text;
			row.Aspects.push_back(aspect);
		}
	}
	if (m_List.m_hWnd == nullptr)
		return;		// (OnCreate puts them on the screen)
	Sort(GetSortInfo(m_List));
	m_List.SetItemCountEx((int)m_Rows.size(), LVSICF_NOSCROLL);
	if (m_List.GetItemCount() > 0)
		m_List.RedrawItems(0, m_List.GetItemCount() - 1);
}

void CPartListView::ApplyTextFont() {
	if (!Helpers::UserTextFont(m_TextFont))
		return;
	m_List.SetFont(m_TextFont);
	// the glyph font goes with it
	LOGFONT lf;
	m_TextFont.GetLogFont(lf);
	wcscpy_s(lf.lfFaceName, L"HamburgSymbols");
	if (m_Font)
		m_Font.DeleteObject();
	m_Font.CreateFontIndirect(&lf);
	// the columns grow with the text (they were made for 9 points)
	int size = -MulDiv(lf.lfHeight, 72 * 10, ::GetDeviceCaps(CClientDC(m_hWnd), LOGPIXELSY));
	double ratio = std::max(1.0, size / 90.0);
	const int widths[] = { 90, 110, 40, 170, 420 };
	for (int i = 0; i < _countof(widths); i++)
		Helpers::SetColumnWidth(m_List, i, static_cast<int>(std::lround(widths[i] * ratio)));
	m_List.Invalidate();
}

CString CPartListView::GetColumnText(HWND h, int row, int col) const {
	auto& r = m_Rows[row];
	switch (GetColumnManager(h)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Part: return r.Data.Name.c_str();
		case ColumnType::Position: return Helpers::FormatLongitude(r.Data.Longitude, FormatOptions::ShowSeconds | FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph);
		case ColumnType::House: {
			CString s;
			if (r.Data.House > 0)
				s.Format(L"%d", r.Data.House);
			return s;
		}
		case ColumnType::Formula: return r.Data.Formula.c_str();
		case ColumnType::Aspects: return r.AspectText;
	}
	return CString();
}

void CPartListView::DoSort(SortInfo const* si) {
	auto compare = [&](Row const& r1, Row const& r2) {
		switch (GetColumnManager(m_List)->GetColumnTag<ColumnType>(si->SortColumn)) {
			case ColumnType::Part: return SortHelper::Sort(r1.Order, r2.Order, si->SortAscending);
			case ColumnType::Position: return SortHelper::Sort(r1.Data.Longitude.Value, r2.Data.Longitude.Value, si->SortAscending);
			case ColumnType::House: return SortHelper::Sort(r1.Data.House, r2.Data.House, si->SortAscending);
			case ColumnType::Formula: return SortHelper::Sort(r1.Data.Formula, r2.Data.Formula, si->SortAscending);
			case ColumnType::Aspects: return SortHelper::Sort(r1.Aspects.size(), r2.Aspects.size(), si->SortAscending);
		}
		return false;
	};
	std::ranges::stable_sort(m_Rows, compare);
}

DWORD CPartListView::OnPrePaint(int, LPNMCUSTOMDRAW) noexcept {
	return CDRF_NOTIFYITEMDRAW;
}

DWORD CPartListView::OnItemPrePaint(int, LPNMCUSTOMDRAW) noexcept {
	return CDRF_NOTIFYSUBITEMDRAW;
}

DWORD CPartListView::OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) const noexcept {
	auto lv = (LPNMLVCUSTOMDRAW)cd;
	auto col = GetColumnManager(m_List)->GetColumnTag<ColumnType>(lv->iSubItem);
	auto& row = m_Rows[(int)cd->dwItemSpec];
	auto text = GetColumnText(m_List, (int)cd->dwItemSpec, lv->iSubItem);

	// every column is custom-painted, like the aspect list's, so that hovering looks the same in all of them
	if (col == ColumnType::Position)
		DrawCell(cd, text, m_Font.m_hFont, CAspectListView::GetElementColor(row.Data.Longitude.Sign()));
	else if (col == ColumnType::Aspects)
		DrawAspects(cd, row.Aspects);
	else
		DrawCell(cd, text, m_List.GetFont(), CLR_INVALID, col == ColumnType::House);		// (the house number is right aligned)
	return CDRF_SKIPDEFAULT;
}

void CPartListView::GetCellColors(LPNMCUSTOMDRAW cd, COLORREF backColorOverride, COLORREF& backColor, COLORREF& textColor) const {
	// (the state in cd isn't reliable for a list with LVS_OWNERDATA, so the control is asked)
	bool selected = (m_List.GetItemState((int)cd->dwItemSpec, LVIS_SELECTED) & LVIS_SELECTED) != 0;
	backColor = selected ? ::GetSysColor(COLOR_HIGHLIGHT)
		: (backColorOverride != CLR_INVALID ? backColorOverride : m_List.GetBkColor());
	textColor = selected ? ::GetSysColor(COLOR_HIGHLIGHTTEXT) : m_List.GetTextColor();
}

void CPartListView::FillCell(LPNMCUSTOMDRAW cd, COLORREF backColorOverride, CRect& rc, COLORREF& textColor) const {
	auto lv = (LPNMLVCUSTOMDRAW)cd;
	CDCHandle dc(cd->hdc);
	m_List.GetSubItemRect((int)cd->dwItemSpec, lv->iSubItem, lv->iSubItem == 0 ? LVIR_LABEL : LVIR_BOUNDS, &rc);
	rc.right -= 2;		// (the last two pixels are the column's line: leave them alone)

	COLORREF backColor;
	GetCellColors(cd, backColorOverride, backColor, textColor);

	CBrush back;
	back.CreateSolidBrush(backColor);
	dc.FillRect(&rc, back);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(textColor);
}

void CPartListView::DrawAspects(LPNMCUSTOMDRAW cd, std::vector<PartAspect> const& aspects) const {
	CDCHandle dc(cd->hdc);
	CRect rc;
	COLORREF textColor;
	FillCell(cd, CLR_INVALID, rc, textColor);

	int x = rc.left + 4;
	for (auto const& aspect : aspects) {
		// the two glyphs, then the orb in the list's font
		auto glyphs = DefaultFont::Get().GetAspectGlyphAsString(aspect.Type) + DefaultFont::Get().GetPlanetGlyphAsString(aspect.Planet.Planet);
		CString orb;
		orb.Format(L" %.2f%c", aspect.Orb, 0xb0);
		CSize glyphSize, orbSize;
		dc.SelectFont(m_Font);
		dc.GetTextExtent(glyphs, glyphs.GetLength(), &glyphSize);
		dc.SelectFont(m_List.GetFont());
		dc.GetTextExtent(orb, orb.GetLength(), &orbSize);
		if (x + glyphSize.cx + orbSize.cx > rc.right)
			break;
		CRect r(x, rc.top, x + glyphSize.cx, rc.bottom);
		dc.SelectFont(m_Font);
		dc.DrawText(glyphs, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
		r = CRect(x + glyphSize.cx, rc.top, rc.right, rc.bottom);
		dc.SelectFont(m_List.GetFont());
		dc.DrawText(orb, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
		x += glyphSize.cx + orbSize.cx + 14;
	}
}

void CPartListView::DrawCell(LPNMCUSTOMDRAW cd, PCWSTR text, HFONT font, COLORREF backColorOverride, bool right) const {
	CDCHandle dc(cd->hdc);
	CRect rc;
	COLORREF textColor;
	FillCell(cd, backColorOverride, rc, textColor);

	CRect r(rc);
	r.left += 4;
	r.right -= 6;
	dc.SelectFont(font);
	dc.DrawText(text, -1, &r, (right ? DT_RIGHT : DT_LEFT) | DT_VCENTER | DT_SINGLELINE);
}

LRESULT CPartListView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_OWNERDATA | LVS_REPORT | LVS_SHOWSELALWAYS);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

	LOGFONT lf;
	CFontHandle(m_List.GetFont()).GetLogFont(lf);
	wcscpy_s(lf.lfFaceName, L"HamburgSymbols");
	m_Font.CreateFontIndirect(&lf);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Part", LVCFMT_LEFT, 90, ColumnType::Part);
	cm->AddColumn(L"Position", LVCFMT_LEFT, 110, ColumnType::Position);
	cm->AddColumn(L"H", LVCFMT_RIGHT | LVCFMT_FIXED_WIDTH, 40, ColumnType::House);
	cm->AddColumn(L"Formula", LVCFMT_LEFT, 170, ColumnType::Formula);
	CString aspectsHeader;
	aspectsHeader.Format(L"Aspects to the planets (%d%c orb)", static_cast<int>(PartAspectOrb), 0xb0);
	cm->AddColumn(aspectsHeader, LVCFMT_LEFT, 420, ColumnType::Aspects);
	cm->UpdateColumns();
	ApplyTextFont();

	// what was set before the window existed
	m_List.SetItemCountEx((int)m_Rows.size(), LVSICF_NOSCROLL);
	return 0;
}
