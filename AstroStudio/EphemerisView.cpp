#include "pch.h"
#include "EphemerisView.h"
#include "ChartData.h"
#include "TimeZones.h"
#include <ToolbarHelper.h>

#include "ColorHelper.h"
#include "WTLHelper.h"
#include "DarkMode/DarkModeSubclass.h"

ColorOptions DarkColors{
	RGB(30, 30, 30),
	CLR_INVALID,
	{
		ColorHelper::Darken(RGB(255, 128, 0), 40),
		ColorHelper::Darken(RGB(224, 224, 0), 40),
		ColorHelper::Darken(RGB(0, 255, 128), 40),
		ColorHelper::Darken(RGB(0, 192, 255), 50)
	},
	true, true
};

void CPlanetStrip::SetText(PCWSTR label, HFONT labelFont, PCWSTR text, HFONT font) {
	m_Label = label;
	m_LabelFont = labelFont;
	m_Text = text;
	m_Font = font;
	if (m_hWnd)
		Invalidate();
}

LRESULT CPlanetStrip::OnPaint(UINT msg, WPARAM wp, LPARAM, BOOL&) {
	CPaintDC paintDc(msg == WM_PAINT ? m_hWnd : nullptr);
	CDCHandle dc(msg == WM_PAINT ? paintDc.m_hDC : (HDC)wp);

	CRect rc;
	GetClientRect(&rc);
	bool dark = WTLHelper::IsDarkMode();
	dc.FillSolidRect(&rc, dark ? DarkMode::getCtrlBackgroundColor() : ::GetSysColor(COLOR_BTNFACE));
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(dark ? DarkMode::getTextColor() : ::GetSysColor(COLOR_BTNTEXT));
	rc.left += 6;
	const UINT flags = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS;
	auto old = dc.SelectFont(m_LabelFont ? m_LabelFont : AtlGetDefaultGuiFont());
	CSize size;
	dc.GetTextExtent(m_Label, m_Label.GetLength(), &size);
	dc.DrawText(m_Label, m_Label.GetLength(), &rc, flags);
	rc.left += size.cx + 16;
	dc.SelectFont(m_Font ? m_Font : AtlGetDefaultGuiFont());
	dc.DrawText(m_Text, m_Text.GetLength(), &rc, flags);
	dc.SelectFont(old);
	return 0;
}

