#include "pch.h"
#include "ChartView.h"
#include "TimeZones.h"
#include "Helpers.h"
#include "Aspects.h"
#include "DefaultFont.h"
#include <filesystem>
#include <ToolbarHelper.h>
#include <DarkMode/DmlibColor.h>
#include <DarkMode/DarkModeSubclass.h>

CChartView::CChartView(IMainFrame* frame) : CFrameView(frame), m_ChartDrawing(frame), m_AspectGrid(frame), m_AspectList(frame) {
}

BOOL CChartView::PreTranslateMessage(MSG* pMsg) {
	return m_DetailsTabs.PreTranslateMessage(pMsg);
}

void CChartView::Chart(ChartData data) {
	AstroCalculator calc;
	calc.Calculate(data);
	m_Data = std::move(data);
	m_DetailsView.SetChartData(&m_Data);
	m_ChartDrawing.SetChartData(&m_Data);
	m_AspectGrid.SetChartData(&m_Data);

	AspectCalculator ac;
	auto aspects = ac.Calculate(m_Data.AllPlanets());
	m_AspectList.SetAspects(aspects);
	m_AspectList.Refresh();
	m_AspectGrid.SetAspects(aspects);
	m_AspectGrid.Refresh();
	UpdateAspectGridScrollSize();
	m_ChartDrawing.SetAspects(std::move(aspects));
	m_ChartDrawing.Refresh();
}

void CChartView::ChartForNow() {
	auto info = Frame()->DefaultChartInfo();
	info.Time = DateTime::Now();
	info.TimeZone = TimeZones::Machine();

	// The chart opens straight away; if the location hasn't arrived yet it is a
	// placeholder and the details view says so until WM_LOCATION_UPDATED.
	m_AwaitingLocation = Frame()->IsLocationPending();

	Chart(Helpers::CreateChartData(std::move(info)));
	m_DetailsView.SetLocationPending(m_AwaitingLocation);
}

LRESULT CChartView::OnLocationUpdated(UINT, WPARAM wParam, LPARAM, BOOL&) {
	if (!m_AwaitingLocation)
		return 0;

	m_AwaitingLocation = false;

	// Don't stomp a location the user typed while the lookup was still running.
	if (!wParam || m_DetailsView.IsLocationEdited()) {
		m_DetailsView.SetLocationPending(false);
		return 0;
	}

	auto const& source = Frame()->DefaultChartInfo();
	auto& info = m_Data.Info();
	info.Latitude = source.Latitude;
	info.Longitude = source.Longitude;
	info.Elevation = source.Elevation;
	info.City = source.City;
	info.State = source.State;
	info.Country = source.Country;

	// The time is deliberately left alone - it was captured when the chart was
	// created, and shifting it now would silently change the chart.
	m_NotModifying++;		// the program filled this in; the user hasn't edited anything
	SendMessage(WM_RECALC, static_cast<WPARAM>(Recalc::Houses));
	m_NotModifying--;

	// Has to come after the copy above: SetLocationPending refreshes the
	// latitude/longitude fields and the location text from the chart's info,
	// and UpdateControls does not touch them at all.
	m_DetailsView.SetLocationPending(false);
	m_DetailsView.UpdateControls();

	return 0;
}

ChartData const& CChartView::Chart() const {
	return m_Data;
}

void CChartView::DisplayPlanets(CDCHandle dc, int x, int y) const {
	CFont font;
	font.CreatePointFont(110, L"HamburgSymbols");
	dc.SelectFont(font);

	for (auto& p : m_Data.AllPlanets()) {
		dc.TextOut(x, y, DefaultFont::Get().GetPlanetGlyphAsString(p.Planet), -1);
		dc.TextOut(x + 20, y, Helpers::FormatLongitude(p.Longitude,
			FormatOptions::ShowDegreeGlyph | FormatOptions::ShowSeconds | FormatOptions::UseGlyphs), -1);
		y += 22;
	}
}

