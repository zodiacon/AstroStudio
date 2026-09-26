#include "pch.h"
#include "MidpointListView.h"
#include "AspectListView.h"
#include "DerivedCharts.h"
#include "Helpers.h"
#include "DefaultFont.h"
#include "SortHelper.h"
#include "MidpointSettings.h"
#include <WTLHelper.h>
#include <ToolbarHelper.h>
#include <algorithm>

CMidpointListView::CMidpointListView(IMainFrame* frame) : CFrameView(frame) {
}

CString CMidpointListView::PointText(ChartPoint const& point) const {
	auto name = PointName(point);
	if (!m_Overlay)
		return name;
	return CString(point.Set == 1 ? m_Overlay->Label.c_str() : m_Overlay->BaseLabel.c_str()) + L" " + name;
}

void CMidpointListView::SelectKind(ContactKind kind) {
	for (int i = 0; i < m_Kind.GetCount(); i++)
		if (static_cast<ContactKind>(m_Kind.GetItemData(i)) == kind)
			m_Kind.SetCurSel(i);
}

void CMidpointListView::SyncSettings() {
	auto& settings = MidpointSettings::Current();
	if (m_Orb.m_hWnd) {
		m_Orb.SetCurSel(MidpointSettings::NearestOrb(settings.ListOrb));
		SelectKind(settings.ListKind);
	}
	SetChartData(m_Chart);
}

LRESULT CMidpointListView::OnChoice(WORD, WORD, HWND, BOOL&) {
	auto& settings = MidpointSettings::Current();
	int orb = m_Orb.GetCurSel();
	if (orb >= 0 && orb < _countof(MidpointSettings::Orbs))
		settings.ListOrb = MidpointSettings::Orbs[orb];
	if (m_Kind.GetCurSel() >= 0)
		settings.ListKind = static_cast<ContactKind>(m_Kind.GetItemData(m_Kind.GetCurSel()));
	MidpointSettings::StoreInSettings();
	SetChartData(m_Chart);
	return 0;
}

void CMidpointListView::SetOverlay(ChartOverlay const* overlay) {
	m_Overlay = overlay;
	SetChartData(m_Chart);
}

void CMidpointListView::SetChartData(ChartData const* chart) {
	m_Chart = chart;
	m_Rows.clear();
	if (chart && !chart->AllPlanets().empty()) {
		auto& settings = MidpointSettings::Current();
		auto points = Midpoints::Points(*chart, settings.Points(true));
		std::vector<MidpointData> midpoints;
		if (m_Overlay) {
			// the planets around the chart with the chart's: the overlay's are the first of each pair (a transit's sky has no angles)
			bool hasAngles = m_Overlay->Data.Houses().Asc.Value != 0 || m_Overlay->Data.Houses().MC.Value != 0;
			auto other = Midpoints::Points(m_Overlay->Data, settings.Points(hasAngles), 1);
			midpoints = Midpoints::CalcBetween(other, points);
			points.insert(points.end(), other.begin(), other.end());		// (both sets can stand on a midpoint)
		}
		else
			midpoints = Midpoints::Calculate(points);
		// the points on the axis of each midpoint, listed with it
		auto contacts = Midpoints::Contacts(midpoints, points, settings.ListContacts());
		std::vector<CString> on(midpoints.size());
		for (auto const& contact : contacts) {
			CString text;
			CString angle = contact.Angle == 0 ? CString() : L" (" + CString(MidpointSettings::ShortAngleWords(contact.Angle)) + L")";
			text.Format(L"%s%s %.2f%c", (PCWSTR)PointText(contact.Point), (PCWSTR)angle, contact.Orb, 0xb0);
			auto& cell = on[contact.Midpoint];
			cell += (cell.IsEmpty() ? L"" : L", ") + text;
		}
		for (size_t i = 0; i < midpoints.size(); i++)
			m_Rows.push_back({ midpoints[i], DerivedCharts::HouseOf(chart->Houses(), midpoints[i].Longitude), on[i] });
	}
	if (m_List.m_hWnd == nullptr)
		return;		// (OnCreate puts them on the screen)
	Sort(GetSortInfo(m_List));
	m_List.SetItemCountEx((int)m_Rows.size(), LVSICF_NOSCROLL);
	if (m_List.GetItemCount() > 0)
		m_List.RedrawItems(0, m_List.GetItemCount() - 1);
}

