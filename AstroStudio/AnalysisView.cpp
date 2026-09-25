#include "pch.h"
#include "AnalysisView.h"
#include "Printing.h"
#include "AnalysisDlg.h"
#include "AnalysisNames.h"
#include "AppSettings.h"
#include "AspectOptions.h"
#include "DefaultFont.h"
#include "Helpers.h"
#include "TimeZones.h"
#include "SortHelper.h"
#include <DarkMode/DarkModeSubclass.h>
#include <ToolbarHelper.h>
#include <algorithm>

namespace {
	PCWSTR const ColumnNames[] = { L"Date", L"Analysis", L"Event", L"Mover", L"Aspect", L"Target", L"Orb", L"Pass", L"Position", L"Enter / Exact / Leave" };
	// where the columns that change their headers with the glyphs are (the order they were added in)
	constexpr int AnalysisColumn = 1, MoverColumn = 3, AspectColumn = 4, TargetColumn = 5;
}

LRESULT CAnalysisView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_Glyphs = AppSettings::Get().AnalysisGlyphs() != 0;
	m_FontSize = std::clamp(AppSettings::Get().AnalysisFontSize(), 70, 180);

	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_OWNERDATA | LVS_REPORT | LVS_SHOWSELALWAYS);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
	CreateFonts();
	m_List.SetFont(m_StdFont);

	ToolBarButtonInfo buttons[] = {
		{ ID_VIEW_GLYPHS, IDI_GLYPH, BTNS_CHECK, L"Glyphs" },
		{ ID_FONT_BIGGER, IDI_FONT_BIGGER },
		{ ID_FONT_SMALLER, IDI_FONT_SMALLER },
		{ ID_FONT_SIZE_DEFAULT, IDI_FONT_SIZE_DEFAULT },
		{ 0 },
		{ ID_ANALYSIS_OPTIONS, IDI_OPTIONS, 0, L"Options" },
		{ ID_ANALYSIS_REFRESH, IDI_CLOCK_REFRESH, 0, L"Refresh" },
		{ ID_ANALYSIS_CANCEL, IDI_STOP, 0, L"Cancel" },
	};
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	auto tb = ToolbarHelper::CreateAndInitToolBar(m_hWndToolBar, buttons, _countof(buttons), 16);
	// a box for filtering the events sits in the toolbar, on a separator made as wide as it is (created before the toolbar joins the
	// rebar, which sizes the band from its buttons)
	CToolBarCtrl bar(tb);
	int dpi = CClientDC(m_hWnd).GetDeviceCaps(LOGPIXELSX);
	auto px = [&](int value) { return MulDiv(value, dpi, 96); };
	bar.AddSeparator(px(8));
	int filterSlot = bar.GetButtonCount();
	bar.AddSeparator(px(190));
	AddSimpleReBarBand(tb);
	Frame()->AddToolBarToUI(tb);

	CRect item;
	bar.GetItemRect(filterSlot, &item);
	int height = px(22), top = item.top + (item.Height() - height) / 2;
	CRect box(item.left, top, item.right - px(4), top + height);
	m_Filter.Create(tb, box, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, IDC_AN_FILTER);
	m_Filter.SetFont(AtlGetDefaultGuiFont());
	m_Filter.SetCueBannerText(L"Filter: square saturn", TRUE);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(ColumnNames[0], LVCFMT_LEFT, 140, ColumnType::Date);
	cm->AddColumn(ColumnNames[1], LVCFMT_LEFT, 160, ColumnType::Analysis);
	cm->AddColumn(ColumnNames[2], LVCFMT_LEFT, 140, ColumnType::Event);
	cm->AddColumn(ColumnNames[3], LVCFMT_CENTER, 130, ColumnType::Mover);
	cm->AddColumn(ColumnNames[4], LVCFMT_CENTER, 110, ColumnType::Aspect);
	cm->AddColumn(ColumnNames[5], LVCFMT_CENTER, 150, ColumnType::Target);
	cm->AddColumn(ColumnNames[6], LVCFMT_RIGHT, 80, ColumnType::Orb);
	cm->AddColumn(ColumnNames[7], LVCFMT_CENTER, 50, ColumnType::Pass);
	cm->AddColumn(ColumnNames[8], LVCFMT_LEFT, 130, ColumnType::Longitude);
	cm->AddColumn(ColumnNames[9], LVCFMT_LEFT, 380, ColumnType::Stay);
	cm->UpdateColumns();
	UpdateHeaders(true);

	// where the progress of a run shows
	CreateSimpleStatusBar(L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	ShowRunning(false);

	DarkMode::setDarkWndNotifySafe(m_hWnd);
	return 0;
}