void CChartView::DisplayHouses(CDCHandle dc, int x, int y) const {
	CFont font;
	font.CreatePointFont(110, L"HamburgSymbols");
	dc.SelectFont(font);

	auto& houses = m_Data.Houses();
	int i = 1;
	CString text;
	dc.TextOut(x, y, L"Z  " + Helpers::FormatLongitude(houses.Asc, FormatOptions::ShowDegreeGlyph | FormatOptions::ShowSeconds | FormatOptions::UseGlyphs));
	y += 22;
	dc.TextOut(x, y, L"X  " + Helpers::FormatLongitude(houses.MC, FormatOptions::ShowDegreeGlyph | FormatOptions::ShowSeconds | FormatOptions::UseGlyphs));
	y += 44;

	for (auto& cusp : houses.Cusps) {
		text.Format(L"%02d.  %s", i++, (PCWSTR)Helpers::FormatLongitude(cusp, FormatOptions::ShowDegreeGlyph | FormatOptions::ShowSeconds | FormatOptions::UseGlyphs));
		dc.TextOut(x, y - 3, text, text.GetLength());
		y += 22;
	}
}

void CChartView::PageActivated(bool active) {
	// the Chart menu and the shortcuts apply to the chart page that is showing
	m_PageActive = active;
	auto& ui = Frame()->GetUI();
	// stepping by hand is off the table while auto step is running
	ui.UIEnable(ID_CHART_STEP_BACK, active && !m_AutoStep);
	ui.UIEnable(ID_CHART_STEP_FORWARD, active && !m_AutoStep);
	ui.UIEnable(ID_CHART_AUTOSTEP, active);
	ui.UIEnable(ID_FILE_SAVE, active);
	ui.UIEnable(ID_FILE_SAVE_AS, active);
	if (active)
		ui.UISetCheck(ID_CHART_AUTOSTEP, m_AutoStep);	// the menu and toolbars are shared; show this chart's state
	UpdateAutoStepTimer();
}

void CChartView::SetFile(PCWSTR title, PCWSTR filePath) {
	m_Title = title;
	m_FilePath = filePath ? filePath : L"";
	m_Modified = false;
	UpdateTitle();
}

PCWSTR CChartView::FilePath() const {
	return m_FilePath.IsEmpty() ? nullptr : (PCWSTR)m_FilePath;
}

void CChartView::SetModified(bool modified) {
	if (m_Modified == modified)
		return;
	m_Modified = modified;
	UpdateTitle();
}

void CChartView::UpdateTitle() {
	if (m_Title.IsEmpty())
		return;
	// a chart that has never been saved has no file to be out of step with, so it gets no marker
	CString title = m_Title;
	if (m_Modified && !m_FilePath.IsEmpty())
		title += L" *";
	Frame()->SetViewTitle(this, title);
}

bool CChartView::Save(bool saveAs) {
	CString path = m_FilePath;
	if (path.IsEmpty() || saveAs) {
		// offer the chart's own name, without the characters a file name can't have
		static const wchar_t invalid[] = { 92, L'/', L':', L'*', L'?', 34, L'<', L'>', L'|', 0 };
		CString suggestion = m_FilePath.IsEmpty() ? m_Title : CString(std::filesystem::path((PCWSTR)m_FilePath).stem().c_str());
		for (int i = 0; i < suggestion.GetLength(); i++)
			if (wcschr(invalid, suggestion[i]))
				suggestion.SetAt(i, L'_');

		CFileDialog dlg(FALSE, ChartFile::Extension, suggestion, OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER, ChartFile::Filter, m_hWnd);
		auto ok = dlg.DoModal(m_hWnd) == IDOK;
		if (!ok)
			return false;
		path = dlg.m_szFileName;
	}

	std::wstring error;
	if (!ChartFile::Save(m_Data, path, error)) {
		CString message;
		message.Format(L"The chart could not be saved to %s:\n\n%s", (PCWSTR)path, error.c_str());
		AtlMessageBox(m_hWnd, (PCWSTR)message, L"Astro Studio", MB_ICONWARNING);
		return false;
	}

	m_FilePath = path;
	m_Title = std::filesystem::path((PCWSTR)path).stem().c_str();
	m_Modified = false;
	UpdateTitle();
	Frame()->AddRecentFile(path);
	return true;
}