void CMidpointListView::ApplyTextFont() {
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
	const int widths[] = { 150, 150, 100, 60, 40, 340 };
	for (int i = 0; i < _countof(widths); i++)
		Helpers::SetColumnWidth(m_List, i, static_cast<int>(std::lround(widths[i] * ratio)));
	m_List.Invalidate();
}

CString CMidpointListView::PointName(ChartPoint const& point) {
	switch (point.Kind) {
		case PointKind::Ascendant: return L"Ascendant";
		case PointKind::Midheaven: return L"Midheaven";
		default: return Helpers::GetPlanetName(point.Body);
	}
}

CString CMidpointListView::PointGlyph(ChartPoint const& point) {
	return MidpointSettings::PointGlyph(point);
}

int CMidpointListView::PointOrder(ChartPoint const& point) noexcept {
	return point.Kind == PointKind::Planet ? static_cast<int>(point.Body) : 100 + static_cast<int>(point.Kind);
}

CString CMidpointListView::GetColumnText(HWND h, int row, int col) const {
	auto& data = m_Rows[row].Data;
	switch (GetColumnManager(h)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::PointA: return PointText(data.A);
		case ColumnType::PointB: return PointText(data.B);
		case ColumnType::Midpoint: return Helpers::FormatLongitude(data.Longitude, FormatOptions::ShowSeconds | FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph);
		case ColumnType::Arc: {
			CString s;
			s.Format(L"%.2f%c", data.Arc, 0xb0);
			return s;
		}
		case ColumnType::House: {
			CString s;
			if (m_Rows[row].House > 0)
				s.Format(L"%d", m_Rows[row].House);
			return s;
		}
		case ColumnType::On: return m_Rows[row].On;
	}
	return CString();
}

Helpers::TableSource CMidpointListView::Table() const {
	Helpers::TableSource table;
	table.Headers = { L"Point 1", L"Point 2", L"Midpoint", L"Arc", L"House", L"On the midpoint" };
	table.Rows = static_cast<int>(m_Rows.size());
	table.Cell = [this](int row, int column) -> CString {
		auto& r = m_Rows[row];
		auto position = [](AstroPoint const& longitude) {
			return Helpers::FormatLongitude(longitude, FormatOptions::ShowSeconds | FormatOptions::ShowDegreeGlyph);
		};
		CString text;
		switch (column) {
			case 0: return PointText(r.Data.A);
			case 1: return PointText(r.Data.B);
			case 2: return position(r.Data.Longitude);
			case 3: text.Format(L"%.2f%c", r.Data.Arc, 0xb0); return text;
			case 4: if (r.House > 0) text.Format(L"%d", r.House); return text;
			case 5: return r.On;
		}
		return text;
	};
	return table;
}

void CMidpointListView::DoSort(SortInfo const* si) {
	auto compare = [&](Row const& r1, Row const& r2) {
		switch (GetColumnManager(m_List)->GetColumnTag<ColumnType>(si->SortColumn)) {
			case ColumnType::PointA: return SortHelper::Sort(PointOrder(r1.Data.A), PointOrder(r2.Data.A), si->SortAscending);
			case ColumnType::PointB: return SortHelper::Sort(PointOrder(r1.Data.B), PointOrder(r2.Data.B), si->SortAscending);
			case ColumnType::Midpoint: return SortHelper::Sort(r1.Data.Longitude.Value, r2.Data.Longitude.Value, si->SortAscending);
			case ColumnType::Arc: return SortHelper::Sort(r1.Data.Arc, r2.Data.Arc, si->SortAscending);
			case ColumnType::House: return SortHelper::Sort(r1.House, r2.House, si->SortAscending);
			case ColumnType::On: return SortHelper::Sort(r1.On.IsEmpty(), r2.On.IsEmpty(), si->SortAscending);
		}
		return false;
	};
	std::ranges::stable_sort(m_Rows, compare);
}

DWORD CMidpointListView::OnPrePaint(int, LPNMCUSTOMDRAW) noexcept {
	return CDRF_NOTIFYITEMDRAW;
}

DWORD CMidpointListView::OnItemPrePaint(int, LPNMCUSTOMDRAW) noexcept {
	return CDRF_NOTIFYSUBITEMDRAW;
}