void CAnalysisView::CreateFonts() {
	if (m_SymbolFont)
		m_SymbolFont.DeleteObject();
	m_SymbolFont.CreatePointFont(m_FontSize, L"HamburgSymbols");

	// the text font the user chose with Options > Font, or the system's; its size is the toolbar's
	if (m_StdFont)
		m_StdFont.DeleteObject();
	LOGFONT lf = AppSettings::Get().TextFont();
	if (lf.lfFaceName[0] == 0) {
		CFontHandle(AtlGetDefaultGuiFont()).GetLogFont(lf);
		lf.lfWeight = FW_NORMAL;
	}
	lf.lfHeight = m_FontSize;
	m_StdFont.CreatePointFontIndirect(&lf);
}

void CAnalysisView::ApplyFontSize(int oldSize) {
	AppSettings::Get().AnalysisFontSize(m_FontSize);
	CreateFonts();
	m_List.SetFont(m_StdFont);
	// the columns grow and shrink with the text
	for (int column = 0; column < _countof(ColumnNames); column++)
		if (int width = m_List.GetColumnWidth(column); width > 0 && oldSize > 0)
			m_List.SetColumnWidth(column, width * m_FontSize / oldSize);
	UpdateViewUI();
	m_List.Invalidate();
}

LRESULT CAnalysisView::OnChangeFontSize(WORD, WORD id, HWND, BOOL&) {
	int old = m_FontSize;
	if (id == ID_FONT_SIZE_DEFAULT)
		m_FontSize = 90;
	else
		m_FontSize = std::clamp(m_FontSize + (id == ID_FONT_BIGGER ? 8 : -8), 70, 180);
	ApplyFontSize(old);
	return 0;
}

void CAnalysisView::TextFontChanged() {
	// the size of the chosen font becomes the list's
	int old = m_FontSize;
	if (int size = AppSettings::Get().TextFont().lfHeight; size > 0)
		m_FontSize = std::clamp(size, 70, 180);
	ApplyFontSize(old);
}

void CAnalysisView::UpdateViewUI() {
	auto& ui = Frame()->GetUI();
	ui.UISetCheck(ID_VIEW_GLYPHS, m_Glyphs);
	ui.UIEnable(ID_FONT_BIGGER, m_FontSize < 180);
	ui.UIEnable(ID_FONT_SMALLER, m_FontSize > 70);
}

void CAnalysisView::PageActivated(bool active) {
	m_PageActive = active;
	auto& ui = Frame()->GetUI();
	if (active) {
		ui.UIEnable(ID_FILE_EXPORT, TRUE);
		ui.UIEnable(ID_FILE_PRINT, TRUE);
		ui.UIEnable(ID_FILE_PRINT_PREVIEW, TRUE);
		UpdateViewUI();
		ShowRunning(m_Job != nullptr);
	}
	else {
		ui.UIEnable(ID_FILE_EXPORT, FALSE);		// the next page enables it again if it can export
		ui.UIEnable(ID_FILE_PRINT, FALSE);
		ui.UIEnable(ID_FILE_PRINT_PREVIEW, FALSE);
	}
}

LRESULT CAnalysisView::OnViewGlyphs(WORD, WORD, HWND, BOOL&) {
	m_Glyphs = !m_Glyphs;
	AppSettings::Get().AnalysisGlyphs(m_Glyphs ? 1 : 0);
	UpdateViewUI();
	UpdateHeaders(true);
	m_List.Invalidate();
	return 0;
}

LRESULT CAnalysisView::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&) {
	m_List.RedrawWindow();
	return 0;
}

void CAnalysisView::UpdateHeaders(bool resetWidths) {
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

	// A glyph is narrow, so with glyphs the three columns need only what their headers do; with words they need room for the names.
	struct Sizing { int Column; CString const& Header; int Words; };
	CString aspect = ColumnNames[AspectColumn];
	for (auto const& sizing : { Sizing{ MoverColumn, mover, 130 }, Sizing{ AspectColumn, aspect, 110 }, Sizing{ TargetColumn, target, 150 } }) {
		if (m_Glyphs) {
			int needed = std::max(44, m_List.GetStringWidth(sizing.Header) + 28);
			if (resetWidths || m_List.GetColumnWidth(sizing.Column) < needed)
				m_List.SetColumnWidth(sizing.Column, needed);
		}
		else if (resetWidths)
			m_List.SetColumnWidth(sizing.Column, sizing.Words);
	}
}

