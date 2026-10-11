#include "pch.h"
#include "VoidView.h"
#include "Aspects.h"
#include "AppSettings.h"
#include "DefaultFont.h"
#include "Printing.h"
#include "TimeZones.h"
#include <ToolbarHelper.h>
#include "DarkMode/DarkModeSubclass.h"

namespace {
	DateTime FromJd(double jd) {
		return DateTime(jd, DateTime::AfterPapalReform(jd));
	}

	// A UT instant as this machine's local wall-clock time (as the ephemeris shows its times).
	DateTime ToLocal(DateTime const& ut) {
		return TimeZones::FieldsToDateTime(TimeZones::UtToLocal(ut, TimeZones::Machine()));
	}

	// the UT instant of local midnight at the start of a local calendar date
	DateTime LocalMidnight(long year, long month, long day) {
		LocalDateTime fields;
		fields.Year = year;
		fields.Month = month;
		fields.Day = day;
		auto zone = TimeZones::Machine();
		return TimeZones::LocalToUt(fields, zone);
	}

	AspectType MajorAspectType(double angle) {
		if (angle == 0)
			return AspectType::Conjunction;
		if (angle == 60)
			return AspectType::Sextile;
		if (angle == 90)
			return AspectType::Square;
		if (angle == 120)
			return AspectType::Trine;
		return AspectType::Opposition;
	}

	ZodiacSign NextSign(ZodiacSign sign) {
		return static_cast<ZodiacSign>((static_cast<int>(sign) + 1) % 12);
	}

	// "03:20", or with a day or more "1d 03:20"
	CString FormatDuration(double days) {
		int minutes = (int)std::lround(days * 1440);
		CString text;
		if (minutes >= 1440)
			text.Format(L"%dd %02d:%02d", minutes / 1440, minutes % 1440 / 60, minutes % 60);
		else
			text.Format(L"%02d:%02d", minutes / 60, minutes % 60);
		return text;
	}

	// the purple of the void of course cells of the ephemeris
	COLORREF VoidColor() {
		return WTLHelper::IsDarkMode() ? RGB(95, 55, 175) : RGB(175, 145, 240);
	}
}

//
// CVoidRangeDlg
//

void CVoidRangeDlg::SetRange(DateTime const& from, DateTime const& to) {
	m_From = from;
	m_To = to;
}

void CVoidRangeDlg::DefaultRange(DateTime& from, DateTime& to) {
	auto today = DateTime::Today(true);
	from = today.AddDays(-7);
	// the same day three months on (the last day of the month, if that month is shorter)
	long year = today.Year(), month = today.Month() + 3;
	if (month > 12) {
		month -= 12;
		year++;
	}
	long days = DateTime::DaysInMonth(month, DateTime::IsLeap(year, true));
	to = DateTime(year, month, (double)std::min(today.Day(), days));
}

LRESULT CVoidRangeDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	CenterWindow(GetParent());
	m_FromBoxes.Init(m_hWnd, IDC_AN_FROM_DAY, IDC_AN_FROM_MONTH, IDC_AN_FROM_YEAR);
	m_ToBoxes.Init(m_hWnd, IDC_AN_TO_DAY, IDC_AN_TO_MONTH, IDC_AN_TO_YEAR);
	m_FromBoxes.Set(m_From);
	m_ToBoxes.Set(m_To);
	return TRUE;
}

LRESULT CVoidRangeDlg::OnOK(WORD, WORD, HWND, BOOL&) {
	DateTime from, to;
	UINT control;
	PCWSTR problem;
	if (!m_FromBoxes.Get(from, control, problem) || !m_ToBoxes.Get(to, control, problem)) {
		AtlMessageBox(m_hWnd, problem, L"Astro Studio", MB_ICONWARNING);
		GotoDlgCtrl(GetDlgItem(control));
		return 0;
	}
	if (to.Julian() < from.Julian()) {
		AtlMessageBox(m_hWnd, L"The range must end on or after the day it starts on.", L"Astro Studio", MB_ICONWARNING);
		GotoDlgCtrl(GetDlgItem(IDC_AN_TO_DAY));
		return 0;
	}
	if (to.Julian() - from.Julian() > 3660) {
		AtlMessageBox(m_hWnd, L"The range can be ten years at most.", L"Astro Studio", MB_ICONWARNING);
		GotoDlgCtrl(GetDlgItem(IDC_AN_TO_YEAR));
		return 0;
	}
	m_From = from;
	m_To = to;
	EndDialog(IDOK);
	return 0;
}

