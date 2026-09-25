#include "pch.h"
#include "ChartView.h"
#include "TimeZones.h"
#include "Helpers.h"
#include "Aspects.h"
#include "DefaultFont.h"
#include "AppSettings.h"
#include "AspectOptions.h"
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
	UpdateAspects();
}

void CChartView::UpdateAspects() {
	AspectCalculator ac(AspectOptions::Current().Chart);
	auto aspects = ac.Calculate(m_Data.AllPlanets());
	m_AspectList.SetAspects(aspects);
	m_AspectList.Refresh();
	m_AspectGrid.SetAspects(aspects);
	m_AspectGrid.Refresh();
	UpdateAspectGridScrollSize();
	m_ChartDrawing.SetAspects(std::move(aspects));
	m_ChartDrawing.Refresh();
	if (m_Overlay)
		UpdateOverlay();		// its aspects are to this chart
}

void CChartView::WheelOptionsChanged() {
	m_ChartDrawing.Refresh();
}

void CChartView::AspectSettingsChanged() {
	UpdateAspects();
}

void CChartView::DerivedChart(ChartData data) {
	m_ReadOnly = true;
	m_Data = std::move(data);
	m_DetailsView.SetChartData(&m_Data);
	m_DetailsView.SetReadOnly();
	m_ChartDrawing.SetChartData(&m_Data);
	m_AspectGrid.SetChartData(&m_Data);
	UpdateAspects();
	if (m_PageActive)
		PageActivated(true);		// (the frame did that when the page was added, before this chart was known to be read-only)
}