void CAnalysisView::Analyse(OpenChart chart, AnalysisSettings const& settings) {
	Frame()->SetViewTitle(this, L"Analysing...");
	Start(std::move(chart), settings);
}

void CAnalysisView::SetStatus(PCWSTR text) {
	if (m_hWndStatusBar)
		CStatusBarCtrl(m_hWndStatusBar).SetText(0, text);
}

void CAnalysisView::ShowRunning(bool running) {
	if (running)
		SetTimer(ProgressTimer, 150);
	else
		KillTimer(ProgressTimer);
	if (m_PageActive)
		Frame()->GetUI().UIEnable(ID_ANALYSIS_CANCEL, running);
	if (running)
		SetStatus(L"Analysing...");
}

void CAnalysisView::StopWorker() {
	if (m_Job)
		m_Job->Cancel = true;
	if (m_Worker.joinable())
		m_Worker.join();		// (it looks at the flag every few thousand steps, so this is quick)
	m_Job.reset();
}

void CAnalysisView::Start(OpenChart chart, AnalysisSettings settings) {
	StopWorker();
	auto job = std::make_shared<Job>();
	job->Chart = std::move(chart);
	job->Settings = std::move(settings);
	m_Job = job;
	WPARAM id = ++m_JobId;
	m_Worker = std::thread([job, id, hwnd = m_hWnd] {
		// the ephemeris keeps its state per thread, so this calculator is the thread's own
		AstroCalculator calc;
		job->Result = Analysis::RunAll(calc, job->Chart.Data, job->Settings, [&](double fraction) {
			job->Percent = static_cast<int>(fraction * 100);
			return !job->Cancel;
		});
		::PostMessage(hwnd, WM_ANALYSIS_DONE, id, 0);
	});
	ShowRunning(true);
}

LRESULT CAnalysisView::OnTimer(UINT, WPARAM id, LPARAM, BOOL& handled) {
	if (id == FilterTimer) {
		KillTimer(FilterTimer);
		ApplyFilter();
		return 0;
	}
	if (id != ProgressTimer) {
		handled = FALSE;
		return 0;
	}
	if (m_Job) {
		CString text;
		text.Format(L"Analysing...  %d%%", m_Job->Percent.load());
		SetStatus(text);
	}
	return 0;
}

LRESULT CAnalysisView::OnDone(UINT, WPARAM id, LPARAM, BOOL&) {
	if (!m_Job || id != m_JobId)
		return 0;		// the message of a run that was replaced
	m_Worker.join();
	auto job = std::move(m_Job);
	m_Job.reset();
	ShowRunning(false);

	if (job->Result.Cancelled) {
		SetStatus(L"Cancelled");
		if (m_Events.empty() && m_Chart.Name.IsEmpty())
			Frame()->SetViewTitle(this, L"Analysis (cancelled)");
		return 0;
	}
	m_Chart = std::move(job->Chart);
	m_Settings = std::move(job->Settings);
	m_Events = std::move(job->Result.Events);
	m_Stale = false;
	ApplyFilter();
	UpdateHeaders(false);
	UpdateTitle();
	return 0;
}

LRESULT CAnalysisView::OnCancel(WORD, WORD, HWND, BOOL&) {
	if (m_Job)
		m_Job->Cancel = true;		// OnDone hears of it
	return 0;
}

LRESULT CAnalysisView::OnFilterChanged(WORD, WORD, HWND, BOOL&) {
	// as the user types: a moment after the last key
	SetTimer(FilterTimer, 250);
	return 0;
}