CString CEphemerisView::GetColumnText(HWND h, int row, int col) {
	auto type = GetColumnManager(h)->GetColumnTag<ColumnType>(col);
	CString text;
	switch (type) {
		case ColumnType::Time: return Helpers::FormatDateTime(m_StartTime.AddDays(row));
		case ColumnType::Phenom: return GetRowPhenom(row, (m_FormatOptions & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs);

		default:
			EnsureRows(row + 2);
			ATLASSERT(row < m_Items.size());
			auto& item = m_Items[row];
			int index = int(type) - int(ColumnType::Planet);
			auto planet = (Planet)(int(type) - int(ColumnType::Planet));
			auto& pp = item.Planets[index];
			pp.Position.Longitude.Flags |= (pp.Position.Speed < 0 ? AstroPointFlags::Retro : AstroPointFlags::None);
			text = Helpers::FormatLongitude(pp.Position.Longitude, m_FormatOptions);
			text = ((m_FormatOptions & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs ?
				(PCWSTR)DefaultFont::Get().GetPlanetGlyphAsString(pp.Planet) : L"") + CString(L" ") + text;
			break;
	}
	return text;
}

bool CEphemerisView::IsSortable(HWND, int col) const {
	return false;
}

bool CEphemerisView::OnRightClickList(HWND, int row, int col, POINT const& pt) {
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	return Frame()->TrackPopupMenu(menu.GetSubMenu(0), 0, pt.x, pt.y);
}

DWORD CEphemerisView::OnPrePaint(int, LPNMCUSTOMDRAW cd) {
	return CDRF_NOTIFYITEMDRAW;
}

DWORD CEphemerisView::OnItemPrePaint(int, LPNMCUSTOMDRAW cd) {
	return CDRF_NOTIFYSUBITEMDRAW;
}

DWORD CEphemerisView::OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) {
	if ((int)cd->dwItemSpec >= m_Items.size())
		return CDRF_DODEFAULT;

	auto lv = (NMLVCUSTOMDRAW*)cd;
	auto colType = GetColumnManager(m_List)->GetColumnTag<ColumnType>(lv->iSubItem);
	CDCHandle dc(cd->hdc);

	lv->clrTextBk = CLR_INVALID;
	auto& item = m_Items[(int)cd->dwItemSpec];
	bool highlight = item.Date == DateTime::Today(true);
	if (colType >= ColumnType::Planet) {
		if (m_ColorOptions.PaintSigns) {
			int element = int(item.Planets[lv->iSubItem - 1].Position.Longitude.Sign()) % 4;
			lv->clrTextBk = m_ColorOptions.ElementBackColor[element];
		}
		if (m_ColorOptions.PaintRetro) {
			if (item.Planets[lv->iSubItem - 1].Position.Speed < 0) {
				if (m_ColorOptions.PaintSigns)
					lv->clrTextBk = Helpers::Darken(lv->clrTextBk, 10);
				else
					lv->clrTextBk = m_ColorOptions.RetroBackColor;
			}
		}
		if (highlight)
			lv->clrTextBk = Helpers::Lighten(lv->clrTextBk, 25);
	}
	else if (highlight && colType == ColumnType::Time)
		lv->clrTextBk = WTLHelper::IsDarkMode() ? RGB(20, 20, 0) : RGB(220, 220, 0);

	dc.SelectFont(colType != ColumnType::Time && (m_FormatOptions & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs ? m_Font : m_StdFont);
	return CDRF_NEWFONT | CDRF_SKIPPOSTPAINT;
}

void CEphemerisView::UpdateUI(CUpdateUIBase& ui) {
	UpdateViewUI();
}

void CEphemerisView::PageActivated(bool active) {
	if (active)
		UpdateViewUI();
	else
		Frame()->GetUI().UIEnable(ID_FILE_EXPORT, FALSE);	// the next page enables it again if it can export
}

void CEphemerisView::UpdateList() {
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
}

void CEphemerisView::EnsureRows(size_t count) {
	while (m_Items.size() < count) {
		RowData data;
		data.Date = m_StartTime.AddDays(m_Increment * (int)m_Items.size());
		for (auto p : m_Planets) {
			PlanetData pd;
			pd.Position = m_Calc.CalcPlanet(p, data.Date);
			pd.Planet = p;
			data.Planets.push_back(pd);
		}
		m_Items.push_back(std::move(data));
	}
}

CString CEphemerisView::GetRowPhenom(int row, bool glyphs) const {
	CString text;
	auto& item = m_Items[row];
	if (!item.PhenomCalculated) {
		auto& next = m_Items[row + 1];
		item.PhenomCalculated = true;
		auto count = (int)item.Planets.size();
		for (int i = 0; i < count; i++) {
			auto& c = next.Planets[i];
			auto& p = item.Planets[i];

			//
			// sign ingress
			//
			if (c.Position.Longitude.Sign() != p.Position.Longitude.Sign()) {
				if (!item.PhenomGlyph.IsEmpty())
					item.PhenomGlyph += L" | ";
				if (!item.PhenomText.IsEmpty())
					item.PhenomText += L" | ";
				auto ingress = m_Calc.CalcPlanetIngress(c.Planet, item.Date, c.Position.Speed < 0);
				item.PhenomGlyph += DefaultFont::Get().GetPlanetGlyphAsString(c.Planet) + CString(L" ") +
					DefaultFont::Get().GetSignGlyphAsString(c.Position.Longitude.Sign());
				item.PhenomText += Helpers::GetPlanetName(c.Planet) + CString(L" to ") +
					Helpers::GetZodiacSignName(c.Position.Longitude.Sign()).Left(3);
				auto dt = L" (" + Helpers::FormatDateTime(ingress.Time, DateTimeFormatOptions::TimeOnly) + L")";
				item.PhenomGlyph += dt;
				item.PhenomText += dt;
			}
			//
			// Retro/Direct
			//
			bool direct = c.Position.Speed > 0 && p.Position.Speed < 0;
			bool retro = c.Position.Speed < 0 && p.Position.Speed > 0;
			if (direct || retro) {
				auto station = m_Calc.CalcPlanetStation(c.Planet, item.Date);
				if (!item.PhenomGlyph.IsEmpty())
					item.PhenomGlyph += L" | ";
				if (!item.PhenomText.IsEmpty())
					item.PhenomText += L" | ";
				if (direct) {
					item.PhenomGlyph += DefaultFont::Get().GetPlanetGlyphAsString(c.Planet) + CString(L" ") + DefaultFont::Get().GetDirectGlyphAsString();
					item.PhenomText += Helpers::GetPlanetName(c.Planet) + CString(L" D");
				}
				else {
					item.PhenomGlyph += DefaultFont::Get().GetPlanetGlyphAsString(c.Planet) + CString(L" ") + DefaultFont::Get().GetRetroGlyphAsString();
					item.PhenomText += Helpers::GetPlanetName(c.Planet) + CString(L" R");
				}
				auto dt = L" (" + Helpers::FormatDateTime(station.Time, DateTimeFormatOptions::TimeOnly) + L")";
				item.PhenomGlyph += dt;
				item.PhenomText += dt;
			}
		}
	}
	return glyphs ? item.PhenomGlyph : item.PhenomText;
}

void CEphemerisView::CreateFonts() {
	if (m_Font)
		m_Font.DeleteObject();
	m_Font.CreatePointFont(m_FontSize, L"HamburgSymbols");
	if (m_StdFont)
		m_StdFont.DeleteObject();
	m_StdFont.CreatePointFont(m_FontSize, L"Consolas");
}

void CEphemerisView::UpdateViewUI() {
	auto& ui = Frame()->GetUI();
	ui.UISetCheck(ID_VIEW_GLYPHS, (m_FormatOptions & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs);
	ui.UISetCheck(ID_VIEW_SECONDS, (m_FormatOptions & FormatOptions::ShowSeconds) == FormatOptions::ShowSeconds);
	ui.UIEnable(ID_FONT_BIGGER, m_FontSize < 180);
	ui.UIEnable(ID_FONT_SMALLER, m_FontSize > 70);
	ui.UIEnable(ID_FILE_EXPORT, TRUE);
	ui.UISetCheck(ID_VIEW_GRIDLINES, (m_List.GetExtendedListViewStyle() & LVS_EX_GRIDLINES) != 0);
}

void CEphemerisView::UpdateNowStrip() {
	if (!m_NowStrip)
		return;

	// The whole line is in the glyph font when glyphs are on (the digits are in it too, like in the list),
	// with planet names instead of glyphs when they are off.
	bool glyphs = (m_FormatOptions & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs;
	// the time is read once: the label shows it to the second, and it is what the positions are for
	SYSTEMTIME st;
	::GetSystemTime(&st);
	DateTime now(st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond + st.wMilliseconds / 1000.0, true);
	// The positions depend only on the instant, which is what UT is; the label shows it as this machine's local time
	// (and how far that is from UT), which is the clock the user is looking at.
	SYSTEMTIME local;
	::GetLocalTime(&local);
	FILETIME ftUt, ftLocal;
	::SystemTimeToFileTime(&st, &ftUt);
	::SystemTimeToFileTime(&local, &ftLocal);
	auto diff = [](FILETIME const& f) { return (long long)((ULARGE_INTEGER{ f.dwLowDateTime, f.dwHighDateTime }).QuadPart); };
	int offset = (int)std::llround((diff(ftLocal) - diff(ftUt)) / 600000000.0);	// minutes east of UT
	CString label;
	label.Format(L"%04d/%02d/%02d %02d:%02d:%02d (UTC%c%02d:%02d) |", local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond,
		offset < 0 ? L'-' : L'+', abs(offset) / 60, abs(offset) % 60);
	CString text;
	for (auto p : m_Planets) {
		auto pos = m_Calc.CalcPlanet(p, now);
		pos.Longitude.Flags |= (pos.Speed < 0 ? AstroPointFlags::Retro : AstroPointFlags::None);
		if (!text.IsEmpty())
			text += L"    ";
		text += glyphs ? (PCWSTR)DefaultFont::Get().GetPlanetGlyphAsString(p) : (PCWSTR)CString(Helpers::GetPlanetName(p)).Left(3);
		text += L" " + Helpers::FormatLongitude(pos.Longitude, m_FormatOptions);
	}
	m_NowStrip.SetText(label, m_StdFont, text, glyphs ? m_Font : m_StdFont);

	// the band is as high as the text
	CClientDC dc(m_NowStrip);
	auto old = dc.SelectFont(glyphs ? m_Font : m_StdFont);
	TEXTMETRIC tm;
	dc.GetTextMetrics(&tm);
	dc.SelectFont(old);
	int height = tm.tmHeight + 8;
	CReBarCtrl rebar(m_hWndToolBar);
	REBARBANDINFO info{ sizeof(info), RBBIM_CHILDSIZE };
	rebar.GetBandInfo(NowBand, &info);
	if ((int)info.cyMinChild != height) {
		info.cyMinChild = info.cyChild = info.cyMaxChild = height;
		rebar.SetBandInfo(NowBand, &info);
		UpdateLayout();
	}
}

void CEphemerisView::AutoSizeColumns() {
	m_List.SetRedraw(FALSE);
	int count = m_List.GetHeader().GetItemCount();
	for (int i = 0; i < count; i++) {
		m_List.SetColumnWidth(i, LVSCW_AUTOSIZE);
		if (m_List.GetColumnWidth(i) < 100)
			m_List.SetColumnWidth(i, 100);
	}
	m_List.SetRedraw(TRUE);
}

LRESULT CEphemerisView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | 0*WS_CLIPCHILDREN
		| LVS_OWNERDATA | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 0, 1);
	m_List.SetImageList(images, LVSIL_SMALL);

	m_ColorOptions = WTLHelper::IsDarkMode() ? DarkColors : ColorOptions();

	ToolBarButtonInfo buttons[] = {
		{ ID_VIEW_GLYPHS, IDI_GLYPH, BTNS_CHECK, L"Glyphs" },
		{ ID_VIEW_SECONDS, IDI_CLOCK, BTNS_CHECK, L"Seconds" },
		{ 0 },
		{ ID_FONT_BIGGER, IDI_FONT_BIGGER },
		{ ID_FONT_SMALLER, IDI_FONT_SMALLER },
		{ ID_FONT_SIZE_DEFAULT, IDI_FONT_SIZE_DEFAULT },
		{ ID_VIEW_GRIDLINES, IDI_GRID, BTNS_CHECK },
	};

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	auto tb = ToolbarHelper::CreateAndInitToolBar(m_hWndToolBar, buttons, _countof(buttons));
	AddSimpleReBarBand(tb);
	Frame()->AddToolBarToUI(tb);
	CreateFonts();

	m_Planets = Helpers::GetStandardPlanets();
	m_Planets.push_back(Planet::Chiron);
	m_Planets.push_back(Planet::Lilith);
	//m_Planets.push_back(PlanetType::OscuApog);
	m_Planets.push_back(Planet::TrueNode);

	m_FormatOptions = FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph;

	// a second toolbar line with where the planets are right now
	{
		CClientDC dc(m_hWnd);
		auto old = dc.SelectFont(m_Font);
		TEXTMETRIC tm;
		dc.GetTextMetrics(&tm);
		dc.SelectFont(old);
		RECT rcStrip{ 0, 0, 100, tm.tmHeight + 8 };
		m_NowStrip.Create(m_hWndToolBar, rcStrip, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS);
		AddSimpleReBarBand(m_NowStrip, nullptr, TRUE, 0, TRUE);
		UpdateNowStrip();
		SetTimer(NowTimerId, NowIntervalMs);
	}

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Date", LVCFMT_LEFT, 120, ColumnType::Time);
	int i = 0;
	for (auto& p : m_Planets) {
		cm->AddColumn(Helpers::GetPlanetName(p), LVCFMT_LEFT, 100, ColumnType(int(ColumnType::Planet) + i++));
	}
	cm->AddColumn(L"Phenomena", LVCFMT_LEFT, 420, ColumnType::Phenom);
	cm->UpdateColumns();

	m_StartTime = DateTime::Today();
	m_StartTime = m_StartTime.AddDays(-30);
	m_Items.reserve(2000);
	m_List.SetItemCount(2000);

	DarkMode::setDarkWndNotifySafe(m_hWnd);

	return 0;
}

LRESULT CEphemerisView::OnViewGlyphs(WORD, WORD, HWND, BOOL&) {
	m_FormatOptions ^= FormatOptions::UseGlyphs;
	UpdateNowStrip();
	UpdateList();
	AutoSizeColumns();
	m_List.RedrawWindow();

	return 0;
}

LRESULT CEphemerisView::OnViewSeconds(WORD, WORD, HWND, BOOL&) {
	m_FormatOptions ^= FormatOptions::ShowSeconds;
	UpdateNowStrip();
	UpdateList();
	AutoSizeColumns();
	m_List.RedrawWindow();

	return 0;
}

LRESULT CEphemerisView::OnChangeFontSize(WORD, WORD id, HWND, BOOL&) {
	if (id == ID_FONT_SIZE_DEFAULT)
		m_FontSize = 100;
	else
		m_FontSize += id == ID_FONT_BIGGER ? 8 : -8;
	auto images = m_List.GetImageList(LVSIL_SMALL);
	images.SetIconSize(1, m_FontSize / 6);
	m_List.SetImageList(images, LVSIL_SMALL);

	CreateFonts();
	UpdateNowStrip();
	AutoSizeColumns();
	m_List.RedrawWindow();
	UpdateViewUI();

	return 0;
}

LRESULT CEphemerisView::OnViewGridLines(WORD, WORD, HWND, BOOL&) {
	auto style = m_List.GetExtendedListViewStyle() ^ LVS_EX_GRIDLINES;
	m_List.SetExtendedListViewStyle(style, LVS_EX_GRIDLINES);

	return 0;
}

CString CEphemerisView::PlainCellText(int row, ColumnType type) {
	EnsureRows(row + 2);
	if (type == ColumnType::Time)
		return Helpers::FormatDateTime(m_StartTime.AddDays(row)).Trim();
	if (type == ColumnType::Phenom)
		return GetRowPhenom(row, false);

	auto& pp = m_Items[row].Planets[int(type) - int(ColumnType::Planet)];
	auto longitude = pp.Position.Longitude;
	if (pp.Position.Speed < 0)
		longitude.Flags |= AstroPointFlags::Retro;
	return Helpers::FormatLongitude(longitude, m_FormatOptions & ~FormatOptions::UseGlyphs);
}

CString CEphemerisView::BuildTable(std::vector<int> const& rows, bool csv) {
	const wchar_t separator = csv ? L',' : L'\t';
	auto cell = [&](CString text) {
		if (csv) {
			if (text.FindOneOf(L",\"\r\n") >= 0) {
				text.Replace(L"\"", L"\"\"");
				text = L"\"" + text + L"\"";
			}
		}
		else {
			text.Replace(L'\t', L' ');
		}
		return text;
	};

	int count = m_List.GetHeader().GetItemCount();
	std::vector<int> order(count);
	m_List.GetColumnOrderArray(count, order.data());
	auto columns = GetColumnManager(m_List);

	CString table;
	for (int i = 0; i < count; i++) {
		WCHAR name[128]{};
		LVCOLUMN column{ LVCF_TEXT };
		column.pszText = name;
		column.cchTextMax = _countof(name);
		m_List.GetColumn(order[i], &column);
		if (i > 0)
			table += separator;
		table += cell(name);
	}
	table += L"\r\n";

	for (int row : rows) {
		for (int i = 0; i < count; i++) {
			if (i > 0)
				table += separator;
			table += cell(PlainCellText(row, columns->GetColumnTag<ColumnType>(order[i])));
		}
		table += L"\r\n";
	}
	return table;
}

LRESULT CEphemerisView::OnEditCopy(WORD, WORD, HWND, BOOL&) {
	// the selected rows, as text for a spreadsheet or a text editor
	std::vector<int> rows;
	for (int row = m_List.GetNextItem(-1, LVNI_SELECTED); row >= 0; row = m_List.GetNextItem(row, LVNI_SELECTED))
		rows.push_back(row);
	if (rows.empty())
		return 0;

	CWaitCursor wait;
	if (!Helpers::CopyTextToClipboard(m_hWnd, BuildTable(rows, false)))
		AtlMessageBox(m_hWnd, L"The rows could not be copied to the clipboard.", L"Astro Studio", MB_ICONWARNING);
	return 0;
}

LRESULT CEphemerisView::OnExport(WORD, WORD, HWND, BOOL&) {
	static constexpr wchar_t filter[] = L"CSV files (*.csv)\0*.csv\0All files (*.*)\0*.*\0";
	CSimpleFileDialog dlg(FALSE, L"csv", L"Ephemeris", OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING, filter, m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (!ok)
		return 0;

	// every row of the list
	CWaitCursor wait;
	std::vector<int> rows(m_List.GetItemCount());
	for (int i = 0; i < (int)rows.size(); i++)
		rows[i] = i;
	CString table = BuildTable(rows, true);

	// UTF-8 with a byte order mark, which is how Excel knows it is UTF-8 (the degree signs need it)
	int length = ::WideCharToMultiByte(CP_UTF8, 0, table, table.GetLength(), nullptr, 0, nullptr, nullptr);
	std::string bytes("\xEF\xBB\xBF");
	bytes.resize(3 + length);
	::WideCharToMultiByte(CP_UTF8, 0, table, table.GetLength(), bytes.data() + 3, length, nullptr, nullptr);

	DWORD error = ERROR_SUCCESS;
	HANDLE file = ::CreateFileW(dlg.m_szFileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		error = ::GetLastError();
	else {
		DWORD written = 0;
		if (!::WriteFile(file, bytes.data(), (DWORD)bytes.size(), &written, nullptr))
			error = ::GetLastError();
		::CloseHandle(file);
	}
	if (error != ERROR_SUCCESS) {
		WCHAR reason[256]{};
		::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0, reason, _countof(reason), nullptr);
		CString message;
		message.Format(L"The ephemeris could not be saved to %s:\n\n%s", dlg.m_szFileName, reason);
		AtlMessageBox(m_hWnd, (PCWSTR)message, L"Astro Studio", MB_ICONWARNING);
	}
	return 0;
}

LRESULT CEphemerisView::OnNewChart(WORD, WORD, HWND, BOOL&) {
	int selected = m_List.GetSelectionMark();
	if (selected < 0 || selected >= (int)m_Items.size()) {
		Frame()->NewChartWithDialog();	// now, in this machine's time zone
		return 0;
	}

	// For now a chart from an ephemeris row is for noon UT of that date (the row itself is a UT midnight).
	// The dialog shows it, like any time, as local time in this machine's zone; UT is calculated from that.
	auto info = Frame()->DefaultChartInfo();
	info.Time = m_Items[selected].Date.AddDays(0.5);
	info.TimeZone = TimeZones::Machine();
	Frame()->NewChartWithDialog(&info);

	return 0;
}

LRESULT CEphemerisView::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
	KillTimer(NowTimerId);
	handled = FALSE;
	return 0;
}

LRESULT CEphemerisView::OnTimer(UINT, WPARAM id, LPARAM, BOOL& handled) {
	if (id != NowTimerId) {
		handled = FALSE;
		return 0;
	}
	UpdateNowStrip();
	return 0;
}

LRESULT CEphemerisView::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&) {
	m_ColorOptions = WTLHelper::IsDarkMode() ? DarkColors : ColorOptions();
	m_NowStrip.Invalidate();
	m_List.RedrawWindow();
	return 0;
}