void CChartView::ChartForNow() {
	auto info = Frame()->DefaultChartInfo();
	info.Time = DateTime::Now();
	info.TimeZone = TimeZones::Machine();

	// The chart opens straight away; if the location hasn't arrived yet it is a
	// placeholder and the details view says so until WM_LOCATION_UPDATED.
	m_AwaitingLocation = Frame()->IsLocationPending();

	Chart(Helpers::CreateChartData(std::move(info), static_cast<HouseSystem>(AppSettings::Get().LastHouseSystem())));
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
	ui.UIEnable(ID_CHART_STEP_BACK, active && CanMoveTime() && !m_AutoStep && !m_Live);
	ui.UIEnable(ID_CHART_STEP_FORWARD, active && CanMoveTime() && !m_AutoStep && !m_Live);
	ui.UIEnable(ID_CHART_AUTOSTEP, active && CanMoveTime());
	ui.UIEnable(ID_CHART_LIVE, active && CanMoveTime());
	// (charts worked out from others have nothing to progress, return to or combine)
	ui.UIEnable(ID_CHART_OVERLAY_PROGRESSED, active && !m_ReadOnly);
	ui.UIEnable(ID_CHART_OVERLAY_SOLARARC, active && !m_ReadOnly);
	for (UINT id : { ID_CHART_DERIVED_SOLARRETURN, ID_CHART_DERIVED_LUNARRETURN, ID_CHART_DERIVED_COMPOSITE, ID_CHART_DERIVED_DAVISON })
		ui.UIEnable(id, active && !m_ReadOnly);
	for (UINT id : { ID_CHART_OVERLAY, ID_CHART_TRANSITS, ID_CHART_OVERLAY_NONE, ID_CHART_OVERLAY_SYNASTRY })
		ui.UIEnable(id, active);
	ui.UIEnable(ID_FILE_SAVE, active && !m_ReadOnly);
	ui.UIEnable(ID_FILE_SAVE_AS, active && !m_ReadOnly);
	ui.UIEnable(ID_FILE_EXPORT, active);
	if (active) {
		// the menu and toolbars are shared; show this chart's state
		ui.UISetCheck(ID_CHART_AUTOSTEP, m_AutoStep);
		ui.UISetCheck(ID_CHART_LIVE, m_Live);
	}
	UpdateOverlayUI();
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

// the name without the characters a file name can't have
static CString SafeFileName(CString name) {
	static const wchar_t invalid[] = { 92, L'/', L':', L'*', L'?', 34, L'<', L'>', L'|', 0 };
	for (int i = 0; i < name.GetLength(); i++)
		if (wcschr(invalid, name[i]))
			name.SetAt(i, L'_');
	return name;
}

bool CChartView::Save(bool saveAs) {
	CString path = m_FilePath;
	if (path.IsEmpty() || saveAs) {
		// offer the chart's own name
		auto suggestion = SafeFileName(m_FilePath.IsEmpty() ? m_Title : CString(std::filesystem::path((PCWSTR)m_FilePath).stem().c_str()));

		CSimpleFileDialog dlg(FALSE, ChartFile::Extension, suggestion, OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING, ChartFile::Filter, m_hWnd);
		WTLHelper::SuspendHook();
		auto ok = dlg.DoModal(m_hWnd) == IDOK;
		WTLHelper::ResumeHook();
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
	if (m_ReadOnly)
		return 0;
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

	// and Live, which keeps the chart at the current time
	tb.AddSeparator(px(10));
	tb.AddButton(ID_CHART_LIVE, BTNS_CHECK | BTNS_SHOWTEXT, TBSTATE_ENABLED, images.AddIcon(AtlLoadIconImage(IDI_CLOCK_REFRESH, 0, 24, 24)), L"Live", 0);
	// (checked while an overlay is shown; the drop-down is the same choice as the Chart > Overlay menu)
	tb.AddButton(ID_CHART_OVERLAY, BTNS_CHECK | BTNS_WHOLEDROPDOWN | BTNS_SHOWTEXT, TBSTATE_ENABLED, images.AddIcon(AtlLoadIconImage(IDI_HOURGLASS, 0, 24, 24)), L"Overlay", 0);

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
	m_StepCount.SetCurSel(std::clamp(AppSettings::Get().ChartStepCount(), 1, 30) - 1);

	struct UnitItem {
		PCWSTR Text;
		StepUnit Unit;
	};
	const UnitItem units[] = {
		{ L"Seconds", StepUnit::Second }, { L"Minutes", StepUnit::Minute }, { L"Hours", StepUnit::Hour }, { L"Days", StepUnit::Day },
		{ L"Weeks", StepUnit::Week }, { L"Months", StepUnit::Month }, { L"Years", StepUnit::Year },
	};
	CRect unitRect = slotRect(2, dropHeight);
	m_StepUnit.Create(tb, unitRect, nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, 0, IDC_STEPUNIT);
	m_StepUnit.SetFont(font);
	for (auto const& unit : units) {
		int n = m_StepUnit.AddString(unit.Text);
		m_StepUnit.SetItemData(n, (DWORD_PTR)unit.Unit);
		if (static_cast<int>(unit.Unit) == AppSettings::Get().ChartStepUnit())
			m_StepUnit.SetCurSel(n);
	}
	if (m_StepUnit.GetCurSel() < 0)
		m_StepUnit.SetCurSel(m_StepUnit.FindStringExact(-1, L"Days"));

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
		if ((int)interval.Milliseconds == AppSettings::Get().ChartStepInterval())
			m_StepInterval.SetCurSel(n);
	}
	if (m_StepInterval.GetCurSel() < 0)
		m_StepInterval.SetCurSel(m_StepInterval.FindStringExact(-1, L"1 sec"));
}

void CChartView::UpdateAutoStepTimer() {
	constexpr UINT_PTR AutoStepTimer = 1;
	KillTimer(AutoStepTimer);
	if ((!m_AutoStep && !m_Live) || !m_PageActive)
		return;

	int selected = m_StepInterval.m_hWnd ? m_StepInterval.GetCurSel() : -1;
	UINT interval = selected < 0 ? 1000 : (UINT)m_StepInterval.GetItemData(selected);
	SetTimer(AutoStepTimer, interval);
}

void CChartView::UpdateStepUI() {
	if (!m_PageActive)
		return;
	auto& ui = Frame()->GetUI();
	ui.UISetCheck(ID_CHART_AUTOSTEP, m_AutoStep);
	ui.UISetCheck(ID_CHART_LIVE, m_Live);
	bool canMove = CanMoveTime();
	ui.UIEnable(ID_CHART_AUTOSTEP, canMove);
	ui.UIEnable(ID_CHART_LIVE, canMove);
	ui.UIEnable(ID_CHART_STEP_BACK, canMove && !m_AutoStep && !m_Live);
	ui.UIEnable(ID_CHART_STEP_FORWARD, canMove && !m_AutoStep && !m_Live);
}

void CChartView::SetAutoStep(bool on) {
	if (on && !CanMoveTime())
		return;
	if (on)
		m_Live = false;
	m_AutoStep = on;
	UpdateStepUI();
	UpdateAutoStepTimer();
}

void CChartView::SetLive(bool on) {
	if (on && !CanMoveTime())
		return;
	if (on)
		m_AutoStep = false;
	m_Live = on;
	UpdateStepUI();
	UpdateAutoStepTimer();
	if (on)
		TickLive();		// no waiting for the first tick
}

namespace {
	// a moment (UT) at the current time, and the zone it is shown in with the offset in effect now
	void SetToNow(DateTime& time, TimeZoneInfo& zone) {
		time = DateTime::Now();
		int offset = zone.OffsetUT;
		TimeZones::UtToLocal(time, zone, &offset);
		zone.OffsetUT = offset;
	}
}

void CChartView::TickLive() {
	if (m_Data.AllPlanets().empty())
		return;
	if (OverlayHasTime()) {
		SetToNow(m_Overlay->When, m_Overlay->Zone);
		UpdateOverlay();
		return;
	}
	auto& info = m_Data.Info();
	SetToNow(info.Time, info.TimeZone);
	m_NotModifying++;		// following the clock isn't an edit
	SendMessage(WM_RECALC, static_cast<WPARAM>(Recalc::All));
	m_NotModifying--;
	m_DetailsView.UpdateControls();
}

bool CChartView::GetChart(OpenChart& chart) const {
	if (m_Data.AllPlanets().empty())
		return false;
	chart.Name = m_Title.IsEmpty() ? CString(L"Chart") : m_Title;
	chart.Data = m_Data;
	return true;
}

bool CChartView::PickOtherChart(OpenChart& chart, PCWSTR what) {
	auto charts = Frame()->OpenCharts(this);
	if (charts.empty()) {
		CString message;
		message.Format(L"Open another chart first: %s.", what);
		AtlMessageBox(m_hWnd, (PCWSTR)message, L"Astro Studio", MB_ICONINFORMATION);
		return false;
	}
	size_t chosen = 0;
	if (charts.size() > 1) {
		CMenu menu;
		menu.CreatePopupMenu();
		for (size_t i = 0; i < charts.size(); i++)
			menu.AppendMenu(MF_STRING, i + 1, charts[i].Name);
		POINT pt;
		::GetCursorPos(&pt);
		auto choice = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, m_hWnd);
		if (choice <= 0)
			return false;
		chosen = choice - 1;
	}
	chart = std::move(charts[chosen]);
	return true;
}