LRESULT CAnalysisView::OnDoubleClick(int, LPNMHDR pnmh, BOOL& handled) {
	if (pnmh->hwndFrom != m_List) {
		handled = FALSE;
		return 0;
	}
	int row = m_List.GetNextItem(-1, LVNI_SELECTED);
	auto event = EventAt(row);
	if (!event)
		return 0;

	MomentKind kind = MomentKind::Transits;
	if (event->Type == AnalysisType::SolarArcToNatal)
		kind = MomentKind::SolarArc;
	else if (event->Type == AnalysisType::ProgressedToNatal || event->Type == AnalysisType::ProgressedToProgressed)
		kind = MomentKind::Progressions;

	// the chart's own tab if it is still open, otherwise a new tab with the copy this one was made from
	IView* chart = nullptr;
	if (m_Chart.View)
		for (auto const& open : Frame()->OpenCharts())
			if (open.View == m_Chart.View)
				chart = open.View;
	if (!chart) {
		chart = Frame()->AddChartView(m_Chart.Data, m_Chart.Name);
		m_Chart.View = chart;
	}
	Frame()->ActivateView(chart);
	if (!chart->ShowMoment(event->Time, kind))
		AtlMessageBox(m_hWnd, L"That chart can't show this moment around it.", L"Astro Studio", MB_ICONINFORMATION);
	return 0;
}

void CAnalysisView::AspectSettingsChanged() {
	if (m_Settings.AspectEvents && !m_Events.empty()) {
		m_Stale = true;
		ShowCount();
	}
}

LRESULT CAnalysisView::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
	StopWorker();
	handled = FALSE;
	return 0;
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

AnalysisEvent const* CAnalysisView::EventAt(int row) const {
	if (row < 0 || row >= static_cast<int>(m_Shown.size()))
		return nullptr;
	return &m_Events[m_Shown[row]];
}

bool CAnalysisView::Matches(AnalysisEvent const& event) const {
	if (m_Terms.empty())
		return true;
	// everything a row says, in words
	CString text;
	for (auto column : { ColumnType::Date, ColumnType::Analysis, ColumnType::Event, ColumnType::Mover, ColumnType::Aspect, ColumnType::Target, ColumnType::Longitude })
		text += CellText(event, column, false) + L" ";
	text.MakeLower();
	for (auto const& term : m_Terms)
		if (text.Find(term) < 0)
			return false;
	return true;
}

void CAnalysisView::ApplyFilter() {
	CString filter;
	if (m_Filter)
		m_Filter.GetWindowText(filter);
	filter.MakeLower();
	m_Terms.clear();
	int position = 0;
	for (auto term = filter.Tokenize(L" \t", position); position >= 0; term = filter.Tokenize(L" \t", position))
		if (!term.IsEmpty())
			m_Terms.push_back(term);

	m_Shown.clear();
	for (int i = 0; i < static_cast<int>(m_Events.size()); i++)
		if (Matches(m_Events[i]))
			m_Shown.push_back(i);
	m_List.SetItemCount(static_cast<int>(m_Shown.size()));
	m_List.Invalidate();
	ShowCount();
}

