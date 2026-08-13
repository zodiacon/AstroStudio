#include "pch.h"
#include "ChartDetailsView.h"

#include "AstroFont.h"
#include "AstroHelpers.h"

#include <wx/spinctrl.h>
#include <wx/datectrl.h>
#include <wx/timectrl.h>
#include <wx/dateevt.h>
#include <wx/listctrl.h>
#include <wx/wrapsizer.h>

namespace {

	//
	// KNOWN GAP: the date and time pickers do not follow dark mode.
	//
	// They are SysDateTimePick32, and Windows has no dark theme for that
	// control. Measured, not assumed - both of these leave it stark white:
	//
	//   * SetWindowTheme(hwnd, L"DarkMode_CFD", nullptr) directly
	//   * overriding wxWindow::MSWGetDarkModeSupport() to report the same
	//     theme name, which is wx's own supported hook (wx/msw/window.h) and
	//     the only one that survives, since wx re-applies its choice after
	//     creation and on every appearance change
	//
	// The WTL build does not theme this control either - it custom-*paints* it,
	// via WTLHelper's DarkModeSubclass. Matching that means subclassing and
	// painting the control by hand.
	//
	// The other option is wxDatePickerCtrlGeneric (wx/generic/datectrl.h),
	// which is wx-drawn and would theme correctly - but there is no generic
	// time picker to pair it with, so the two fields would stop matching.
	//

	// Column tags, matching CChartDetailsView::ColumnType.
	enum PlanetColumn { PlanetGlyph, PlanetLongitude, PlanetLatitude, PlanetSpeed };
	enum HouseColumn { HouseNumber, HouseLongitude };

	constexpr int HouseRowCount = 16;	// 12 cusps + Asc, MC, Vertex, Polar Asc

	const HouseSystem AllHouseSystems[] = {
		HouseSystem::Placidus, HouseSystem::Koch, HouseSystem::Porphyrius,
		HouseSystem::Regiomontanus, HouseSystem::Campanus, HouseSystem::Equal,
		HouseSystem::Morinus, HouseSystem::Topocentric, HouseSystem::Alcabitus,
		HouseSystem::Horizontal, HouseSystem::Krusinski, HouseSystem::EqualWholeSign,
		HouseSystem::CarterPoliEqu, HouseSystem::EqualMC, HouseSystem::Sunshine,
		HouseSystem::SunshineAlt, HouseSystem::APCHouses,
	};

	// DateTime holds UT; the pickers show local time, as the WTL version did
	// with SystemTimeToTzSpecificLocalTime.
	wxDateTime ToLocal(DateTime const& dt) {
		wxDateTime t(static_cast<wxDateTime::wxDateTime_t>(dt.Day()),
			static_cast<wxDateTime::Month>(dt.Month() - 1),
			static_cast<int>(dt.Year()),
			static_cast<wxDateTime::wxDateTime_t>(dt.Hour()),
			static_cast<wxDateTime::wxDateTime_t>(dt.Minute()),
			static_cast<wxDateTime::wxDateTime_t>(dt.Second()));
		return t.IsValid() ? t.FromUTC() : wxDateTime::Now();
	}

} // namespace

//
// ---------------------------------------------------------------- DetailsList
//

ChartDetailsView::DetailsList::DetailsList(ChartDetailsView* owner, Kind kind, wxWindow* parent)
	: wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL),
	m_Owner(owner), m_Kind(kind) {
}

wxString ChartDetailsView::DetailsList::OnGetItemText(long item, long column) const {
	return m_Kind == Kind::Planets
		? m_Owner->PlanetCellText(item, column)
		: m_Owner->HouseCellText(item, column);
}

//
// Replaces OnPrePaint/OnItemPrePaint/OnSubItemPrePaint entirely: the element
// background is per row, the glyph font is per column, and wx asks for both.
//
wxItemAttr* ChartDetailsView::DetailsList::OnGetItemColumnAttr(long item, long column) const {
	if (!m_Owner->m_Data)
		return nullptr;

	m_Attr = wxItemAttr();

	if (m_Kind == Kind::Planets) {
		if (item < static_cast<long>(m_Owner->m_Planets.size()))
			m_Attr.SetBackgroundColour(
				AstroHelpers::ElementColour(m_Owner->m_Planets[item].Longitude.Sign()));

		if (column == PlanetGlyph || column == PlanetLongitude)
			m_Attr.SetFont(m_Owner->m_GlyphFont);
	}
	else {
		// Only the twelve cusps are coloured; Asc/MC/Vx/PAsc are not, matching
		// the `cd->dwItemSpec < 12` test in OnItemPrePaint.
		if (item < 12)
			m_Attr.SetBackgroundColour(
				AstroHelpers::ElementColour(m_Owner->m_Data->Houses().Cusps[item].Sign()));

		if (column == HouseLongitude)
			m_Attr.SetFont(m_Owner->m_GlyphFont);
	}

	return &m_Attr;
}