LRESULT CChartView::OnSave(WORD, WORD wID, HWND, BOOL&) {
	Save(wID == ID_FILE_SAVE_AS);
	return 0;
}

bool CChartView::CanClose() {
	if (!m_Modified)
		return true;

	Frame()->ActivateView(this);		// so the user can see which chart is being asked about
	CString message;
	message.Format(L"Save changes to \"%s\"?", (PCWSTR)m_Title);
	switch (AtlMessageBox(m_hWnd, (PCWSTR)message, L"Astro Studio", MB_YESNOCANCEL | MB_ICONQUESTION)) {
		case IDYES:
			return Save(false);		// false if the user cancelled the Save As or it failed: stay open
		case IDNO:
			return true;
	}
	return false;
}

void CChartView::CreateStepToolBar() {
	ToolBarButtonInfo buttons[] = {
		{ ID_CHART_STEP_BACK, IDI_BACK, BTNS_BUTTON, L"Back" },
		{ ID_CHART_STEP_FORWARD, IDI_FORWARD, BTNS_BUTTON, L"Forward" },
		{ 0 },
	};
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	CToolBarCtrl tb(ToolbarHelper::CreateAndInitToolBar(m_hWndToolBar, buttons, _countof(buttons)));

	// The label and the two combo boxes live inside the toolbar, on separators made as wide as they are.
	// These have to be there before the toolbar joins the rebar, which sizes the band from its buttons.
	int dpi = CClientDC(m_hWnd).GetDeviceCaps(LOGPIXELSX);
	auto px = [&](int value) { return MulDiv(value, dpi, 96); };
	int firstSlot = tb.GetButtonCount();
	tb.AddSeparator(px(40));	// "Step:"
	tb.AddSeparator(px(54));	// count
	tb.AddSeparator(px(94));	// unit

	// then the auto step check button and its interval
	tb.AddSeparator(px(10));
	CImageList images = tb.GetImageList();
	tb.AddButton(ID_CHART_AUTOSTEP, BTNS_CHECK | BTNS_SHOWTEXT, TBSTATE_ENABLED, images.AddIcon(AtlLoadIconImage(IDI_PLAY, 0, 24, 24)), L"Auto", 0);
	int intervalSlot = tb.GetButtonCount();
	tb.AddSeparator(px(84));	// interval

	AddSimpleReBarBand(tb);
	Frame()->AddToolBarToUI(tb);

	CFontHandle font(AtlGetDefaultGuiFont());
	const int comboHeight = px(22), dropHeight = px(220);
	// the closed combo box is comboHeight high and centered in the toolbar; dropHeight is room for its list
	auto slotRect = [&](int slot, int dropHeight) {
		CRect item;
		tb.GetItemRect(firstSlot + slot, &item);
		int top = item.top + (item.Height() - comboHeight) / 2;
		return CRect(item.left, top, item.right - px(4), top + comboHeight + dropHeight);
	};

	CRect rc = slotRect(0, 0);
	m_StepLabel.Create(tb, rc, L"Step:", WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE);
	m_StepLabel.SetFont(font);

	CRect countRect = slotRect(1, dropHeight);
	m_StepCount.Create(tb, countRect, nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, 0, IDC_STEPCOUNT);
	m_StepCount.SetFont(font);
	for (int i = 1; i <= 30; i++) {
		CString text;
		text.Format(L"%d", i);
		m_StepCount.AddString(text);
	}
	m_StepCount.SetCurSel(0);

	struct UnitItem {
		PCWSTR Text;
		StepUnit Unit;
	};
	const UnitItem units[] = {
		{ L"Minutes", StepUnit::Minute }, { L"Hours", StepUnit::Hour }, { L"Days", StepUnit::Day },
		{ L"Weeks", StepUnit::Week }, { L"Months", StepUnit::Month }, { L"Years", StepUnit::Year },
	};
	CRect unitRect = slotRect(2, dropHeight);
	m_StepUnit.Create(tb, unitRect, nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, 0, IDC_STEPUNIT);
	m_StepUnit.SetFont(font);
	for (auto const& unit : units) {
		int n = m_StepUnit.AddString(unit.Text);
		m_StepUnit.SetItemData(n, (DWORD_PTR)unit.Unit);
		if (unit.Unit == StepUnit::Day)
			m_StepUnit.SetCurSel(n);
	}

	struct IntervalItem {
		PCWSTR Text;
		UINT Milliseconds;
	};
	const IntervalItem intervals[] = {
		{ L"100 msec", 100 }, { L"200 msec", 200 }, { L"500 msec", 500 }, { L"1 sec", 1000 },
		{ L"2 sec", 2000 }, { L"3 sec", 3000 }, { L"5 sec", 5000 }, { L"10 sec", 10000 },
	};
	CRect intervalRect = slotRect(intervalSlot - firstSlot, dropHeight);
	m_StepInterval.Create(tb, intervalRect, nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, 0, IDC_STEPINTERVAL);
	m_StepInterval.SetFont(font);
	for (auto const& interval : intervals) {
		int n = m_StepInterval.AddString(interval.Text);
		m_StepInterval.SetItemData(n, interval.Milliseconds);
		if (interval.Milliseconds == 1000)
			m_StepInterval.SetCurSel(n);
	}
}

