#include "pch.h"
#include "SynastryView.h"
#include "AspectOptions.h"
#include "Printing.h"
#include <ToolbarHelper.h>
#include "SortHelper.h"
#include "DarkMode/DarkModeSubclass.h"
#include <algorithm>

namespace {
	CString Position(AstroPoint const& longitude) {
		return Helpers::FormatLongitude(longitude, FormatOptions::ShowDegreeGlyph);
	}

	PCWSTR AngleName(bool midheaven) {
		return midheaven ? L"Midheaven" : L"Ascendant";
	}

	CString HouseName(int house) {
		CString text;
		text.Format(L"House %d", house);
		return text;
	}
}

void CSynastryView::SetCharts(OpenChart a, OpenChart b) {
	m_A = std::move(a);
	m_B = std::move(b);
	m_Zodiac = AstroCalculator::DefaultZodiac();
}

CString CSynastryView::Title() const {
	return L"Synastry: " + m_A.Name + L" & " + m_B.Name;
}

LRESULT CSynastryView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_OWNERDATA | LVS_REPORT | LVS_SHOWSELALWAYS);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);

	ToolBarButtonInfo buttons[] = {
		{ ID_SYNASTRY_SWAP, IDI_FORWARD, 0, L"Swap" },
	};
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	CToolBarCtrl tb(m_Strip.Create(m_hWndToolBar, buttons, _countof(buttons)));
	AddSimpleReBarBand(tb);
	Frame()->AddToolBarToUI(tb);

	CreateFonts();
	auto cm = GetColumnManager(m_List);
	cm->AddColumn(m_A.Name, LVCFMT_LEFT, 110, ColumnType::First);
	cm->AddColumn(L"Position", LVCFMT_LEFT, 110, ColumnType::FirstPosition);
	cm->AddColumn(L"Contact", LVCFMT_LEFT, 100, ColumnType::Contact);
	cm->AddColumn(m_B.Name, LVCFMT_LEFT, 110, ColumnType::Second);
	cm->AddColumn(L"Position", LVCFMT_LEFT, 110, ColumnType::SecondPosition);
	cm->AddColumn(L"Orb", LVCFMT_RIGHT, 70, ColumnType::Orb);
	cm->UpdateColumns();

	Rebuild();
	return 0;
}

LRESULT CSynastryView::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&) {
	m_List.RedrawWindow();
	return 0;
}

void CSynastryView::CreateFonts() {
	// the text font the user chose with Options > Font, or the system's
	UINT dpi = DpiHelper::GetDpi(m_hWnd);
	if (!m_Font.IsNull())
		m_Font.DeleteObject();
	if (!Helpers::UserTextFont(m_Font, dpi)) {
		LOGFONT lf;
		CFontHandle(AtlGetDefaultGuiFont()).GetLogFont(lf);
		lf.lfWeight = FW_NORMAL;
		lf.lfHeight = 90;
		Helpers::PointFontIndirect(m_Font, lf, dpi);
	}
	m_List.SetFont(m_Font);
}

void CSynastryView::OnDpiChanged(UINT) {
	CreateFonts();
	m_Strip.Update();
	AutoSizeColumns();
	UpdateLayout();
	m_List.RedrawWindow();
}

void CSynastryView::ReadCharts() {
	for (auto side : { &m_A, &m_B }) {
		if (!side->View)
			continue;
		OpenChart fresh;
		if (side->View->GetChart(fresh)) {
			side->Name = fresh.Name;
			side->Data = std::move(fresh.Data);
		}
	}
}