//
// ----------------------------------------------------------- ChartDetailsView
//

ChartDetailsView::ChartDetailsView(wxWindow* parent, RecalcHandler onRecalc)
	: wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxTAB_TRAVERSAL | wxCLIP_CHILDREN),
	m_OnRecalc(std::move(onRecalc)) {
	BuildControls();
}

void ChartDetailsView::BuildControls() {
	// The WTL version enlarged the list font by 20% and derived the glyph font
	// from it (ChartDetailsView.cpp:203-211).
	auto listFont = GetFont();
	listFont.SetPointSize(listFont.GetPointSize() * 12 / 10);
	m_GlyphFont = AstroHelpers::GlyphFont(listFont.GetPixelSize().y);

	auto root = new wxBoxSizer(wxVERTICAL);
	auto flags = wxSizerFlags().Centre().Border(wxRIGHT, FromDIP(4));

	//
	// Name / location
	//
	auto row1 = new wxBoxSizer(wxHORIZONTAL);
	row1->Add(new wxStaticText(this, wxID_ANY, "Name:"), flags);
	m_Name = new wxTextCtrl(this, wxID_ANY);
	row1->Add(m_Name, wxSizerFlags(1).Centre().Border(wxRIGHT, FromDIP(8)));
	row1->Add(new wxStaticText(this, wxID_ANY, "Location:"), flags);
	m_Location = new wxTextCtrl(this, wxID_ANY);
	row1->Add(m_Location, wxSizerFlags(1).Centre().Border(wxRIGHT, FromDIP(4)));
	m_Lookup = new wxButton(this, wxID_ANY, "Lookup");
	row1->Add(m_Lookup, wxSizerFlags().Centre());
	root->Add(row1, wxSizerFlags().Expand().Border(wxALL, FromDIP(6)));

	//
	// Everything below the name row goes into a wrap sizer as self-contained
	// groups.
	//
	// The .rc laid this dialog out at a fixed 440 dialog units and simply
	// assumed the width; here the form lives in a splitter pane that the user
	// can drag arbitrarily narrow, so a single row of controls gets its tail
	// clipped. Wrapping by group keeps every control reachable at any pane
	// width, which fixed-coordinate dialogs cannot do.
	//
	auto fields = new wxWrapSizer(wxHORIZONTAL);
	auto groupFlags = wxSizerFlags().Centre().Border(wxRIGHT | wxBOTTOM, FromDIP(8));

	//
	// Latitude / longitude. Each edit+updown pair from the .rc is one spin ctrl.
	//
	auto row2 = new wxBoxSizer(wxHORIZONTAL);
	row2->Add(new wxStaticText(this, wxID_ANY, "Latitude:"), flags);
	m_LatDeg = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		FromDIP(wxSize(60, -1)), wxSP_ARROW_KEYS, 0, 89);
	row2->Add(m_LatDeg, flags);
	m_North = new wxRadioButton(this, wxID_ANY, "N", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	m_South = new wxRadioButton(this, wxID_ANY, "S");
	row2->Add(m_North, flags);
	row2->Add(m_South, flags);
	m_LatMin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		FromDIP(wxSize(60, -1)), wxSP_ARROW_KEYS, 0, 59);
	row2->Add(m_LatMin, wxSizerFlags().Centre());
	fields->Add(row2, groupFlags);

	auto lonGroup = new wxBoxSizer(wxHORIZONTAL);
	lonGroup->Add(new wxStaticText(this, wxID_ANY, "Longitude:"), flags);
	m_LonDeg = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		FromDIP(wxSize(60, -1)), wxSP_ARROW_KEYS, 0, 179);
	lonGroup->Add(m_LonDeg, flags);
	m_East = new wxRadioButton(this, wxID_ANY, "E", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	m_West = new wxRadioButton(this, wxID_ANY, "W");
	lonGroup->Add(m_East, flags);
	lonGroup->Add(m_West, flags);
	m_LonMin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		FromDIP(wxSize(60, -1)), wxSP_ARROW_KEYS, 0, 59);
	lonGroup->Add(m_LonMin, flags);
	m_Here = new wxButton(this, wxID_ANY, "Here");
	lonGroup->Add(m_Here, wxSizerFlags().Centre());
	fields->Add(lonGroup, groupFlags);

	//
	// House system / date / time / harmonic
	//
	auto houseGroup = new wxBoxSizer(wxHORIZONTAL);
	houseGroup->Add(new wxStaticText(this, wxID_ANY, "House System:"), flags);
	m_HouseSystem = new wxChoice(this, wxID_ANY);
	for (auto system : AllHouseSystems)
		m_HouseSystem->Append(AstroHelpers::HouseSystemToString(system),
			reinterpret_cast<void*>(static_cast<intptr_t>(system)));

	//
	// Seed the selection here, while the panel still has no sizer, rather than
	// leaving the first UpdateControls to do it: wxChoice::SetSelection
	// invalidates the control's best size, and once this control is inside the
	// wxWrapSizer that costs a full relayout. ChartData defaults to Koch; if it
	// ever does not, the first UpdateControls corrects it.
	//
	m_ShownHouseSystem = HouseSystem::Koch;
	m_HouseSystem->SetSelection(
		m_HouseSystem->FindString(AstroHelpers::HouseSystemToString(m_ShownHouseSystem)));

	// Pin the size so later selection changes cannot invalidate it either.
	m_HouseSystem->SetInitialSize(m_HouseSystem->GetBestSize());

	houseGroup->Add(m_HouseSystem, wxSizerFlags().Centre());
	fields->Add(houseGroup, groupFlags);

	auto whenGroup = new wxBoxSizer(wxHORIZONTAL);
	whenGroup->Add(new wxStaticText(this, wxID_ANY, "Date:"), flags);
	// See the KNOWN GAP note above about these two and dark mode.
	m_Date = new wxDatePickerCtrl(this, wxID_ANY, wxDefaultDateTime, wxDefaultPosition,
		wxDefaultSize, wxDP_DROPDOWN | wxDP_SHOWCENTURY);
	whenGroup->Add(m_Date, flags);
	whenGroup->Add(new wxStaticText(this, wxID_ANY, "Time:"), flags);
	m_Time = new wxTimePickerCtrl(this, wxID_ANY);
	whenGroup->Add(m_Time, flags);
	m_Now = new wxButton(this, wxID_ANY, "Now");
	whenGroup->Add(m_Now, wxSizerFlags().Centre());
	fields->Add(whenGroup, groupFlags);

	auto harmonicGroup = new wxBoxSizer(wxHORIZONTAL);
	harmonicGroup->Add(new wxStaticText(this, wxID_ANY, "Harmonic:"), flags);
	m_Harmonic = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		FromDIP(wxSize(70, -1)), wxSP_ARROW_KEYS, 1, 9999, 1);
	harmonicGroup->Add(m_Harmonic, wxSizerFlags().Centre());
	fields->Add(harmonicGroup, groupFlags);

	root->Add(fields, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, FromDIP(6)));

	//
	// The two tables
	//
	m_PlanetList = new DetailsList(this, DetailsList::Kind::Planets, this);
	m_PlanetList->SetFont(listFont);
	m_PlanetList->AppendColumn("P", wxLIST_FORMAT_LEFT, FromDIP(34));
	m_PlanetList->AppendColumn("Longitude", wxLIST_FORMAT_LEFT, FromDIP(125));
	m_PlanetList->AppendColumn("Latitude", wxLIST_FORMAT_LEFT, FromDIP(80));
	m_PlanetList->AppendColumn("Speed", wxLIST_FORMAT_RIGHT, FromDIP(80));

	m_HouseList = new DetailsList(this, DetailsList::Kind::Houses, this);
	m_HouseList->SetFont(listFont);
	m_HouseList->AppendColumn("H", wxLIST_FORMAT_RIGHT, FromDIP(44));
	m_HouseList->AppendColumn("Longitude", wxLIST_FORMAT_LEFT, FromDIP(100));
	m_HouseList->SetItemCount(HouseRowCount);

	auto tables = new wxBoxSizer(wxHORIZONTAL);
	tables->Add(m_PlanetList, wxSizerFlags(3).Expand().Border(wxRIGHT, FromDIP(6)));
	tables->Add(m_HouseList, wxSizerFlags(2).Expand());
	root->Add(tables, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT, FromDIP(6)));

	m_Apply = new wxButton(this, wxID_ANY, "Apply");
	root->Add(m_Apply, wxSizerFlags().Border(wxALL, FromDIP(6)));

	SetSizer(root);

	//
	// Network-backed; both arrive in phase 6 with NetworkHelper.
	//
	m_Here->Disable();
	m_Lookup->Disable();

	m_HouseSystem->Bind(wxEVT_CHOICE, &ChartDetailsView::OnHouseSystemChanged, this);
	m_Date->Bind(wxEVT_DATE_CHANGED, &ChartDetailsView::OnDateTimeChanged, this);
	m_Time->Bind(wxEVT_TIME_CHANGED, &ChartDetailsView::OnDateTimeChanged, this);
	m_Harmonic->Bind(wxEVT_SPINCTRL, &ChartDetailsView::OnHarmonicChanged, this);
	m_Now->Bind(wxEVT_BUTTON, &ChartDetailsView::OnNow, this);
	m_Apply->Bind(wxEVT_BUTTON, &ChartDetailsView::OnApply, this);

	// Declared as wxWindow*[] so each element converts on the way in; a braced
	// list of mixed pointer types has no common type to deduce.
	wxWindow* locationSpins[] = { m_LatDeg, m_LatMin, m_LonDeg, m_LonMin };
	for (auto w : locationSpins)
		w->Bind(wxEVT_SPINCTRL, &ChartDetailsView::OnLocationChanged, this);

	wxWindow* hemisphereButtons[] = { m_North, m_South, m_East, m_West };
	for (auto w : hemisphereButtons)
		w->Bind(wxEVT_RADIOBUTTON, &ChartDetailsView::OnLocationChanged, this);
}