void CChartView::UpdateAutoStepTimer() {
	constexpr UINT_PTR AutoStepTimer = 1;
	KillTimer(AutoStepTimer);
	if (!m_AutoStep || !m_PageActive)
		return;

	int selected = m_StepInterval.m_hWnd ? m_StepInterval.GetCurSel() : -1;
	UINT interval = selected < 0 ? 1000 : (UINT)m_StepInterval.GetItemData(selected);
	SetTimer(AutoStepTimer, interval);
}

void CChartView::SetAutoStep(bool on) {
	m_AutoStep = on;
	if (m_PageActive) {
		auto& ui = Frame()->GetUI();
		ui.UISetCheck(ID_CHART_AUTOSTEP, on);
		ui.UIEnable(ID_CHART_STEP_BACK, !on);
		ui.UIEnable(ID_CHART_STEP_FORWARD, !on);
	}
	UpdateAutoStepTimer();
}

LRESULT CChartView::OnAutoStep(WORD, WORD, HWND, BOOL&) {
	SetAutoStep(!m_AutoStep);
	return 0;
}

LRESULT CChartView::OnIntervalChanged(WORD, WORD, HWND, BOOL&) {
	UpdateAutoStepTimer();		// restarts the timer with the new interval
	return 0;
}

LRESULT CChartView::OnTimer(UINT, WPARAM wParam, LPARAM, BOOL& handled) {
	if (wParam != 1) {
		handled = FALSE;
		return 0;
	}
	// every tick is a step forward; running out of years switches auto step off
	m_NotModifying++;		// ticks aren't edits (the chart is only being watched moving)
	bool stepped = StepTime(1);
	m_NotModifying--;
	if (!stepped)
		SetAutoStep(false);
	return 0;
}

bool CChartView::StepTime(int direction) {
	if (m_Data.AllPlanets().empty() || m_StepCount.m_hWnd == nullptr)
		return false;		// no chart loaded yet

	int selected = m_StepCount.GetCurSel();
	int count = (selected < 0 ? 0 : selected) + 1;
	int unitIndex = m_StepUnit.GetCurSel();
	auto unit = unitIndex < 0 ? StepUnit::Day : (StepUnit)m_StepUnit.GetItemData(unitIndex);

	auto& info = m_Data.Info();
	DateTime ut = info.Time;
	TimeZoneInfo tz = info.TimeZone;
	if (!TimeStep::Step(ut, tz, unit, direction * count)) {
		::MessageBeep(MB_ICONWARNING);		// past the years the ephemeris covers
		return false;
	}
	info.Time = ut;
	info.TimeZone = tz;

	SendMessage(WM_RECALC, static_cast<WPARAM>(Recalc::All));
	m_DetailsView.UpdateControls();
	return true;
}