DWORD CMidpointListView::OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) const noexcept {
	auto lv = (LPNMLVCUSTOMDRAW)cd;
	auto col = GetColumnManager(m_List)->GetColumnTag<ColumnType>(lv->iSubItem);
	auto& data = m_Rows[(int)cd->dwItemSpec].Data;

	// every column is custom-painted, like the aspect list's, so that hovering looks the same in all of them
	switch (col) {
		case ColumnType::PointA:
			DrawGlyphAndName(cd, PointGlyph(data.A), PointText(data.A));
			break;
		case ColumnType::PointB:
			DrawGlyphAndName(cd, PointGlyph(data.B), PointText(data.B));
			break;
		case ColumnType::Midpoint:
			DrawCell(cd, GetColumnText(m_List, (int)cd->dwItemSpec, lv->iSubItem), m_Font.m_hFont, CAspectListView::GetElementColor(data.Longitude.Sign()));
			break;
		default:
			// (the numbers are right aligned; asking the list for the column's alignment doesn't work for these columns)
			DrawCell(cd, GetColumnText(m_List, (int)cd->dwItemSpec, lv->iSubItem), m_List.GetFont(), CLR_INVALID, col == ColumnType::Arc || col == ColumnType::House);
			break;
	}
	return CDRF_SKIPDEFAULT;
}

void CMidpointListView::GetCellColors(LPNMCUSTOMDRAW cd, COLORREF backColorOverride, COLORREF& backColor, COLORREF& textColor) const {
	// (the state in cd isn't reliable for a list with LVS_OWNERDATA, so the control is asked)
	bool selected = (m_List.GetItemState((int)cd->dwItemSpec, LVIS_SELECTED) & LVIS_SELECTED) != 0;
	backColor = selected ? ::GetSysColor(COLOR_HIGHLIGHT)
		: (backColorOverride != CLR_INVALID ? backColorOverride : m_List.GetBkColor());
	textColor = selected ? ::GetSysColor(COLOR_HIGHLIGHTTEXT) : m_List.GetTextColor();
}

