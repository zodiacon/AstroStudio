#include "pch.h"
#include "AnalysisView.h"
#include "AnalysisDlg.h"
#include "AnalysisNames.h"
#include "AppSettings.h"
#include "DefaultFont.h"
#include "Helpers.h"
#include "TimeZones.h"
#include "SortHelper.h"
#include <DarkMode/DarkModeSubclass.h>
#include <ToolbarHelper.h>
#include <algorithm>

namespace {
	PCWSTR const ColumnNames[] = { L"Date", L"Analysis", L"Event", L"Mover", L"Aspect", L"Target", L"Orb", L"Pass", L"Position" };
	// where the columns that change their headers with the glyphs are (the order they were added in)
	constexpr int AnalysisColumn = 1, MoverColumn = 3, TargetColumn = 5;
}

LRESULT CAnalysisView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_Glyphs = AppSettings::Get().AnalysisGlyphs() != 0;

	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_OWNERDATA | LVS_REPORT | LVS_SHOWSELALWAYS);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
	CreateFonts();

	ToolBarButtonInfo buttons[] = {
		{ ID_VIEW_GLYPHS, IDI_GLYPH, BTNS_CHECK, L"Glyphs" },
		{ 0 },
		{ ID_ANALYSIS_OPTIONS, IDI_OPTIONS, 0, L"Options" },
		{ ID_ANALYSIS_REFRESH, IDI_CLOCK_REFRESH, 0, L"Refresh" },
	};
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	auto tb = ToolbarHelper::CreateAndInitToolBar(m_hWndToolBar, buttons, _countof(buttons), 16);
	AddSimpleReBarBand(tb);
	Frame()->AddToolBarToUI(tb);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(ColumnNames[0], LVCFMT_LEFT, 140, ColumnType::Date);
	cm->AddColumn(ColumnNames[1], LVCFMT_LEFT, 160, ColumnType::Analysis);
	cm->AddColumn(ColumnNames[2], LVCFMT_LEFT, 140, ColumnType::Event);
	cm->AddColumn(ColumnNames[3], LVCFMT_LEFT, 130, ColumnType::Mover);
	cm->AddColumn(ColumnNames[4], LVCFMT_LEFT, 110, ColumnType::Aspect);
	cm->AddColumn(ColumnNames[5], LVCFMT_LEFT, 150, ColumnType::Target);
	cm->AddColumn(ColumnNames[6], LVCFMT_RIGHT, 80, ColumnType::Orb);
	cm->AddColumn(ColumnNames[7], LVCFMT_CENTER, 50, ColumnType::Pass);
	cm->AddColumn(ColumnNames[8], LVCFMT_LEFT, 130, ColumnType::Longitude);
	cm->UpdateColumns();
	UpdateHeaders();

	DarkMode::setDarkWndNotifySafe(m_hWnd);
	return 0;
}

void CAnalysisView::CreateFonts() {
	if (m_SymbolFont)
		m_SymbolFont.DeleteObject();
	LOGFONT lf;
	CFontHandle(m_List.GetFont()).GetLogFont(lf);
	wcscpy_s(lf.lfFaceName, L"HamburgSymbols");
	m_SymbolFont.CreateFontIndirect(&lf);
}

void CAnalysisView::UpdateViewUI() {
	Frame()->GetUI().UISetCheck(ID_VIEW_GLYPHS, m_Glyphs);
}

void CAnalysisView::PageActivated(bool active) {
	auto& ui = Frame()->GetUI();
	if (active) {
		ui.UIEnable(ID_FILE_EXPORT, TRUE);
		UpdateViewUI();
	}
	else
		ui.UIEnable(ID_FILE_EXPORT, FALSE);		// the next page enables it again if it can export
}

LRESULT CAnalysisView::OnViewGlyphs(WORD, WORD, HWND, BOOL&) {
	m_Glyphs = !m_Glyphs;
	AppSettings::Get().AnalysisGlyphs(m_Glyphs ? 1 : 0);
	UpdateViewUI();
	UpdateHeaders();
	m_List.Invalidate();
	return 0;
}

LRESULT CAnalysisView::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&) {
	m_List.RedrawWindow();
	return 0;
}