void CChartView::NewReturnChart(Planet planet) {
	// a return near the moment shown: the overlay's if it has one, otherwise now
	DateTime from = OverlayHasTime() ? m_Overlay->When : DateTime::Now();
	auto found = DerivedCharts::FindReturn(m_Calc, m_Data, planet, from, ReturnSearch::Nearest);
	if (!found) {
		AtlMessageBox(m_hWnd, L"No return was found.", L"Astro Studio", MB_ICONWARNING);
		return;
	}

	// The dialog opens for that moment at this chart's place and in its zone, to be looked over (or moved: a return can be
	// cast for where the person is then) before the chart opens.
	ChartInfo info = m_Data.Info();
	info.Type = InfoType::Event;
	info.Time = *found;
	int offset = info.TimeZone.OffsetUT;
	auto local = TimeZones::UtToLocal(info.Time, info.TimeZone, &offset);
	info.TimeZone.OffsetUT = offset;
	CString name;
	if (planet == Planet::Sun)
		name.Format(L"%s Solar Return %04ld", (PCWSTR)m_Title, local.Year);
	else
		name.Format(L"%s Lunar Return %04ld/%02ld/%02ld", (PCWSTR)m_Title, local.Year, local.Month, local.Day);
	info.FirstName = (PCWSTR)name;
	info.MiddleName.clear();
	info.LastName.clear();

	auto houses = m_Data.GetHouseSystem();
	Frame()->NewChartWithDialog(&info, &houses);
}