LRESULT CChartView::OnStep(WORD, WORD wID, HWND, BOOL&) {
	// a disabled menu item or button doesn't stop the Alt+Left/Right shortcuts
	if (m_AutoStep)
		return 0;
	StepTime(wID == ID_CHART_STEP_FORWARD ? 1 : -1);
	return 0;
}

LRESULT CChartView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, WS_EX_TRANSPARENT);
	m_ChartDrawing.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_ChartDrawing.SetStatic();

	m_DetailsTabs.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

	m_DetailsView.Create(m_DetailsTabs);
	m_DetailsView.SetNotifyWindow(m_hWnd);

	m_AspectGridScroll.Create(m_DetailsTabs, rcDefault, nullptr, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_HSCROLL | WS_VSCROLL);
	m_AspectGrid.Create(m_AspectGridScroll, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_AspectGrid.SetStatic();
	m_AspectGridScroll.SetClient(m_AspectGrid);

	m_AspectList.Create(m_DetailsTabs, rcDefault, nullptr, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_AspectList.SetStatic();

	m_DetailsTabs.AddPage(m_DetailsView.m_hWnd, L"Details");
	m_DetailsTabs.AddPage(m_AspectGridScroll.m_hWnd, L"Aspect Grid");
	m_DetailsTabs.AddPage(m_AspectList.m_hWnd, L"Aspect List");
	m_DetailsTabs.SetActivePage(0);

	m_Splitter.SetSplitterPanes(m_ChartDrawing, m_DetailsTabs);
	m_Splitter.SetSplitterPosPct(50);

	CreateStepToolBar();

	DarkMode::setDarkWndNotifySafe(m_hWnd);
	UpdateAspectGridScrollBarTheme();

	return 0;
}

LRESULT CChartView::OnEditCopy(WORD, WORD, HWND, BOOL&) {
	return 0;
}

LRESULT CChartView::OnForwardMsg(UINT, WPARAM, LPARAM lParam, BOOL& bHandled) {
	bHandled = TRUE;
	return m_DetailsTabs.PreTranslateMessage((LPMSG)lParam);
}

LRESULT CChartView::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&) {
	UpdateAspectGridScrollBarTheme();
	return 0;
}

void CChartView::UpdateAspectGridScrollSize() {
	auto size = m_AspectGrid.NaturalSize();
	m_AspectGridScroll.SetScrollSize(size, size);
	m_AspectGridScroll.UpdateLayout();
}

void CChartView::UpdateAspectGridScrollBarTheme() {
	// CScrollContainer is a plain custom-class window, not a recognized common control,
	// so DarkMode::setDarkWndNotifySafe's generic child-control theming doesn't reach its
	// native scroll bars - they need to be themed explicitly.
	DarkMode::setDarkScrollBar(m_AspectGridScroll.m_hWnd);
	m_AspectGridScroll.Invalidate();
}

LRESULT CChartView::OnRecalc(UINT, WPARAM wp, LPARAM, BOOL&) {
	if (m_NotModifying == 0)
		SetModified(true);
	switch (static_cast<Recalc>(wp)) {
	case Recalc::Houses:
		m_Data.CalcHouses(m_Calc);
		break;

	case Recalc::Planets:
		m_Data.CalcPlanets(m_Calc);
		break;

	case Recalc::All:
		m_Data.CalcPlanets(m_Calc);
		m_Data.CalcHouses(m_Calc);
		break;
	}
	AspectCalculator ac;
	auto aspects = ac.Calculate(m_Data.AllPlanets());
	m_AspectList.SetAspects(aspects);
	m_AspectList.Refresh();
	m_AspectGrid.SetAspects(aspects);
	m_AspectGrid.Refresh();
	UpdateAspectGridScrollSize();
	m_ChartDrawing.SetAspects(std::move(aspects));
	m_ChartDrawing.Refresh();
	return 0;
}