void CAnalysisView::UpdateHeaders() {
	// With glyphs a cell has only the glyph, so what the planets are - transits, progressed, natal - is said once, in the header.
	// (When several analyses are listed together it is said by the Analysis column, which is shown only then.)
	auto types = m_Settings.TypeList();
	bool several = types.size() > 1;
	CString mover = ColumnNames[MoverColumn], target = ColumnNames[TargetColumn];
	if (m_Glyphs && !several) {
		mover = AnalysisNames::MoverLabel(types[0]);
		target = AnalysisNames::TargetLabel(types[0]);
		target.SetAt(0, static_cast<WCHAR>(towupper(target[0])));
	}
	m_List.SetColumnWidth(AnalysisColumn, several ? 170 : 0);
	auto set = [&](int index, CString const& text) {
		LVCOLUMN column{ LVCF_TEXT };
		column.pszText = const_cast<LPWSTR>((PCWSTR)text);
		m_List.SetColumn(index, &column);
	};
	set(MoverColumn, mover);
	set(TargetColumn, target);
}

void CAnalysisView::Analyse(OpenChart chart, AnalysisSettings const& settings) {
	m_Chart = std::move(chart);
	m_Settings = settings;
	Run();
}

void CAnalysisView::Run() {
	{
		CWaitCursor wait;
		m_Events = Analysis::RunAll(m_Calc, m_Chart.Data, m_Settings).Events;
		m_List.SetItemCount(static_cast<int>(m_Events.size()));
		m_List.Invalidate();
	}
	UpdateHeaders();
	UpdateTitle();
}

void CAnalysisView::UpdateTitle() {
	CString title, what;
	auto types = m_Settings.TypeList();
	if (types.size() == 1)
		what = AnalysisNames::TypeName(types[0]);
	else
		what.Format(L"%d analyses", static_cast<int>(types.size()));
	title.Format(L"%s: %s (%d)", (PCWSTR)what, (PCWSTR)m_Chart.Name, static_cast<int>(m_Events.size()));
	Frame()->SetViewTitle(this, title);
}

CString CAnalysisView::EventText(AnalysisEvent const& event) const {
	CString text;
	switch (event.Kind) {
		case AnalysisEventKind::EnterOrb: return L"Enters orb";
		case AnalysisEventKind::Exact: return L"Exact";
		case AnalysisEventKind::LeaveOrb: return L"Leaves orb";
		case AnalysisEventKind::InOrbAtStart: return L"In orb at start";
		case AnalysisEventKind::InOrbAtEnd: return L"In orb at end";
		case AnalysisEventKind::HouseIngress: text.Format(L"Enters house %d", event.Index); return text;
		case AnalysisEventKind::SignIngress: text.Format(L"Enters %s", (PCWSTR)Helpers::GetZodiacSignName(static_cast<ZodiacSign>(event.Index))); return text;
		case AnalysisEventKind::StationRetrograde: return L"Stations retrograde";
		case AnalysisEventKind::StationDirect: return L"Stations direct";
	}
	return text;
}

bool CAnalysisView::IsGlyphColumn(ColumnType column) {
	return column == ColumnType::Mover || column == ColumnType::Aspect || column == ColumnType::Target || column == ColumnType::Longitude;
}

COLORREF CAnalysisView::RowColor(AnalysisEvent const& event) {
	bool dark = WTLHelper::IsDarkMode();
	const COLORREF green = dark ? RGB(35, 85, 45) : RGB(198, 236, 198);
	const COLORREF red = dark ? RGB(100, 40, 40) : RGB(246, 200, 200);
	const COLORREF blue = dark ? RGB(40, 65, 115) : RGB(198, 214, 246);
	const COLORREF purple = dark ? RGB(85, 50, 125) : RGB(226, 206, 246);
	if (event.Kind == AnalysisEventKind::HouseIngress)
		return purple;
	switch (event.Aspect) {
		case AspectType::Conjunction: return green;
		case AspectType::Square:
		case AspectType::Opposition: return red;
		case AspectType::Trine:
		case AspectType::Sextile: return blue;
	}
	return CLR_INVALID;
}