void CChartView::NewPairChart(bool davison) {
	OpenChart other;
	if (!PickOtherChart(other, davison ? L"a Davison chart is made from two charts" : L"a composite is made from two charts"))
		return;
	ChartData chart = davison ? DerivedCharts::Davison(m_Calc, m_Data, other.Data) : DerivedCharts::Composite(m_Calc, m_Data, other.Data);
	CString title;
	title.Format(L"%s: %s + %s", davison ? L"Davison" : L"Composite", (PCWSTR)m_Title, (PCWSTR)other.Name);
	Frame()->AddDerivedChartView(std::move(chart), title);
}

LRESULT CChartView::OnDerived(WORD, WORD id, HWND, BOOL&) {
	if (m_ReadOnly || m_Data.AllPlanets().empty())
		return 0;
	switch (id) {
		case ID_CHART_DERIVED_SOLARRETURN: NewReturnChart(Planet::Sun); break;
		case ID_CHART_DERIVED_LUNARRETURN: NewReturnChart(Planet::Moon); break;
		case ID_CHART_DERIVED_COMPOSITE: NewPairChart(false); break;
		case ID_CHART_DERIVED_DAVISON: NewPairChart(true); break;
	}
	return 0;
}

bool CChartView::ShowOverlay(OverlayKind kind, ProgressionMethod method) {
	ChartOverlay overlay;
	overlay.Kind = kind;
	overlay.Method = method;
	overlay.Label = ChartOverlay::DefaultLabel(kind);
	if (kind == OverlayKind::Progressed && method == ProgressionMethod::SolarArc)
		overlay.Label = L"Directed";

	if (kind == OverlayKind::Progressed && m_ReadOnly)
		return false;		// (a chart worked out from others has no birth to progress from)

	if (kind == OverlayKind::Synastry) {
		OpenChart other;
		if (!PickOtherChart(other, L"a synastry overlay shows one chart around another"))
			return false;
		// a copy: it shows the other chart as it is now, and stays that way if the other one is changed or closed
		overlay.Data = std::move(other.Data);
		overlay.Label = other.Name;
		overlay.BaseLabel = m_Title.IsEmpty() ? L"chart" : (PCWSTR)m_Title;
	}
	else {
		// at the current time, in the chart's zone
		overlay.Zone = m_Data.Info().TimeZone;
		SetToNow(overlay.When, overlay.Zone);
		if (kind == OverlayKind::Transit) {
			// the same planets as the chart's, with the chart's place
			std::vector<Planet> planets;
			for (auto const& p : m_Data.AllPlanets())
				planets.push_back(p.Planet);
			overlay.Data.AddPlanets(planets);
			overlay.Data.Info() = m_Data.Info();
		}
	}
	m_Overlay = std::move(overlay);
	StopTimeIfFixed();
	UpdateOverlayUI();
	UpdateOverlay();
	return true;
}

void CChartView::HideOverlay() {
	m_Overlay.reset();
	StopTimeIfFixed();
	UpdateOverlayUI();
	UpdateOverlay();
}