void ChartDetailsView::SetChartData(ChartData* data) {
	m_Data = data;
	if (!m_Data)
		return;

	auto const& info = m_Data->Info();
	m_Name->ChangeValue(wxString(
		(info.LastName.empty() ? L"" : info.LastName + L", ") + info.FirstName));
	UpdateControls();
}

void ChartDetailsView::UpdateControls(Recalc type) {
	if (!m_Data)
		return;

	m_Updating = true;

	if (type == Recalc::All || type == Recalc::Houses) {
		//
		// Only touch the combo when the value actually changed.
		//
		// wxChoice::SetStringSelection costs ~130 ms here, and UpdateControls
		// runs on every recalculation - so re-asserting the same selection was
		// essentially the entire cost of changing a date. The shown value is
		// tracked in a member so the common path makes no wx call at all.
		//
		if (m_ShownHouseSystem != m_Data->GetHouseSystem()) {
			m_ShownHouseSystem = m_Data->GetHouseSystem();
			auto index = m_HouseSystem->FindString(
				AstroHelpers::HouseSystemToString(m_ShownHouseSystem));
			if (index != wxNOT_FOUND)
				m_HouseSystem->SetSelection(index);
		}
		m_HouseList->SetItemCount(HouseRowCount);
		m_HouseList->Refresh();
	}

	auto local = ToLocal(m_Data->Info().Time);
	m_Date->SetValue(local);
	m_Time->SetValue(local);
	m_Harmonic->SetValue(m_Data->Harmonic());
	UpdateLocationControls();

	if (type == Recalc::All || type == Recalc::Planets) {
		m_Planets = m_Data->AllPlanets();
		m_PlanetList->SetItemCount(static_cast<long>(m_Planets.size()));
		m_PlanetList->Refresh();
	}

	m_Updating = false;
}