void CSynastryView::Rebuild() {
	m_Report = Synastry::Make(m_A.Data, m_B.Data, AspectOptions::Current().Transit);

	// the aspects of both kinds together, tightest first, then the two house overlays in the planets' order
	struct Item {
		float Orb;
		Row Row;
	};
	std::vector<Item> aspects;
	for (int i = 0; i < (int)m_Report.Aspects.size(); i++)
		aspects.push_back({ m_Report.Aspects[i].Orb, { RowKind::Aspect, i } });
	for (int i = 0; i < (int)m_Report.AToB.size(); i++)
		aspects.push_back({ m_Report.AToB[i].Orb, { RowKind::AToAngle, i } });
	for (int i = 0; i < (int)m_Report.BToA.size(); i++)
		aspects.push_back({ m_Report.BToA[i].Orb, { RowKind::BToAngle, i } });
	std::stable_sort(aspects.begin(), aspects.end(), [](auto const& x, auto const& y) { return x.Orb < y.Orb; });

	m_Rows.clear();
	for (auto const& item : aspects)
		m_Rows.push_back(item.Row);
	for (int i = 0; i < (int)m_Report.AInB.size(); i++)
		if (m_Report.AInB[i] > 0)
			m_Rows.push_back({ RowKind::AInB, i });
	for (int i = 0; i < (int)m_Report.BInA.size(); i++)
		if (m_Report.BInA[i] > 0)
			m_Rows.push_back({ RowKind::BInA, i });

	m_List.SetItemCountEx((int)m_Rows.size(), LVSICF_NOSCROLL);
	Sort(GetSortInfo(m_List));		// (in the order the user sorted by, if they did)
	UpdateHeaders();
	AutoSizeColumns();
	m_List.RedrawWindow();
}

void CSynastryView::UpdateHeaders() {
	auto cm = GetColumnManager(m_List);
	int count = m_List.GetHeader().GetItemCount();
	for (int i = 0; i < count; i++) {
		auto type = cm->GetColumnTag<ColumnType>(i);
		if (type != ColumnType::First && type != ColumnType::Second)
			continue;
		CString name = type == ColumnType::First ? m_A.Name : m_B.Name;
		LVCOLUMN column{ LVCF_TEXT };
		column.pszText = (PWSTR)(PCWSTR)name;
		m_List.SetColumn(i, &column);
	}
}

void CSynastryView::AutoSizeColumns() {
	// (LVSCW_AUTOSIZE doesn't see the rows of a virtual list)
	CClientDC dc(m_List);
	auto old = dc.SelectFont(m_Font);
	auto measure = [&](CString const& text) {
		CSize size;
		dc.GetTextExtent(text, text.GetLength(), &size);
		return (int)size.cx;
	};
	auto cm = GetColumnManager(m_List);
	m_List.SetRedraw(FALSE);
	int count = m_List.GetHeader().GetItemCount();
	for (int i = 0; i < count; i++) {
		auto type = cm->GetColumnTag<ColumnType>(i);
		WCHAR name[128]{};
		LVCOLUMN column{ LVCF_TEXT };
		column.pszText = name;
		column.cchTextMax = _countof(name);
		m_List.GetColumn(i, &column);
		int width = measure(name) + DpiHelper::Scale(12, m_List);		// (room for the sort arrow)
		for (auto const& row : m_Rows)
			width = std::max(width, measure(CellText(row, type)));
		Helpers::SetColumnWidth(m_List, i, width + DpiHelper::Scale(24, m_List));
	}
	dc.SelectFont(old);
	m_List.SetRedraw(TRUE);
}

CSynastryView::Side CSynastryView::SideOf(Row const& row, bool first) const {
	auto const& chart = first ? m_A.Data : m_B.Data;
	auto planet = [](PlanetPosition const& p) {
		return Side{ Helpers::GetPlanetName(p.Planet), p.Longitude, (double)p.Planet };
	};
	auto angle = [](HouseData const& houses, bool mc) {
		return Side{ AngleName(mc), mc ? houses.MC : houses.Asc, 100.0 + mc };
	};
	auto house = [](HouseData const& houses, int h) {
		return Side{ HouseName(h), houses.Cusps[h - 1], 200.0 + h };
	};
	switch (row.Kind) {
		case RowKind::Aspect: {
			auto const& aspect = m_Report.Aspects[row.Index];
			return planet(first ? aspect.Planet1 : aspect.Planet2);
		}
		case RowKind::AToAngle: {
			// A's planet, B's angle
			auto const& contact = m_Report.AToB[row.Index];
			return first ? planet(chart.AllPlanets()[contact.Planet]) : angle(chart.Houses(), contact.Midheaven);
		}
		case RowKind::BToAngle: {
			// A's angle, B's planet
			auto const& contact = m_Report.BToA[row.Index];
			return first ? angle(chart.Houses(), contact.Midheaven) : planet(chart.AllPlanets()[contact.Planet]);
		}
		case RowKind::AInB:
			return first ? planet(chart.AllPlanets()[row.Index]) : house(chart.Houses(), m_Report.AInB[row.Index]);
		case RowKind::BInA:
			return first ? house(chart.Houses(), m_Report.BInA[row.Index]) : planet(chart.AllPlanets()[row.Index]);
	}
	return {};
}