void CChartView::StopTimeIfFixed() {
	// a read-only chart's time only moves through an overlay's: without one, Live and Auto have nothing to move
	if (!CanMoveTime()) {
		m_AutoStep = m_Live = false;
		UpdateAutoStepTimer();
	}
	UpdateStepUI();
}

void CChartView::UpdateOverlayUI() {
	if (!m_PageActive)
		return;
	auto& ui = Frame()->GetUI();
	auto kind = m_Overlay ? std::optional(m_Overlay->Kind) : std::nullopt;
	bool solarArc = m_Overlay && m_Overlay->Method == ProgressionMethod::SolarArc;
	ui.UISetCheck(ID_CHART_OVERLAY, m_Overlay.has_value());
	ui.UISetCheck(ID_CHART_OVERLAY_NONE, !m_Overlay);
	ui.UISetCheck(ID_CHART_TRANSITS, kind == OverlayKind::Transit);
	ui.UISetCheck(ID_CHART_OVERLAY_PROGRESSED, kind == OverlayKind::Progressed && !solarArc);
	ui.UISetCheck(ID_CHART_OVERLAY_SOLARARC, kind == OverlayKind::Progressed && solarArc);
	ui.UISetCheck(ID_CHART_OVERLAY_SYNASTRY, kind == OverlayKind::Synastry);
}

void CChartView::UpdateOverlay() {
	if (!m_Overlay) {
		m_ChartDrawing.ClearOverlay();
		return;
	}

	auto& overlay = *m_Overlay;
	CString caption;
	// the moment, as the chart's own time is shown: in its zone
	auto when = [&] {
		int offset = overlay.Zone.OffsetUT;
		auto local = TimeZones::UtToLocal(overlay.When, overlay.Zone, &offset);
		CString text;
		text.Format(L"%04ld/%02ld/%02ld  %02ld:%02ld:%02ld  (UTC%s)", local.Year, local.Month, local.Day, local.Hour, local.Minute,
			local.Second, (PCWSTR)TimeZones::FormatOffset(offset));
		return text;
	};

	switch (overlay.Kind) {
		case OverlayKind::Transit: {
			auto& info = overlay.Data.Info();
			info.Time = overlay.When;
			info.TimeZone = overlay.Zone;
			overlay.Data.Harmonic(m_Data.Harmonic());
			overlay.Data.CalcPlanets(m_Calc);
			caption = L"Transits  " + when();
			break;
		}
		case OverlayKind::Progressed: {
			ProgressionOptions options;
			options.Method = overlay.Method;
			overlay.Data = DerivedCharts::Progress(m_Calc, m_Data, overlay.When, options);
			if (overlay.Method == ProgressionMethod::SolarArc) {
				CString arc;
				arc.Format(L"  arc %.2f\u00b0", DerivedCharts::Arc(m_Calc, m_Data, overlay.When, ArcKey::Actual));
				caption = L"Solar arc directions to  " + when() + arc;
			}
			else {
				CString age;
				age.Format(L"  age %.2f", DerivedCharts::YearsBetween(m_Data.Info().Time, overlay.When));
				caption = L"Secondary progressions to  " + when() + age;
			}
			break;
		}
		case OverlayKind::Synastry:
			caption = (overlay.Label + L" around " + overlay.BaseLabel).c_str();
			break;
	}
	overlay.Caption = caption;

	// (by default with tight orbs and the major aspects only)
	AspectCalculator calc(AspectOptions::Current().Transit);
	overlay.Aspects = calc.CalcBetween(overlay.Data.AllPlanets(), m_Data.AllPlanets());

	m_ChartDrawing.SetOverlay(&overlay);
	m_ChartDrawing.Refresh();
}