void ChartDetailsView::UpdateLocationControls() {
	auto const& info = m_Data->Info();

	{
		auto [deg, min, _] = AstroHelpers::GetDegMinSec(info.Latitude);
		m_LatDeg->SetValue(deg);
		m_LatMin->SetValue(min);
		(info.Latitude >= 0 ? m_North : m_South)->SetValue(true);
	}
	{
		auto [deg, min, _] = AstroHelpers::GetDegMinSec(info.Longitude);
		m_LonDeg->SetValue(deg);
		m_LonMin->SetValue(min);
		(info.Longitude >= 0 ? m_East : m_West)->SetValue(true);
	}

	m_Location->ChangeValue(wxString(
		info.City + (info.State.empty() ? L"" : (L", " + info.State)) + L", " + info.Country));
}

void ChartDetailsView::ApplyLocationFromControls() {
	auto& info = m_Data->Info();
	info.Latitude = (m_LatDeg->GetValue() + m_LatMin->GetValue() / 60.0)
		* (m_South->GetValue() ? -1 : 1);
	info.Longitude = (m_LonDeg->GetValue() + m_LonMin->GetValue() / 60.0)
		* (m_West->GetValue() ? -1 : 1);
}

void ChartDetailsView::Recalculate(Recalc what) {
	if (m_OnRecalc)
		m_OnRecalc(what);
}