void CAnalysisView::ShowCount() {
	CString status;
	if (m_Terms.empty())
		status.Format(L"%d events", static_cast<int>(m_Events.size()));
	else
		status.Format(L"%d of %d events", static_cast<int>(m_Shown.size()), static_cast<int>(m_Events.size()));
	if (m_Stale)
		status += L"     The aspect settings have changed since: Refresh runs it again with them";
	SetStatus(status);
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

CString CAnalysisView::LocalText(DateTime const& ut, bool withTime) const {
	auto const& zone = m_Chart.Data.Info().TimeZone;
	int offset = zone.OffsetUT;
	auto local = TimeZones::UtToLocal(ut, zone, &offset);
	CString text;
	if (withTime)
		text.Format(L"%04ld/%02ld/%02ld  %02ld:%02ld", local.Year, local.Month, local.Day, local.Hour, local.Minute);
	else
		text.Format(L"%04ld/%02ld/%02ld", local.Year, local.Month, local.Day);
	return text;
}

CString CAnalysisView::StayText(AnalysisEvent const& event) const {
	if (!event.Window)
		return CString();
	auto const& stay = *event.Window;
	// dates are enough for a stay of weeks or years; a short one needs its hours
	double first = stay.HasEnter ? stay.Enter.Julian() : (stay.Exacts.empty() ? stay.Leave.Julian() : stay.Exacts.front().Julian());
	bool withTime = stay.Leave.Julian() - first <= 14;

	CString exacts;
	for (auto const& exact : stay.Exacts)
		exacts += (exacts.IsEmpty() ? L"" : L", ") + LocalText(exact, withTime);
	if (exacts.IsEmpty())
		exacts = L"-";
	// (a stay that was under way when the range began, or still is at its end, has no entering or no leaving to show)
	return (stay.HasEnter ? LocalText(stay.Enter, withTime) : CString(L"(range start)")) + L"  /  " + exacts + L"  /  " +
		(stay.HasLeave ? LocalText(stay.Leave, withTime) : CString(L"(range end)"));
}

CString CAnalysisView::CellText(AnalysisEvent const& event, ColumnType column, bool glyphs) const {
	CString text;
	bool aspect = event.Aspect != AspectType::None;
	switch (column) {
		case ColumnType::Date: {
			return LocalText(event.Time, true);
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
		case ColumnType::Stay:
			return StayText(event);
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
	auto event = EventAt(row);
	if (!event)
		return CString();
	return CellText(*event, GetColumnManager(h)->GetColumnTag<ColumnType>(col), m_Glyphs);
}

DWORD CAnalysisView::OnPrePaint(int, LPNMCUSTOMDRAW) {
	return CDRF_NOTIFYITEMDRAW;
}

DWORD CAnalysisView::OnItemPrePaint(int, LPNMCUSTOMDRAW) {
	return CDRF_NOTIFYSUBITEMDRAW;
}

DWORD CAnalysisView::OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) {
	auto lv = reinterpret_cast<LPNMLVCUSTOMDRAW>(cd);
	auto event = EventAt(static_cast<int>(cd->dwItemSpec));
	if (!event)
		return CDRF_DODEFAULT;

	lv->clrTextBk = RowColor(*event);
	lv->clrText = WTLHelper::IsDarkMode() ? RGB(235, 235, 235) : RGB(0, 0, 0);		// (without it the date cell comes out in a colour of its own)
	auto column = GetColumnManager(m_List)->GetColumnTag<ColumnType>(lv->iSubItem);
	::SelectObject(cd->hdc, m_Glyphs && IsGlyphColumn(column) ? m_SymbolFont.m_hFont : m_StdFont.m_hFont);
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
			case ColumnType::Stay: {
				// by when the stay was entered (or, failing that, left); rows without one after those with
				auto key = [](AnalysisEvent const& e) {
					if (!e.Window)
						return 1e300;
					return e.Window->HasEnter ? e.Window->Enter.Julian() : e.Window->Leave.Julian();
				};
				return SortHelper::Sort(key(a), key(b), si->SortAscending);
			}
		}
		return false;
	};
	std::stable_sort(m_Events.begin(), m_Events.end(), compare);
	ApplyFilter();		// (the rows the filter lets through are in the new places)
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
	Start(closed && dlg.Chart() == 0 ? m_Chart : charts[dlg.Chart()], dlg.Settings());
	return 0;
}

LRESULT CAnalysisView::OnRefresh(WORD, WORD, HWND, BOOL&) {
	// the chart as it is now, if it is still open
	OpenChart chart = m_Chart;
	if (m_Chart.View)
		for (auto& open : Frame()->OpenCharts())
			if (open.View == m_Chart.View) {
				chart = std::move(open);
				break;
			}
	// the aspects are those that were chosen, with the orbs of the aspect options as they are now
	auto settings = m_Settings;
	auto orbs = AspectOptions::Current().Transit;
	orbs.AspectEnabled = settings.Aspects.AspectEnabled;
	orbs.MajorOnly = false;
	settings.Aspects = orbs;
	Start(std::move(chart), settings);
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
		auto event = EventAt(row);
		if (!event)
			continue;
		for (int column = 0; column < _countof(ColumnNames); column++) {
			if (column)
				table += separator;
			// plain words, whatever the list shows
			table += quote(CellText(*event, static_cast<ColumnType>(column), false));
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

LRESULT CAnalysisView::OnPrint(WORD, WORD id, HWND, BOOL&) {
	// the events the list shows (what the filter lets through), in words
	std::unique_ptr<Printing::TableDocument> document;
	{
		CWaitCursor wait;
		std::vector<int> rows(m_Shown.size());
		for (int i = 0; i < static_cast<int>(rows.size()); i++)
			rows[i] = i;
		document = std::make_unique<Printing::TableDocument>(L"Analysis", L"", Printing::TableFromText(BuildTable(rows, false)), true);
	}
	if (id == ID_FILE_PRINT_PREVIEW)
		Printing::Preview(m_hWnd, *document);
	else
		Printing::Print(m_hWnd, *document);
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
	std::vector<int> rows(m_Shown.size());		// (what the filter lets through)
	for (int i = 0; i < static_cast<int>(rows.size()); i++)
		rows[i] = i;
	Helpers::SaveTextFileUtf8(m_hWnd, dlg.m_szFileName, BuildTable(rows, true), L"The events");
	return 0;
}