void CMidpointListView::DrawGlyphAndName(LPNMCUSTOMDRAW cd, PCWSTR glyph, PCWSTR name) const {
	auto lv = (LPNMLVCUSTOMDRAW)cd;
	CDCHandle dc(cd->hdc);
	CRect rc;
	m_List.GetSubItemRect((int)cd->dwItemSpec, lv->iSubItem, lv->iSubItem == 0 ? LVIR_LABEL : LVIR_BOUNDS, &rc);
	rc.right -= 2;		// (the last two pixels are the column's line: leave them alone)

	COLORREF backColor, textColor;
	GetCellColors(cd, CLR_INVALID, backColor, textColor);

	CBrush back;
	back.CreateSolidBrush(backColor);
	dc.FillRect(&rc, back);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(textColor);

	CRect r(rc);
	r.left += 4;

	// the glyph (HamburgSymbols) and the name (the list's font) can't share a font
	dc.SelectFont(m_Font);
	CSize sz;
	dc.GetTextExtent(glyph, (int)wcslen(glyph), &sz);
	dc.DrawText(glyph, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	r.left += sz.cx + 6;

	dc.SelectFont(m_List.GetFont());
	dc.DrawText(name, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void CMidpointListView::DrawCell(LPNMCUSTOMDRAW cd, PCWSTR text, HFONT font, COLORREF backColorOverride, bool right) const {
	auto lv = (LPNMLVCUSTOMDRAW)cd;
	CDCHandle dc(cd->hdc);
	CRect rc;
	m_List.GetSubItemRect((int)cd->dwItemSpec, lv->iSubItem, lv->iSubItem == 0 ? LVIR_LABEL : LVIR_BOUNDS, &rc);
	rc.right -= 2;		// (the last two pixels are the column's line: leave them alone)

	COLORREF backColor, textColor;
	GetCellColors(cd, backColorOverride, backColor, textColor);

	CBrush back;
	back.CreateSolidBrush(backColor);
	dc.FillRect(&rc, back);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(textColor);

	CRect r(rc);
	r.left += 4;
	r.right -= 6;
	dc.SelectFont(font);
	dc.DrawText(text, -1, &r, (right ? DT_RIGHT : DT_LEFT) | DT_VCENTER | DT_SINGLELINE);
}

LRESULT CMidpointListView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_OWNERDATA | LVS_REPORT | LVS_SHOWSELALWAYS);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

	LOGFONT lf;
	CFontHandle(m_List.GetFont()).GetLogFont(lf);
	wcscpy_s(lf.lfFaceName, L"HamburgSymbols");
	m_Font.CreateFontIndirect(&lf);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Point 1", LVCFMT_LEFT, 150, ColumnType::PointA);
	cm->AddColumn(L"Point 2", LVCFMT_LEFT, 150, ColumnType::PointB);
	cm->AddColumn(L"Midpoint", LVCFMT_LEFT, 100, ColumnType::Midpoint);
	cm->AddColumn(L"Arc", LVCFMT_CENTER | LVCFMT_FIXED_WIDTH, 60, ColumnType::Arc);
	cm->AddColumn(L"H", LVCFMT_CENTER | LVCFMT_FIXED_WIDTH, 40, ColumnType::House);
	cm->AddColumn(L"On the midpoint", LVCFMT_LEFT, 340, ColumnType::On);
	cm->UpdateColumns();
	ApplyTextFont();

	// The orb and the kind of contact, as at the top of the midpoint tree, on a toolbar's separators (made before the toolbar joins the
	// rebar, which sizes the band from its buttons).
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	ToolBarButtonInfo buttons[] = { { 0 } };
	auto tb = ToolbarHelper::CreateAndInitToolBar(m_hWndToolBar, buttons, _countof(buttons), 16);
	CToolBarCtrl bar(tb);
	int dpi = CClientDC(m_hWnd).GetDeviceCaps(LOGPIXELSX);
	auto px = [&](int value) { return MulDiv(value, dpi, 96); };
	int firstSlot = bar.GetButtonCount();
	bar.AddSeparator(px(36));		// "Orb:"
	bar.AddSeparator(px(70));
	bar.AddSeparator(px(70));		// "Contacts:"
	bar.AddSeparator(px(250));
	AddSimpleReBarBand(tb);
	Frame()->AddToolBarToUI(tb);

	CFontHandle font(AtlGetDefaultGuiFont());
	const int comboHeight = px(22), dropHeight = px(120);
	auto slotRect = [&](int slot, int drop) {
		CRect item;
		bar.GetItemRect(firstSlot + slot, &item);
		int top = item.top + (item.Height() - comboHeight) / 2;
		return CRect(item.left, top, item.right - px(4), top + comboHeight + drop);
	};
	CRect slot1 = slotRect(0, 0);
	m_OrbLabel.Create(tb, slot1, L"Orb:", WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE);
	m_OrbLabel.SetFont(font);
	CRect slot2 = slotRect(1, dropHeight);
	m_Orb.Create(tb, slot2, nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, 0, IDC_ML_ORB);
	m_Orb.SetFont(font);
	for (int orb : MidpointSettings::Orbs) {
		CString text;
		text.Format(L"%g%c", orb / 100.0, 0xb0);
		m_Orb.AddString(text);
	}
	CRect slot3 = slotRect(2, 0);
	m_KindLabel.Create(tb, slot3, L"Contacts:", WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE);
	m_KindLabel.SetFont(font);
	CRect slot4 = slotRect(3, dropHeight);
	m_Kind.Create(tb, slot4, nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, 0, IDC_ML_KIND);
	m_Kind.SetFont(font);
	CString dial;
	dial.Format(L"90%c dial (on, semi-square, square...)", 0xb0);
	m_Kind.SetItemData(m_Kind.AddString(dial), static_cast<DWORD_PTR>(ContactKind::Dial90));
	dial.Format(L"45%c dial (all of those together)", 0xb0);
	m_Kind.SetItemData(m_Kind.AddString(dial), static_cast<DWORD_PTR>(ContactKind::Dial45));
	m_Kind.SetItemData(m_Kind.AddString(L"Axis (on and opposite)"), static_cast<DWORD_PTR>(ContactKind::Axis));
	auto& settings = MidpointSettings::Current();
	m_Orb.SetCurSel(MidpointSettings::NearestOrb(settings.ListOrb));
	SelectKind(settings.ListKind);

	// what was set before the window existed
	m_List.SetItemCountEx((int)m_Rows.size(), LVSICF_NOSCROLL);
	return 0;
}