LRESULT CVoidRangeDlg::OnCancel(WORD, WORD, HWND, BOOL&) {
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CVoidRangeDlg::OnDefault(WORD, WORD, HWND, BOOL&) {
	DateTime from, to;
	DefaultRange(from, to);
	m_FromBoxes.Set(from);
	m_ToBoxes.Set(to);
	return 0;
}

LRESULT CVoidRangeDlg::OnMonthOrYearChanged(WORD, WORD id, HWND, BOOL&) {
	(id == IDC_AN_FROM_MONTH || id == IDC_AN_FROM_YEAR ? m_FromBoxes : m_ToBoxes).Update();
	return 0;
}

//
// CVoidView
//

LRESULT CVoidView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_Glyphs = AppSettings::Get().VoidGlyphs() != 0;
	CVoidRangeDlg::DefaultRange(m_From, m_To);

	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_OWNERDATA | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);

	ToolBarButtonInfo buttons[] = {
		{ ID_VIEW_GLYPHS, IDI_GLYPH, BTNS_CHECK, L"Glyphs" },
		{ 0 },
		{ ID_VOID_RANGE, IDI_OPTIONS, 0, L"Range" },
	};
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	CToolBarCtrl tb(ToolbarHelper::CreateAndInitToolBar(m_hWndToolBar, buttons, _countof(buttons)));
	AddSimpleReBarBand(tb);
	Frame()->AddToolBarToUI(tb);
	// The toolbars share one UI map, which updates a button only when its state there changes: if another tab left Glyphs in
	// the state this one wants, this new button would never hear of it.
	tb.CheckButton(ID_VIEW_GLYPHS, m_Glyphs);

	CreateFonts();
	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Starts", LVCFMT_LEFT, 180, ColumnType::Start);
	cm->AddColumn(L"Last aspect", LVCFMT_LEFT, 180, ColumnType::LastAspect);
	cm->AddColumn(L"Moon in", LVCFMT_LEFT, 90, ColumnType::Sign);
	cm->AddColumn(L"Ends", LVCFMT_LEFT, 180, ColumnType::End);
	cm->AddColumn(L"Moon enters", LVCFMT_LEFT, 90, ColumnType::Enters);
	cm->AddColumn(L"Duration", LVCFMT_RIGHT, 90, ColumnType::Duration);
	cm->UpdateColumns();

	Calculate();
	SetTimer(NowTimerId, NowIntervalMs);
	return 0;
}

LRESULT CVoidView::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
	KillTimer(NowTimerId);
	handled = FALSE;
	return 0;
}

LRESULT CVoidView::OnTimer(UINT, WPARAM id, LPARAM, BOOL& handled) {
	if (id != NowTimerId) {
		handled = FALSE;
		return 0;
	}
	// the marking of the void going on now (and of the past ones) moves on
	m_List.Invalidate();
	return 0;
}

LRESULT CVoidView::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&) {
	m_List.RedrawWindow();
	return 0;
}

void CVoidView::Calculate() {
	CWaitCursor wait;
	// from local midnight of the first day to local midnight after the last one
	auto start = LocalMidnight(m_From.Year(), m_From.Month(), m_From.Day());
	auto next = m_To.AddDays(1);
	auto end = LocalMidnight(next.Year(), next.Month(), next.Day());

	m_Periods.clear();
	for (auto const& period : m_Calc.CalcVoidOfCourse(start, end))
		if (period.End.Julian() > start.Julian() && period.Start.Julian() < end.Julian())
			m_Periods.push_back(period);

	m_List.SetItemCountEx((int)m_Periods.size(), LVSICF_NOSCROLL);
	AutoSizeColumns();
	m_List.RedrawWindow();

	// the void going on now (or the next one) in view
	double now = DateTime::Now().Julian();
	for (int i = 0; i < (int)m_Periods.size(); i++)
		if (m_Periods[i].End.Julian() > now) {
			m_List.EnsureVisible(std::min(i + 5, (int)m_Periods.size() - 1), FALSE);
			m_List.EnsureVisible(i, FALSE);
			break;
		}
}

void CVoidView::CreateFonts(bool fontChanged) {
	// the size of the text font, if the user chose one
	// the ephemeris's size (its A+/A- buttons, or the size chosen with Options > Font), so that the two lists look alike
	int size = std::clamp(AppSettings::Get().EphemerisFontSize(), 70, 180);
	if (fontChanged)
		if (int chosen = AppSettings::Get().TextFont().lfHeight; chosen > 0)
			size = std::clamp(chosen, 70, 180);
	if (m_Font)
		m_Font.DeleteObject();
	m_Font.CreatePointFont(size, L"HamburgSymbols");
	if (m_StdFont)
		m_StdFont.DeleteObject();
	// the text font the user chose with Options > Font, or the system's (as the analysis view does)
	LOGFONT lf = AppSettings::Get().TextFont();
	if (lf.lfFaceName[0] == 0) {
		CFontHandle(AtlGetDefaultGuiFont()).GetLogFont(lf);
		lf.lfWeight = FW_NORMAL;
	}
	lf.lfHeight = size;
	m_StdFont.CreatePointFontIndirect(&lf);
	m_List.SetFont(m_StdFont);
}