LRESULT CChartView::OnOverlay(WORD, WORD id, HWND, BOOL&) {
	switch (id) {
		// choosing what is already shown changes nothing (it would only take the moment back to now)
		case ID_CHART_TRANSITS:
			if (!m_Overlay || m_Overlay->Kind != OverlayKind::Transit)
				ShowOverlay(OverlayKind::Transit);
			break;
		case ID_CHART_OVERLAY_PROGRESSED:
			if (!m_Overlay || m_Overlay->Kind != OverlayKind::Progressed || m_Overlay->Method != ProgressionMethod::Secondary)
				ShowOverlay(OverlayKind::Progressed, ProgressionMethod::Secondary);
			break;
		case ID_CHART_OVERLAY_SOLARARC:
			if (!m_Overlay || m_Overlay->Kind != OverlayKind::Progressed || m_Overlay->Method != ProgressionMethod::SolarArc)
				ShowOverlay(OverlayKind::Progressed, ProgressionMethod::SolarArc);
			break;
		case ID_CHART_OVERLAY_SYNASTRY:
			ShowOverlay(OverlayKind::Synastry);
			break;
		default:
			HideOverlay();
			break;
	}
	UpdateOverlayUI();		// (also puts the marks back if nothing was chosen)
	return 0;
}

LRESULT CChartView::OnOverlayDropDown(int, LPNMHDR pnmh, BOOL& handled) {
	auto nmtb = reinterpret_cast<NMTOOLBAR*>(pnmh);
	if (nmtb->iItem != ID_CHART_OVERLAY) {
		handled = FALSE;
		return 0;
	}

	// the same choices as the Chart menu's, marked the same way
	struct Choice {
		UINT Id;
		PCWSTR Text;
		bool Checked;
		bool Enabled{ true };
	};
	auto kind = m_Overlay ? std::optional(m_Overlay->Kind) : std::nullopt;
	bool solarArc = m_Overlay && m_Overlay->Method == ProgressionMethod::SolarArc;
	Choice choices[] = {
		{ ID_CHART_OVERLAY_NONE, L"None", !m_Overlay },
		{ ID_CHART_TRANSITS, L"Transits", kind == OverlayKind::Transit },
		{ ID_CHART_OVERLAY_PROGRESSED, L"Secondary Progressions", kind == OverlayKind::Progressed && !solarArc, !m_ReadOnly },
		{ ID_CHART_OVERLAY_SOLARARC, L"Solar Arc Directions", kind == OverlayKind::Progressed && solarArc, !m_ReadOnly },
		{ ID_CHART_OVERLAY_SYNASTRY, L"Synastry...", kind == OverlayKind::Synastry },
	};
	CMenu menu;
	menu.CreatePopupMenu();
	for (auto const& choice : choices)
		menu.AppendMenu(MF_STRING | (choice.Checked ? MF_CHECKED : 0) | (choice.Enabled ? 0 : MF_GRAYED), choice.Id, choice.Text);

	// under the button; the choice comes back as a command, like one from the menu bar
	CPoint pt(nmtb->rcButton.left, nmtb->rcButton.bottom);
	::ClientToScreen(nmtb->hdr.hwndFrom, &pt);
	Frame()->TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y);
	return TBDDRET_DEFAULT;
}

LRESULT CChartView::OnLive(WORD, WORD, HWND, BOOL&) {
	SetLive(!m_Live);
	return 0;
}

LRESULT CChartView::OnAutoStep(WORD, WORD, HWND, BOOL&) {
	SetAutoStep(!m_AutoStep);
	return 0;
}

LRESULT CChartView::OnIntervalChanged(WORD, WORD, HWND, BOOL&) {
	SaveStepSettings();
	UpdateAutoStepTimer();		// restarts the timer with the new interval
	return 0;
}

LRESULT CChartView::OnStepSettingChanged(WORD, WORD, HWND, BOOL&) {
	SaveStepSettings();
	return 0;
}

void CChartView::SaveStepSettings() {
	auto& settings = AppSettings::Get();
	if (int count = m_StepCount.GetCurSel(); count >= 0)
		settings.ChartStepCount(count + 1);
	if (int unit = m_StepUnit.GetCurSel(); unit >= 0)
		settings.ChartStepUnit(static_cast<int>(m_StepUnit.GetItemData(unit)));
	if (int interval = m_StepInterval.GetCurSel(); interval >= 0)
		settings.ChartStepInterval(static_cast<int>(m_StepInterval.GetItemData(interval)));
}