AspectType CSynastryView::AspectOf(Row const& row) const {
	switch (row.Kind) {
		case RowKind::Aspect: return m_Report.Aspects[row.Index].Type;
		case RowKind::AToAngle: return m_Report.AToB[row.Index].Type;
		case RowKind::BToAngle: return m_Report.BToA[row.Index].Type;
	}
	return AspectType::None;
}

float CSynastryView::OrbOf(Row const& row) const {
	switch (row.Kind) {
		case RowKind::Aspect: return m_Report.Aspects[row.Index].Orb;
		case RowKind::AToAngle: return m_Report.AToB[row.Index].Orb;
		case RowKind::BToAngle: return m_Report.BToA[row.Index].Orb;
	}
	return -1;
}

CString CSynastryView::CellText(Row const& row, ColumnType column) const {
	switch (column) {
		case ColumnType::First: return SideOf(row, true).Name;
		case ColumnType::FirstPosition: return Position(SideOf(row, true).Longitude);
		case ColumnType::Second: return SideOf(row, false).Name;
		case ColumnType::SecondPosition: return Position(SideOf(row, false).Longitude);
		case ColumnType::Contact:
			if (row.Kind == RowKind::AInB)
				return L"in";
			if (row.Kind == RowKind::BInA)
				return L"holds";
			return Helpers::GetAspectName(AspectOf(row));
		case ColumnType::Orb:
			if (auto orb = OrbOf(row); orb >= 0) {
				CString text;
				text.Format(L"%.2f%c", orb, 0xb0);
				return text;
			}
			return L"";
	}
	return L"";
}

CString CSynastryView::GetColumnText(HWND h, int row, int col) {
	if (row >= (int)m_Rows.size())
		return L"";
	return CellText(m_Rows[row], GetColumnManager(h)->GetColumnTag<ColumnType>(col));
}

void CSynastryView::DoSort(SortInfo const* si) {
	auto column = GetColumnManager(m_List)->GetColumnTag<ColumnType>(si->SortColumn);
	auto key = [&](Row const& row) -> double {
		switch (column) {
			case ColumnType::First: return SideOf(row, true).Order;
			case ColumnType::FirstPosition: return SideOf(row, true).Longitude.Value;
			case ColumnType::Second: return SideOf(row, false).Order;
			case ColumnType::SecondPosition: return SideOf(row, false).Longitude.Value;
			case ColumnType::Contact:
				return row.Kind == RowKind::AInB ? 100 : row.Kind == RowKind::BInA ? 101 : (double)AspectOf(row);
			case ColumnType::Orb: {
				auto orb = OrbOf(row);
				return orb < 0 ? 1000 : orb;		// (the house rows after the aspects)
			}
		}
		return 0;
	};
	std::stable_sort(m_Rows.begin(), m_Rows.end(), [&](Row const& x, Row const& y) {
		return SortHelper::Sort(key(x), key(y), si->SortAscending);
	});
}

DWORD CSynastryView::OnPrePaint(int, LPNMCUSTOMDRAW) {
	return CDRF_NOTIFYITEMDRAW;
}

DWORD CSynastryView::OnItemPrePaint(int, LPNMCUSTOMDRAW) {
	return CDRF_NOTIFYSUBITEMDRAW;
}

DWORD CSynastryView::OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) {
	auto lv = (NMLVCUSTOMDRAW*)cd;
	int index = (int)cd->dwItemSpec;
	const bool dark = WTLHelper::IsDarkMode();
	// (the colours are always set: left alone, the list's own are those of the light look)
	lv->clrTextBk = dark ? DarkMode::getViewBackgroundColor() : ::GetSysColor(COLOR_WINDOW);
	lv->clrText = dark ? DarkMode::getTextColor() : ::GetSysColor(COLOR_WINDOWTEXT);
	if (index < (int)m_Rows.size()) {
		// the colours of the analysis view: conjunctions green, squares and oppositions red, trines and sextiles blue
		switch (AspectOf(m_Rows[index])) {
			case AspectType::Conjunction: lv->clrTextBk = dark ? RGB(35, 85, 45) : RGB(198, 236, 198); break;
			case AspectType::Square:
			case AspectType::Opposition: lv->clrTextBk = dark ? RGB(100, 40, 40) : RGB(246, 200, 200); break;
			case AspectType::Trine:
			case AspectType::Sextile: lv->clrTextBk = dark ? RGB(40, 65, 115) : RGB(198, 214, 246); break;
		}
	}
	return CDRF_DODEFAULT;
}