void CVoidView::AutoSizeColumns() {
	// LVSCW_AUTOSIZE can't see the rows of a virtual list (it only fits the header), so every cell is measured here, in the font it
	// is drawn in
	CClientDC dc(m_List);
	auto old = dc.SelectFont(m_StdFont);
	auto measure = [&](CString const& text, HFONT font) {
		dc.SelectFont(font);
		CSize size;
		dc.GetTextExtent(text, text.GetLength(), &size);
		return (int)size.cx;
	};
	auto cm = GetColumnManager(m_List);
	m_List.SetRedraw(FALSE);
	int count = m_List.GetHeader().GetItemCount();
	for (int i = 0; i < count; i++) {
		auto type = cm->GetColumnTag<ColumnType>(i);
		bool glyphColumn = m_Glyphs && (type == ColumnType::LastAspect || type == ColumnType::Sign || type == ColumnType::Enters);
		WCHAR name[64]{};
		LVCOLUMN column{ LVCF_TEXT };
		column.pszText = name;
		column.cchTextMax = _countof(name);
		m_List.GetColumn(i, &column);
		int width = measure(name, m_StdFont);
		for (int row = 0; row < (int)m_Periods.size(); row++)
			width = std::max(width, measure(CellText(row, type, m_Glyphs), glyphColumn ? m_Font : m_StdFont));
		m_List.SetColumnWidth(i, width + 24);		// the cell's margins
	}
	dc.SelectFont(old);
	m_List.SetRedraw(TRUE);
}

CString CVoidView::CellText(int row, ColumnType type, bool glyphs) const {
	auto const& p = m_Periods[row];
	auto& font = DefaultFont::Get();
	switch (type) {
		case ColumnType::Start:
			return Helpers::FormatDateTime(ToLocal(p.Start), DateTimeFormatOptions::None).Trim();
		case ColumnType::End:
			return Helpers::FormatDateTime(ToLocal(p.End), DateTimeFormatOptions::None).Trim();
		case ColumnType::LastAspect:
			if (p.WholeSign)
				return glyphs ? CString(L"-") : CString(L"none in the sign");
			{
				auto aspect = MajorAspectType(p.LastAngle);
				if (glyphs)
					return font.GetPlanetGlyphAsString(Planet::Moon) + CString(L" ") + font.GetAspectGlyphAsString(aspect) + L" " +
						font.GetPlanetGlyphAsString(p.LastPlanet);
				return CString(Helpers::GetAspectName(aspect)) + L" " + Helpers::GetPlanetName(p.LastPlanet);
			}
		case ColumnType::Sign:
			return glyphs ? font.GetSignGlyphAsString(p.Sign) : Helpers::GetZodiacSignName(p.Sign);
		case ColumnType::Enters:
			return glyphs ? font.GetSignGlyphAsString(NextSign(p.Sign)) : Helpers::GetZodiacSignName(NextSign(p.Sign));
		case ColumnType::Duration:
			return FormatDuration(p.End.Julian() - p.Start.Julian());
	}
	return L"";
}

CString CVoidView::GetColumnText(HWND h, int row, int col) {
	if (row >= (int)m_Periods.size())
		return L"";
	return CellText(row, GetColumnManager(h)->GetColumnTag<ColumnType>(col), m_Glyphs);
}

DWORD CVoidView::OnPrePaint(int, LPNMCUSTOMDRAW) {
	return CDRF_NOTIFYITEMDRAW;
}

DWORD CVoidView::OnItemPrePaint(int, LPNMCUSTOMDRAW) {
	return CDRF_NOTIFYSUBITEMDRAW;
}