void ChartDetailsView::OnHouseSystemChanged(wxCommandEvent&) {
	if (!m_Data || m_Updating)
		return;

	auto sel = m_HouseSystem->GetSelection();
	if (sel == wxNOT_FOUND)
		return;

	m_Data->SetHouseSystem(static_cast<HouseSystem>(
		reinterpret_cast<intptr_t>(m_HouseSystem->GetClientData(sel))));
	Recalculate(Recalc::Houses);
}

void ChartDetailsView::OnDateTimeChanged(wxDateEvent&) {
	if (!m_Data || m_Updating)
		return;

	// Both pickers are read together: each holds only half of the instant.
	auto date = m_Date->GetValue();
	auto time = m_Time->GetValue();
	wxDateTime local(date.GetDay(), date.GetMonth(), date.GetYear(),
		time.GetHour(), time.GetMinute(), time.GetSecond());
	auto utc = local.ToUTC();

	auto& info = m_Data->Info();
	info.Time.SetDate(utc.GetYear(), utc.GetMonth() + 1, utc.GetDay());
	info.Time.SetTime(utc.GetHour(), utc.GetMinute(), utc.GetSecond());

	Recalculate(Recalc::All);
}

void ChartDetailsView::OnHarmonicChanged(wxSpinEvent&) {
	if (!m_Data || m_Updating)
		return;

	auto harmonic = m_Harmonic->GetValue();
	if (harmonic != m_Data->Harmonic()) {
		m_Data->Harmonic(harmonic);
		Recalculate(Recalc::Planets);
	}
}

void ChartDetailsView::OnNow(wxCommandEvent&) {
	if (!m_Data)
		return;

	m_Data->Info().Time = DateTime::Now();
	Recalculate(Recalc::All);
}

void ChartDetailsView::OnLocationChanged(wxCommandEvent&) {
	if (!m_Data || m_Updating)
		return;

	ApplyLocationFromControls();
	Recalculate(Recalc::Houses);
}

void ChartDetailsView::OnApply(wxCommandEvent&) {
	if (!m_Data)
		return;

	ApplyLocationFromControls();

	auto date = m_Date->GetValue();
	auto time = m_Time->GetValue();
	wxDateTime local(date.GetDay(), date.GetMonth(), date.GetYear(),
		time.GetHour(), time.GetMinute(), time.GetSecond());
	auto utc = local.ToUTC();

	auto& info = m_Data->Info();
	info.Time.SetDate(utc.GetYear(), utc.GetMonth() + 1, utc.GetDay());
	info.Time.SetTime(utc.GetHour(), utc.GetMinute(), utc.GetSecond());

	if (auto sel = m_HouseSystem->GetSelection(); sel != wxNOT_FOUND)
		m_Data->SetHouseSystem(static_cast<HouseSystem>(
			reinterpret_cast<intptr_t>(m_HouseSystem->GetClientData(sel))));

	m_Data->Harmonic(m_Harmonic->GetValue());

	Recalculate(Recalc::All);
}

wxString ChartDetailsView::PlanetCellText(long row, long col) const {
	if (row >= static_cast<long>(m_Planets.size()))
		return wxString();

	auto const& pos = m_Planets[row];
	switch (col) {
		case PlanetGlyph:
			return DefaultFont::Get().GetPlanetGlyphAsString(pos.Planet);
		case PlanetLongitude:
			return AstroHelpers::FormatLongitude(pos.Longitude, FormatOptions::ShowSeconds
				| FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph);
		case PlanetLatitude:
			return AstroHelpers::FormatLatitude(pos.Latitude);
		case PlanetSpeed:
			return wxString::Format("%6.4f", pos.Speed);
	}
	return wxString();
}

wxString ChartDetailsView::HouseCellText(long row, long col) const {
	if (!m_Data)
		return wxString();

	auto const& houses = m_Data->Houses();
	constexpr auto options = FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph;

	if (col == HouseNumber) {
		switch (row) {
			case 12: return "Asc";
			case 13: return "MC";
			case 14: return "Vx";
			case 15: return "PAsc";
			default: return wxString::Format("%d", static_cast<int>(row) + 1);
		}
	}

	switch (row) {
		case 12: return AstroHelpers::FormatLongitude(houses.Asc, options);
		case 13: return AstroHelpers::FormatLongitude(houses.MC, options);
		case 14: return AstroHelpers::FormatLongitude(houses.Vertex, options);
		case 15: return AstroHelpers::FormatLongitude(houses.PolarAsc, options);
		default: return AstroHelpers::FormatLongitude(houses.Cusps[row], options);
	}
}