void CSynastryView::PageActivated(bool active) {
	auto& ui = Frame()->GetUI();
	ui.UIEnable(ID_FILE_EXPORT, active);	// (the next page enables them again if it can)
	ui.UIEnable(ID_FILE_PRINT, active);
	ui.UIEnable(ID_FILE_PRINT_PREVIEW, active);
}

void CSynastryView::TextFontChanged() {
	CreateFonts();
	AutoSizeColumns();
	m_List.RedrawWindow();
}

void CSynastryView::AspectSettingsChanged() {
	Rebuild();
}

void CSynastryView::PartOfFortuneChanged() {
	// the charts' tabs change their planets too, perhaps after this one hears of it
	PostMessage(WM_REREAD);
}

void CSynastryView::ZodiacChanged() {
	int zodiac = AstroCalculator::DefaultZodiac();
	if (zodiac == m_Zodiac)
		return;
	// a copy whose tab is gone is moved to the other zodiac; the others are read again once their tabs have been worked out anew
	for (auto side : { &m_A, &m_B })
		if (!side->View)
			side->Data.ShiftLongitudes(AstroCalculator::ZodiacDifference(m_Zodiac, zodiac, side->Data.Info().Time.Julian()));
	m_Zodiac = zodiac;
	Rebuild();
	PostMessage(WM_REREAD);
}

LRESULT CSynastryView::OnReread(UINT, WPARAM, LPARAM, BOOL&) {
	ReadCharts();
	Rebuild();
	return 0;
}

void CSynastryView::SourceChartChanged(IView* chart, bool) {
	if (!chart || (chart != m_A.View && chart != m_B.View))
		return;
	ReadCharts();
	Frame()->SetViewTitle(this, Title());
	Rebuild();
}

void CSynastryView::SourceChartClosing(IView* chart) {
	// the copy stays as it was
	for (auto side : { &m_A, &m_B })
		if (side->View == chart)
			side->View = nullptr;
}

bool CSynastryView::GetStatusInfo(StatusInfo& info) const {
	info.Name = Title();
	int aspects = (int)(m_Report.Aspects.size() + m_Report.AToB.size() + m_Report.BToA.size());
	info.Details.Format(L"%d aspects", aspects);
	if (Zodiac::IsValid(m_Zodiac))
		info.Details += L"  |  " + Helpers::ZodiacText(m_Zodiac);
	return true;
}

LRESULT CSynastryView::OnSwap(WORD, WORD, HWND, BOOL&) {
	std::swap(m_A, m_B);
	Frame()->SetViewTitle(this, Title());
	Rebuild();
	return 0;
}

Helpers::TableSource CSynastryView::Table() const {
	Helpers::TableSource table;
	table.Headers = { m_A.Name, L"Position", L"Contact", m_B.Name, L"Position", L"Orb" };
	table.Rows = (int)m_Rows.size();
	table.Cell = [this](int row, int column) {
		return CellText(m_Rows[row], static_cast<ColumnType>(column));
	};
	return table;
}

LRESULT CSynastryView::OnEditCopy(WORD, WORD, HWND, BOOL&) {
	Helpers::CopyListRows(m_hWnd, m_List, Table());
	return 0;
}

LRESULT CSynastryView::OnExport(WORD, WORD, HWND, BOOL&) {
	static constexpr wchar_t filter[] = L"CSV files (*.csv)\0*.csv\0All files (*.*)\0*.*\0";
	CSimpleFileDialog dlg(FALSE, L"csv", L"Synastry", OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING, filter, m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (ok)
		Helpers::SaveTable(m_hWnd, Table(), dlg.m_szFileName, L"The synastry report");
	return 0;
}

LRESULT CSynastryView::OnPrint(WORD, WORD id, HWND, BOOL&) {
	Printing::TableDocument document(Title(), L"Aspects with the transit / overlay orbs (Options > Aspects)", Table());
	if (id == ID_FILE_PRINT_PREVIEW)
		Printing::Preview(m_hWnd, document);
	else
		Printing::Print(m_hWnd, document);
	return 0;
}