DWORD CVoidView::OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) {
	auto lv = (NMLVCUSTOMDRAW*)cd;
	int row = (int)cd->dwItemSpec;
	if (row >= (int)m_Periods.size())
		return CDRF_DODEFAULT;
	auto type = GetColumnManager(m_List)->GetColumnTag<ColumnType>(lv->iSubItem);
	auto const& p = m_Periods[row];
	const bool dark = WTLHelper::IsDarkMode();

	// the void going on now is purple; the ones that are over are greyed
	double now = DateTime::Now().Julian();
	// (the colours are always set: left alone, the list's own are those of the light look)
	lv->clrTextBk = dark ? DarkMode::getViewBackgroundColor() : ::GetSysColor(COLOR_WINDOW);
	lv->clrText = dark ? DarkMode::getTextColor() : ::GetSysColor(COLOR_WINDOWTEXT);
	if (p.Start.Julian() <= now && now < p.End.Julian())
		lv->clrTextBk = VoidColor();
	else if (p.End.Julian() <= now)
		lv->clrText = dark ? RGB(135, 135, 135) : RGB(128, 128, 128);

	bool glyphColumn = type == ColumnType::LastAspect || type == ColumnType::Sign || type == ColumnType::Enters;
	CDCHandle(cd->hdc).SelectFont(glyphColumn && m_Glyphs ? m_Font : m_StdFont);
	return CDRF_NEWFONT;
}

void CVoidView::UpdateViewUI() {
	auto& ui = Frame()->GetUI();
	ui.UISetCheck(ID_VIEW_GLYPHS, m_Glyphs);
	ui.UIEnable(ID_FILE_EXPORT, TRUE);
	ui.UIEnable(ID_FILE_PRINT, TRUE);
	ui.UIEnable(ID_FILE_PRINT_PREVIEW, TRUE);
}

void CVoidView::PageActivated(bool active) {
	if (active)
		UpdateViewUI();
	else {
		Frame()->GetUI().UIEnable(ID_FILE_EXPORT, FALSE);	// the next page enables them again if it can
		Frame()->GetUI().UIEnable(ID_FILE_PRINT, FALSE);
		Frame()->GetUI().UIEnable(ID_FILE_PRINT_PREVIEW, FALSE);
	}
}

void CVoidView::TextFontChanged() {
	CreateFonts(true);
	AutoSizeColumns();
	m_List.RedrawWindow();
}

CString CVoidView::RangeText() const {
	CString text;
	text.Format(L"%04d/%02d/%02d - %04d/%02d/%02d", m_From.Year(), m_From.Month(), m_From.Day(), m_To.Year(), m_To.Month(), m_To.Day());
	return text;
}

bool CVoidView::GetStatusInfo(StatusInfo& info) const {
	info.Name = L"Moon void of course";
	info.Time = RangeText() + L" (local time)";
	info.Details.Format(L"%d periods", (int)m_Periods.size());
	return true;
}

LRESULT CVoidView::OnViewGlyphs(WORD, WORD, HWND, BOOL&) {
	m_Glyphs = !m_Glyphs;
	AppSettings::Get().VoidGlyphs(m_Glyphs ? 1 : 0);
	AutoSizeColumns();
	m_List.RedrawWindow();
	UpdateViewUI();
	return 0;
}

LRESULT CVoidView::OnRange(WORD, WORD, HWND, BOOL&) {
	CVoidRangeDlg dlg;
	dlg.SetRange(m_From, m_To);
	if (dlg.DoModal(m_hWnd) == IDOK) {
		m_From = dlg.From();
		m_To = dlg.To();
		Calculate();
	}
	return 0;
}

Helpers::TableSource CVoidView::Table() const {
	Helpers::TableSource table;
	table.Headers = { L"Starts", L"Last aspect", L"Moon in", L"Ends", L"Moon enters", L"Duration" };
	table.Rows = (int)m_Periods.size();
	table.Cell = [this](int row, int column) {
		return CellText(row, static_cast<ColumnType>(column), false);
	};
	return table;
}

LRESULT CVoidView::OnEditCopy(WORD, WORD, HWND, BOOL&) {
	Helpers::CopyListRows(m_hWnd, m_List, Table());
	return 0;
}

LRESULT CVoidView::OnExport(WORD, WORD, HWND, BOOL&) {
	static constexpr wchar_t filter[] = L"CSV files (*.csv)\0*.csv\0All files (*.*)\0*.*\0";
	CSimpleFileDialog dlg(FALSE, L"csv", L"Void of course", OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING, filter, m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (ok)
		Helpers::SaveTable(m_hWnd, Table(), dlg.m_szFileName, L"The void of course list");
	return 0;
}

LRESULT CVoidView::OnPrint(WORD, WORD id, HWND, BOOL&) {
	Printing::TableDocument document(L"Moon Void of Course", RangeText() + L" (local time)", Table());
	if (id == ID_FILE_PRINT_PREVIEW)
		Printing::Preview(m_hWnd, document);
	else
		Printing::Print(m_hWnd, document);
	return 0;
}