CString CAnalysisView::CellText(AnalysisEvent const& event, ColumnType column, bool glyphs) const {
	CString text;
	bool aspect = event.Aspect != AspectType::None;
	switch (column) {
		case ColumnType::Date: {
			// as the chart's own time is shown: in its zone
			auto const& zone = m_Chart.Data.Info().TimeZone;
			int offset = zone.OffsetUT;
			auto local = TimeZones::UtToLocal(event.Time, zone, &offset);
			text.Format(L"%04ld/%02ld/%02ld  %02ld:%02ld", local.Year, local.Month, local.Day, local.Hour, local.Minute);
			return text;
		}
		case ColumnType::Analysis:
			return AnalysisNames::TypeName(event.Type);
		case ColumnType::Event:
			return EventText(event);
		case ColumnType::Mover:
			if (glyphs)
				return DefaultFont::Get().GetPlanetGlyphAsString(event.Mover);
			text.Format(L"%s %s", AnalysisNames::MoverLabel(event.Type), Helpers::GetPlanetName(event.Mover));
			return text;
		case ColumnType::Aspect:
			if (!aspect)
				return text;
			return glyphs ? DefaultFont::Get().GetAspectGlyphAsString(event.Aspect) : CString(Helpers::GetAspectName(event.Aspect));
		case ColumnType::Target:
			if (!aspect)
				return text;
			if (glyphs) {
				// (Z and X are the Ascendant and the Midheaven in the glyph font)
				if (event.TargetKind == AnalysisTarget::Planet)
					return DefaultFont::Get().GetPlanetGlyphAsString(event.Target);
				return event.TargetKind == AnalysisTarget::Ascendant ? L"Z" : L"X";
			}
			if (event.TargetKind == AnalysisTarget::Planet)
				text.Format(L"%s %s", AnalysisNames::TargetLabel(event.Type), Helpers::GetPlanetName(event.Target));
			else
				text.Format(L"natal %s", event.TargetKind == AnalysisTarget::Ascendant ? L"Ascendant" : L"Midheaven");
			return text;
		case ColumnType::Orb:
			// the orb an aspect is judged by, for the moments it comes in and goes out
			if (aspect && event.Kind != AnalysisEventKind::Exact)
				text.Format(L"%.2f%c", event.Orb, 0xb0);		// (as the aspect list does: the degree sign)
			return text;
		case ColumnType::Pass:
			if (aspect)
				text.Format(L"%d", event.Pass);
			return text;
		case ColumnType::Longitude: {
			// where the mover is, with the retrograde mark after it while it goes backward
			AstroPoint position(event.Longitude);
			if (event.Retrograde)
				position.Flags |= AstroPointFlags::Retro;
			auto options = FormatOptions::ShowDegreeGlyph;
			if (glyphs)
				options |= FormatOptions::UseGlyphs;
			return Helpers::FormatLongitude(position, options);
		}
	}
	return text;
}

CString CAnalysisView::GetColumnText(HWND h, int row, int col) const {
	if (row < 0 || row >= static_cast<int>(m_Events.size()))
		return CString();
	return CellText(m_Events[row], GetColumnManager(h)->GetColumnTag<ColumnType>(col), m_Glyphs);
}

DWORD CAnalysisView::OnPrePaint(int, LPNMCUSTOMDRAW) {
	return CDRF_NOTIFYITEMDRAW;
}

DWORD CAnalysisView::OnItemPrePaint(int, LPNMCUSTOMDRAW) {
	return CDRF_NOTIFYSUBITEMDRAW;
}

DWORD CAnalysisView::OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) {
	auto lv = reinterpret_cast<LPNMLVCUSTOMDRAW>(cd);
	int row = static_cast<int>(cd->dwItemSpec);
	if (row < 0 || row >= static_cast<int>(m_Events.size()))
		return CDRF_DODEFAULT;

	lv->clrTextBk = RowColor(m_Events[row]);
	lv->clrText = WTLHelper::IsDarkMode() ? RGB(235, 235, 235) : RGB(0, 0, 0);		// (without it the date cell comes out in a colour of its own)
	auto column = GetColumnManager(m_List)->GetColumnTag<ColumnType>(lv->iSubItem);
	::SelectObject(cd->hdc, m_Glyphs && IsGlyphColumn(column) ? m_SymbolFont.m_hFont : m_List.GetFont());
	return CDRF_NEWFONT | CDRF_SKIPPOSTPAINT;
}

