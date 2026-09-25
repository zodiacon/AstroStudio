#include "pch.h"
#include "ChartView.h"
#include "TimeZones.h"
#include "DirectionsDlg.h"
#include "Helpers.h"
#include "Aspects.h"
#include "DefaultFont.h"
#include "AppSettings.h"
#include "AspectOptions.h"
#include "Printing.h"
#include "ChartImage.h"
#include "StringHelper.h"
#include "DerivedCharts.h"
#include <filesystem>
#include <ToolbarHelper.h>
#include <DarkMode/DmlibColor.h>
#include <DarkMode/DarkModeSubclass.h>

CChartView::CChartView(IMainFrame* frame) : CFrameView(frame), m_ChartDrawing(frame), m_AspectGrid(frame), m_AspectList(frame), m_MidpointList(frame), m_PartList(frame) {
}

BOOL CChartView::PreTranslateMessage(MSG* pMsg) {
	return m_DetailsTabs.PreTranslateMessage(pMsg);
}

void CChartView::Chart(ChartData data) {
	AstroCalculator calc;
	calc.Calculate(data);
	m_Data = std::move(data);
	m_ExtraAdded.clear();
	SyncExtraBodies();
	SyncPartOfFortune();
	m_DetailsView.SetChartData(&m_Data);
	m_ChartDrawing.SetChartData(&m_Data);
	m_AspectGrid.SetChartData(&m_Data);
	UpdateAspects();
}

bool CChartView::SyncExtraBodies() {
	if (m_ReadOnly)
		return false;
	bool changed = false;
	auto& planets = m_Data.AllPlanets();
	auto options = WheelOptions::Current();
	for (int i = 0; i < static_cast<int>(Planet::NumPlanets); i++) {
		auto planet = static_cast<Planet>(i);
		if (!WheelOptions::IsExtra(planet))
			continue;
		bool has = std::ranges::find(planets, planet, &PlanetPosition::Planet) != planets.end();
		if (options.WantsExtra(planet) && !has) {
			planets.push_back(m_Calc.CalcPlanet(planet, m_Data.Info().Time, m_Data.Harmonic()));
			m_ExtraAdded.insert(planet);
			changed = true;
		}
		else if (!options.WantsExtra(planet) && has && m_ExtraAdded.contains(planet)) {
			m_Data.RemovePlanets({ planet });
			m_ExtraAdded.erase(planet);
			changed = true;
		}
	}
	return changed;
}

void CChartView::SyncPartOfFortune() {
	auto& planets = m_Data.AllPlanets();
	auto has = std::ranges::find(planets, Planet::PartOfFortune, &PlanetPosition::Planet) != planets.end();
	bool want = AppSettings::Get().ShowPartOfFortune() != 0;
	if (want && !has) {
		m_Data.AddPlanets({ Planet::PartOfFortune });
		m_Data.UpdatePartOfFortune(&m_Calc);
	}
	else if (!want && has)
		m_Data.RemovePlanets({ Planet::PartOfFortune });
}

void CChartView::PartOfFortuneChanged() {
	if (m_Data.AllPlanets().empty())
		return;
	SyncPartOfFortune();
	m_DetailsView.UpdateControls(Recalc::Planets);		// (the list of planets is made again)
	UpdateAspects();
}

void CChartView::UpdateAspects() {
	AspectCalculator ac(AspectOptions::Current().Chart);
	auto aspects = ac.Calculate(m_Data.AllPlanets());
	m_MidpointList.SetChartData(&m_Data);
	m_MidpointTree.SetChartData(&m_Data);
	m_PartList.SetChartData(&m_Data, AspectOptions::Current().Chart);
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
	if (!m_Data.AllPlanets().empty() && SyncExtraBodies()) {
		// the chart has other planets now: everything that lists them starts over
		m_DetailsView.UpdateControls(Recalc::Planets);
		UpdateAspects();
		return;
	}
	m_ChartDrawing.Refresh();
	m_AspectGrid.Refresh();		// (its colours are the wheel's)
}

void CChartView::TextFontChanged() {
	m_DetailsView.ApplyTextFont();
	m_AspectList.ApplyTextFont();
	m_MidpointList.ApplyTextFont();
	m_MidpointTree.ApplyTextFont();
	m_PartList.ApplyTextFont();
	m_AspectGrid.Refresh();		// (its text is in the same family)
	m_ChartDrawing.Refresh();
}