LRESULT CChartView::OnTimer(UINT, WPARAM wParam, LPARAM, BOOL& handled) {
	if (wParam != 1) {
		handled = FALSE;
		return 0;
	}
	if (m_Live) {
		TickLive();
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
	if (m_Data.AllPlanets().empty() || m_StepCount.m_hWnd == nullptr || !CanMoveTime())
		return false;		// no chart loaded yet, or a read-only one

	int selected = m_StepCount.GetCurSel();
	int count = (selected < 0 ? 0 : selected) + 1;
	int unitIndex = m_StepUnit.GetCurSel();
	auto unit = unitIndex < 0 ? StepUnit::Day : (StepUnit)m_StepUnit.GetItemData(unitIndex);

	// the overlay's moment if it has one, otherwise the chart's own
	bool overlay = OverlayHasTime();
	DateTime& time = overlay ? m_Overlay->When : m_Data.Info().Time;
	TimeZoneInfo& zone = overlay ? m_Overlay->Zone : m_Data.Info().TimeZone;
	DateTime ut = time;
	TimeZoneInfo tz = zone;
	if (!TimeStep::Step(ut, tz, unit, direction * count)) {
		::MessageBeep(MB_ICONWARNING);		// past the years the ephemeris covers
		return false;
	}
	time = ut;
	zone = tz;

	if (overlay) {
		UpdateOverlay();
		return true;
	}
	SendMessage(WM_RECALC, static_cast<WPARAM>(Recalc::All));
	m_DetailsView.UpdateControls();
	return true;
}

LRESULT CChartView::OnStep(WORD, WORD wID, HWND, BOOL&) {
	// a disabled menu item or button doesn't stop the Alt+Left/Right shortcuts
	if (m_AutoStep || m_Live)
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
	// in a text box (the details view has several) Copy is the text box's own
	HWND focus = ::GetFocus();
	wchar_t className[16]{};
	if (focus)
		::GetClassName(focus, className, _countof(className));
	if (_wcsicmp(className, L"Edit") == 0) {
		::SendMessage(focus, WM_COPY, 0, 0);
		return 0;
	}

	// anywhere else it is the chart wheel, as a picture
	auto image = m_ChartDrawing.RenderImage(ChartImage::DefaultSize);
	if (!image || !ChartImage::CopyToClipboard(m_hWnd, image))
		AtlMessageBox(m_hWnd, L"The chart could not be copied to the clipboard.", L"Astro Studio", MB_ICONWARNING);
	return 0;
}

LRESULT CChartView::OnExport(WORD, WORD, HWND, BOOL&) {
	static constexpr wchar_t filter[] = L"PNG pictures (*.png)\0*.png\0";
	CSimpleFileDialog dlg(FALSE, L"png", SafeFileName(m_Title), OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING, filter, m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (!ok)
		return 0;

	std::wstring error;
	if (auto image = m_ChartDrawing.RenderImage(ChartImage::DefaultSize); !image)
		error = L"The chart could not be drawn.";
	else if (ChartImage::SavePng(image, dlg.m_szFileName, error))
		return 0;

	CString message;
	message.Format(L"The picture could not be saved to %s:\n\n%s", dlg.m_szFileName, error.c_str());
	AtlMessageBox(m_hWnd, (PCWSTR)message, L"Astro Studio", MB_ICONWARNING);
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
	if (m_ReadOnly) {
		// what a read-only chart shows isn't what the calculator would make of its details
		UpdateAspects();
		return 0;
	}
	if (m_NotModifying == 0) {
		SetModified(true);
		if (m_Live)
			SetLive(false);		// the user took over the time, or the place
	}
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
	UpdateAspects();
	return 0;
}