void CAnalysisView::DoSort(SortInfo const* si) {
	auto column = GetColumnManager(m_List)->GetColumnTag<ColumnType>(si->SortColumn);
	auto compare = [&](AnalysisEvent const& a, AnalysisEvent const& b) {
		switch (column) {
			case ColumnType::Date: return SortHelper::Sort(a.Time.Julian(), b.Time.Julian(), si->SortAscending);
			case ColumnType::Analysis: return SortHelper::Sort(static_cast<int>(a.Type), static_cast<int>(b.Type), si->SortAscending);
			case ColumnType::Event: return SortHelper::Sort(static_cast<int>(a.Kind), static_cast<int>(b.Kind), si->SortAscending);
			case ColumnType::Mover: return SortHelper::Sort(static_cast<int>(a.Mover), static_cast<int>(b.Mover), si->SortAscending);
			case ColumnType::Aspect: return SortHelper::Sort(static_cast<int>(a.Aspect), static_cast<int>(b.Aspect), si->SortAscending);
			case ColumnType::Target:
				return SortHelper::Sort(static_cast<int>(a.TargetKind) * 100 + static_cast<int>(a.Target), static_cast<int>(b.TargetKind) * 100 + static_cast<int>(b.Target), si->SortAscending);
			case ColumnType::Orb: return SortHelper::Sort(a.Orb, b.Orb, si->SortAscending);
			case ColumnType::Pass: return SortHelper::Sort(a.Pass, b.Pass, si->SortAscending);
			case ColumnType::Longitude: return SortHelper::Sort(a.Longitude, b.Longitude, si->SortAscending);
		}
		return false;
	};
	std::stable_sort(m_Events.begin(), m_Events.end(), compare);
}

LRESULT CAnalysisView::OnOptions(WORD, WORD, HWND, BOOL&) {
	// the charts open now; the one the tab was made from is among them unless it has been closed since
	auto charts = Frame()->OpenCharts();
	int index = -1;
	for (int i = 0; i < static_cast<int>(charts.size()); i++)
		if (m_Chart.View && charts[i].View == m_Chart.View)
			index = i;
	bool closed = index < 0;
	if (closed) {
		OpenChart kept = m_Chart;
		kept.Name += L" (closed)";
		charts.insert(charts.begin(), std::move(kept));
		index = 0;
	}

	CAnalysisDlg dlg;
	dlg.Init(&charts, index, m_Settings);
	if (dlg.DoModal(m_hWnd) != IDOK)
		return 0;

	// (choosing the closed chart again keeps the copy that was made of it)
	if (!(closed && dlg.Chart() == 0))
		m_Chart = charts[dlg.Chart()];
	m_Settings = dlg.Settings();
	Run();
	return 0;
}

LRESULT CAnalysisView::OnRefresh(WORD, WORD, HWND, BOOL&) {
	// the chart as it is now, if it is still open
	if (m_Chart.View)
		for (auto& chart : Frame()->OpenCharts())
			if (chart.View == m_Chart.View) {
				m_Chart = std::move(chart);
				break;
			}
	Run();
	return 0;
}

CString CAnalysisView::BuildTable(std::vector<int> const& rows, bool csv) const {
	auto quote = [&](CString text) {
		if (!csv)
			return text;
		text.Replace(L"\"", L"\"\"");
		return CString(L"\"") + text + L"\"";
	};
	PCWSTR separator = csv ? L"," : L"\t";
	CString table;
	for (int column = 0; column < _countof(ColumnNames); column++) {
		if (column)
			table += separator;
		table += quote(ColumnNames[column]);
	}
	table += L"\r\n";
	for (int row : rows) {
		if (row < 0 || row >= static_cast<int>(m_Events.size()))
			continue;
		for (int column = 0; column < _countof(ColumnNames); column++) {
			if (column)
				table += separator;
			// plain words, whatever the list shows
			table += quote(CellText(m_Events[row], static_cast<ColumnType>(column), false));
		}
		table += L"\r\n";
	}
	return table;
}

LRESULT CAnalysisView::OnEditCopy(WORD, WORD, HWND, BOOL&) {
	// the selected rows, as text for a spreadsheet or a text editor
	std::vector<int> rows;
	for (int row = m_List.GetNextItem(-1, LVNI_SELECTED); row >= 0; row = m_List.GetNextItem(row, LVNI_SELECTED))
		rows.push_back(row);
	if (rows.empty())
		return 0;
	if (!Helpers::CopyTextToClipboard(m_hWnd, BuildTable(rows, false)))
		AtlMessageBox(m_hWnd, L"The rows could not be copied to the clipboard.", L"Astro Studio", MB_ICONWARNING);
	return 0;
}

LRESULT CAnalysisView::OnExport(WORD, WORD, HWND, BOOL&) {
	static constexpr wchar_t filter[] = L"CSV files (*.csv)\0*.csv\0All files (*.*)\0*.*\0";
	CSimpleFileDialog dlg(FALSE, L"csv", L"Analysis", OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING, filter, m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (!ok)
		return 0;

	CWaitCursor wait;
	std::vector<int> rows(m_Events.size());
	for (int i = 0; i < static_cast<int>(rows.size()); i++)
		rows[i] = i;
	Helpers::SaveTextFileUtf8(m_hWnd, dlg.m_szFileName, BuildTable(rows, true), L"The events");
	return 0;
}