void CChartView::AspectSettingsChanged() {
	UpdateAspects();
}

void CChartView::DerivedChart(ChartData data, DerivedRecipe const* recipe) {
	m_ReadOnly = true;
	if (recipe)
		m_Recipe = *recipe;
	m_Data = std::move(data);
	SyncPartOfFortune();
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
	for (UINT id : { ID_CHART_DERIVED_SOLARRETURN, ID_CHART_DERIVED_LUNARRETURN, ID_CHART_DERIVED_SOLARARC, ID_CHART_DERIVED_COMPOSITE, ID_CHART_DERIVED_DAVISON })
		ui.UIEnable(id, active && !m_ReadOnly);
	for (UINT id : { ID_CHART_ANALYSIS, ID_CHART_OVERLAY, ID_CHART_TRANSITS, ID_CHART_OVERLAY_NONE, ID_CHART_OVERLAY_SYNASTRY })
		ui.UIEnable(id, active);
	ui.UIEnable(ID_FILE_SAVE, active && (!m_ReadOnly || m_Recipe));
	ui.UIEnable(ID_FILE_SAVE_AS, active && (!m_ReadOnly || m_Recipe));
	ui.UIEnable(ID_FILE_EXPORT, active);
	ui.UIEnable(ID_FILE_PRINT, active);
	ui.UIEnable(ID_FILE_PRINT_PREVIEW, active);
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

	// (what the wheel options added to the planets is not the chart's)
	ChartData toSave = m_Data;
	for (auto planet : m_ExtraAdded)
		toSave.RemovePlanets({ planet });
	std::wstring error;
	if (!(m_Recipe ? ChartFile::SaveDerived(*m_Recipe, path, error) : ChartFile::Save(toSave, path, error))) {
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
	if (m_ReadOnly && !m_Recipe)
		return 0;		// (a chart that can't be made again from what it was made of can't be saved)
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
	CToolBarCtrl tb(ToolbarHelper::CreateAndInitToolBar(m_hWndToolBar, buttons, _countof(buttons), 16));

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

bool CChartView::GetStatusInfo(StatusInfo& status) const {
	if (m_Data.AllPlanets().empty())
		return false;
	auto& info = m_Data.Info();
	status.Name = m_Title.IsEmpty() ? CString(L"Chart") : m_Title;

	// the moment as the chart's own zone shows it
	int offset = info.TimeZone.OffsetUT;
	auto local = TimeZones::UtToLocal(info.Time, info.TimeZone, &offset);
	status.Time.Format(L"%04ld/%02ld/%02ld  %02ld:%02ld:%02ld  (UTC%s)", local.Year, local.Month, local.Day, local.Hour, local.Minute, local.Second,
		(PCWSTR)TimeZones::FormatOffset(offset));

	CString place;
	for (auto const* part : { &info.City, &info.Country })
		if (!part->empty())
			place += (place.IsEmpty() ? L"" : L", ") + CString(part->c_str());
	status.Place.Format(L"%s%s%.2f%c%c  %.2f%c%c", (PCWSTR)place, place.IsEmpty() ? L"" : L"   ", std::fabs(info.Latitude), 0xb0, info.Latitude >= 0 ? L'N' : L'S',
		std::fabs(info.Longitude), 0xb0, info.Longitude >= 0 ? L'E' : L'W');

	status.Details = StringHelper::HouseSystemToString(m_Data.GetHouseSystem());
	auto add = [&](PCWSTR text) {
		status.Details += L"  |  ";
		status.Details += text;
	};
	if (m_Data.Harmonic() > 1) {
		CString harmonic;
		harmonic.Format(L"Harmonic %d", m_Data.Harmonic());
		add(harmonic);
	}
	if (m_ReadOnly)
		add(L"Read-only");
	if (m_Live)
		add(L"Live");
	else if (m_AutoStep)
		add(L"Auto");
	if (m_Overlay)
		add(m_Overlay->Caption.c_str());
	return true;
}

bool CChartView::ShowMoment(DateTime const& ut, MomentKind kind) {
	if (m_Data.AllPlanets().empty())
		return false;
	bool shown = kind == MomentKind::Transits ? ShowOverlay(OverlayKind::Transit) :
		ShowOverlay(OverlayKind::Progressed, kind == MomentKind::SolarArc ? ProgressionMethod::SolarArc : ProgressionMethod::Secondary);
	if (!shown || !m_Overlay)
		return false;
	// the overlay's moment is the one asked for, shown in the chart's zone as it would be
	m_Overlay->When = ut;
	int offset = m_Overlay->Zone.OffsetUT;
	TimeZones::UtToLocal(ut, m_Overlay->Zone, &offset);
	m_Overlay->Zone.OffsetUT = offset;
	UpdateOverlay();
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

void CChartView::NewSolarArcChart() {
	// the date to start from: the overlay's moment if one shows (so Step and Live can pick it), otherwise now, in this chart's zone
	DateTime target = OverlayHasTime() ? m_Overlay->When : DateTime::Now();
	TimeZoneInfo zone = OverlayHasTime() ? m_Overlay->Zone : m_Data.Info().TimeZone;
	int offset = zone.OffsetUT;
	TimeZones::UtToLocal(target, zone, &offset);
	zone.OffsetUT = offset;

	CDirectionsDlg dlg;
	dlg.Init(target, zone, ArcKey::Actual, L"Solar Arc Chart");
	if (dlg.DoModal(m_hWnd) != IDOK)
		return;

	DerivedRecipe recipe;
	recipe.Kind = DerivedKind::SolarArc;
	recipe.A = m_Data;
	recipe.Target = dlg.Target();
	recipe.Zone = dlg.Zone();
	recipe.Key = dlg.Key();
	auto chart = DerivedCharts::Build(m_Calc, recipe);

	int shown = dlg.Zone().OffsetUT;
	auto local = TimeZones::UtToLocal(dlg.Target(), dlg.Zone(), &shown);
	CString title;
	title.Format(L"Solar Arc %04ld/%02ld/%02ld: %s", local.Year, local.Month, local.Day, (PCWSTR)m_Title);
	Frame()->AddDerivedChartView(std::move(chart), title, &recipe);
}

void CChartView::NewPairChart(bool davison) {
	OpenChart other;
	if (!PickOtherChart(other, davison ? L"a Davison chart is made from two charts" : L"a composite is made from two charts"))
		return;
	DerivedRecipe recipe;
	recipe.Kind = davison ? DerivedKind::Davison : DerivedKind::Composite;
	recipe.A = m_Data;
	recipe.B = std::move(other.Data);
	ChartData chart = DerivedCharts::Build(m_Calc, recipe);
	CString title;
	title.Format(L"%s: %s + %s", davison ? L"Davison" : L"Composite", (PCWSTR)m_Title, (PCWSTR)other.Name);
	Frame()->AddDerivedChartView(std::move(chart), title, &recipe);
}

LRESULT CChartView::OnAnalysis(WORD, WORD, HWND, BOOL&) {
	if (!m_Data.AllPlanets().empty())
		Frame()->NewAnalysis(this);
	return 0;
}

LRESULT CChartView::OnDerived(WORD, WORD id, HWND, BOOL&) {
	if (m_ReadOnly || m_Data.AllPlanets().empty())
		return 0;
	switch (id) {
		case ID_CHART_DERIVED_SOLARRETURN: NewReturnChart(Planet::Sun); break;
		case ID_CHART_DERIVED_LUNARRETURN: NewReturnChart(Planet::Moon); break;
		case ID_CHART_DERIVED_SOLARARC: NewSolarArcChart(); break;
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
				if (p.Planet != Planet::PartOfFortune)		// (it needs the chart's Ascendant: the sky at another moment has no Part of Fortune of its own)
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
	ui.UIEnable(ID_CHART_OVERLAY_DATE, OverlayHasTime());		// (a synastry overlay has no moment)
}

void CChartView::UpdateOverlay() {
	if (!m_Overlay) {
		m_ChartDrawing.ClearOverlay();
		m_AspectList.ClearOverlayAspects();
		m_AspectList.Refresh();
		m_AspectGrid.SetOverlay(nullptr);
		m_MidpointList.SetOverlay(nullptr);
		m_MidpointTree.SetOverlay(nullptr);
		m_AspectGrid.Refresh();
		UpdateAspectGridScrollSize();
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

	m_AspectList.SetOverlayAspects(overlay.Aspects, overlay.Label, overlay.BaseLabel);
	m_AspectList.Refresh();
	m_AspectGrid.SetOverlay(&overlay);
	m_MidpointList.SetOverlay(&overlay);
	m_MidpointTree.SetOverlay(&overlay);
	m_AspectGrid.Refresh();
	UpdateAspectGridScrollSize();
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

LRESULT CChartView::OnOverlayDate(WORD, WORD, HWND, BOOL&) {
	if (!OverlayHasTime())
		return 0;
	// the moment is shown in the overlay's zone, as the chart's own time is
	auto zone = m_Overlay->Zone;
	int offset = zone.OffsetUT;
	TimeZones::UtToLocal(m_Overlay->When, zone, &offset);
	zone.OffsetUT = offset;

	CDirectionsDlg dlg;
	dlg.HideArc();
	dlg.Init(m_Overlay->When, zone, ArcKey::Actual, m_Overlay->Kind == OverlayKind::Transit ? L"Transit Date" : L"Progression Date");
	if (dlg.DoModal(m_hWnd) != IDOK)
		return 0;

	if (m_Live)
		SetLive(false);		// the user took over the time
	m_Overlay->When = dlg.Target();
	m_Overlay->Zone = dlg.Zone();
	UpdateOverlay();
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
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING | (OverlayHasTime() ? 0 : MF_GRAYED), ID_CHART_OVERLAY_DATE, L"Set Date and Time...");

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

	m_MidpointList.Create(m_DetailsTabs, rcDefault, nullptr, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_MidpointList.SetStatic();

	m_MidpointTree.Create(m_DetailsTabs, rcDefault, nullptr, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

	m_PartList.Create(m_DetailsTabs, rcDefault, nullptr, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_PartList.SetStatic();

	m_DetailsTabs.AddPage(m_DetailsView.m_hWnd, L"Details");
	m_DetailsTabs.AddPage(m_AspectGridScroll.m_hWnd, L"Aspect Grid");
	m_DetailsTabs.AddPage(m_AspectList.m_hWnd, L"Aspect List");
	m_DetailsTabs.AddPage(m_MidpointList.m_hWnd, L"Midpoints");
	m_DetailsTabs.AddPage(m_MidpointTree.m_hWnd, L"Midpoint Tree");
	m_DetailsTabs.AddPage(m_PartList.m_hWnd, L"Arabic Parts");
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

	// on a tab with a list, the selected rows as text
	if (auto tab = ActiveListTab(); tab && tab->List && Helpers::CopyListRows(m_hWnd, tab->List, tab->Table))
		return 0;

	// anywhere else (or with no row selected) it is the chart wheel, as a picture
	auto image = m_ChartDrawing.RenderImage(ChartImage::DefaultSize);
	if (!image || !ChartImage::CopyToClipboard(m_hWnd, image))
		AtlMessageBox(m_hWnd, L"The chart could not be copied to the clipboard.", L"Astro Studio", MB_ICONWARNING);
	return 0;
}

LRESULT CChartView::OnPrint(WORD, WORD id, HWND, BOOL&) {
	if (m_Data.AllPlanets().empty())
		return 0;
	std::unique_ptr<Printing::Document> document;
	{
		CWaitCursor wait;
		document = MakePrintDocument();
	}
	if (id == ID_FILE_PRINT_PREVIEW)
		Printing::Preview(m_hWnd, *document);
	else
		Printing::Print(m_hWnd, *document);
	return 0;
}

std::unique_ptr<Printing::Document> CChartView::MakePrintDocument() {
	Printing::ChartSheet sheet;
	sheet.Title = m_Title.IsEmpty() ? CString(L"Chart") : m_Title;

	// what the chart is: the moment (as the chart's own zone shows it), the place, the houses
	auto& info = m_Data.Info();
	int offset = info.TimeZone.OffsetUT;
	auto local = TimeZones::UtToLocal(info.Time, info.TimeZone, &offset);
	CString line;
	line.Format(L"%04ld/%02ld/%02ld  %02ld:%02ld:%02ld  (UTC%s)", local.Year, local.Month, local.Day, local.Hour, local.Minute, local.Second,
		(PCWSTR)TimeZones::FormatOffset(offset));
	sheet.Lines.push_back(line);
	CString place;
	for (auto const* part : { &info.City, &info.State, &info.Country })
		if (!part->empty())
			place += (place.IsEmpty() ? L"" : L", ") + CString(part->c_str());
	line.Format(L"%s%s%.4f%c%c  %.4f%c%c", (PCWSTR)place, place.IsEmpty() ? L"" : L"   ", std::fabs(info.Latitude), 0xb0, info.Latitude >= 0 ? L'N' : L'S',
		std::fabs(info.Longitude), 0xb0, info.Longitude >= 0 ? L'E' : L'W');
	sheet.Lines.push_back(line);
	line.Format(L"House system: %s", StringHelper::HouseSystemToString(m_Data.GetHouseSystem()));
	if (m_Data.Harmonic() > 1)
		line.AppendFormat(L"   Harmonic: %d", m_Data.Harmonic());
	sheet.Lines.push_back(line);
	if (m_Overlay)
		sheet.Lines.push_back(CString(m_Overlay->Caption.c_str()));

	// the wheel, on white whatever the program looks like
	if (auto image = m_ChartDrawing.RenderImage(2400, true))
		sheet.Wheel = ChartImage::ToDib(image);

	std::vector<std::vector<CString>> planets;
	for (auto const& planet : m_Data.AllPlanets()) {
		CString position = Helpers::FormatLongitude(planet.Longitude, FormatOptions::ShowSeconds | FormatOptions::ShowDegreeGlyph);
		// (the retrograde mark is part of the text)
		CString speed, house;
		speed.Format(L"%.4f", planet.Speed);
		if (int h = DerivedCharts::HouseOf(m_Data.Houses(), planet.Longitude); h > 0)
			house.Format(L"%d", h);
		planets.push_back({ CString(Helpers::GetPlanetName(planet.Planet)), position, speed, house });
	}
	sheet.Planets = Printing::TableFromRows({ L"Planet", L"Position", L"Speed", L"House" }, std::move(planets));

	std::vector<std::vector<CString>> houses;
	auto cusp = [](AstroPoint const& value) { return Helpers::FormatLongitude(value, FormatOptions::ShowDegreeGlyph); };
	for (int i = 0; i < 12; i++) {
		CString number;
		number.Format(L"%d", i + 1);
		houses.push_back({ number, cusp(m_Data.Houses().Cusps[i]) });
	}
	houses.push_back({ L"Asc", cusp(m_Data.Houses().Asc) });
	houses.push_back({ L"MC", cusp(m_Data.Houses().MC) });
	sheet.Houses = Printing::TableFromRows({ L"House", L"Cusp" }, std::move(houses));

	// on a tab with a list, the list follows on the pages after
	if (auto tab = ActiveListTab()) {
		sheet.Extra = tab->Table;
		sheet.ExtraTitle = CString(tab->Name) + L" - " + sheet.Title;
	}
	return Printing::MakeChartDocument(std::move(sheet));
}

std::optional<CChartView::ListTab> CChartView::ActiveListTab() {
	HWND page = m_DetailsTabs.GetPageHWND(m_DetailsTabs.GetActivePage());
	if (page == nullptr)
		return std::nullopt;
	if (page == m_AspectList.m_hWnd)
		return ListTab{ m_AspectList.ListWindow(), m_AspectList.Table(), L"Aspects" };
	if (page == m_MidpointList.m_hWnd)
		return ListTab{ m_MidpointList.ListWindow(), m_MidpointList.Table(), L"Midpoints" };
	if (page == m_MidpointTree.m_hWnd)
		return ListTab{ nullptr, m_MidpointTree.Table(), L"Midpoint Tree" };		// (a tree: it has no rows to select, so Copy stays with the wheel)
	if (page == m_PartList.m_hWnd)
		return ListTab{ m_PartList.ListWindow(), m_PartList.Table(), L"Arabic Parts" };
	return std::nullopt;
}

LRESULT CChartView::OnExport(WORD, WORD, HWND, BOOL&) {
	// the tab's list as a CSV file (offered first), or the wheel as a picture: the file type chosen says which
	auto tab = ActiveListTab();
	static constexpr wchar_t pictureFilter[] = L"PNG pictures (*.png)\0*.png\0";
	static constexpr wchar_t listFilter[] = L"CSV files (*.csv)\0*.csv\0PNG pictures (the chart wheel) (*.png)\0*.png\0";
	CString suggestion = tab ? CString(tab->Name) + L" " + SafeFileName(m_Title) : SafeFileName(m_Title);
	CSimpleFileDialog dlg(FALSE, tab ? L"csv" : L"png", suggestion, OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING,
		tab ? listFilter : pictureFilter, m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (!ok)
		return 0;
	if (tab && _wcsicmp(std::filesystem::path(dlg.m_szFileName).extension().c_str(), L".png") != 0) {
		Helpers::SaveTable(m_hWnd, tab->Table, dlg.m_szFileName, tab->Name);
		return 0;
	}

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
	m_AspectGridScroll.SetScrollSize(size.cx, size.cy);
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

