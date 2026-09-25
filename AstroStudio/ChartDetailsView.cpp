#include "pch.h"
#include "ChartDetailsView.h"
#include <AstroCalculator.h>
#include <ChartData.h>
#include "StringHelper.h"
#include "DefaultFont.h"
#include "Helpers.h"
#include "SortHelper.h"
#include <WTLHelper.h>
#include "NetworkHelper.h"
#include "TimeZones.h"

#include "ColorHelper.h"

BOOL CChartDetailsView::PreTranslateMessage(MSG* pMsg) {
	return IsDialogMessage(pMsg);
}

void CChartDetailsView::SetChartData(ChartData* data) {
	m_Data = data;
	if (m_Data) {
		m_ctlHouses.SetItemCount(16);
		auto& info = m_Data->Info();
		UpdateLocationControls();
		SetDlgItemInt(IDC_HARMONIC, m_Data->Harmonic());
		SetDlgItemText(IDC_NAME, ((info.LastName.empty() ? L"" : info.LastName + L", ") + info.FirstName).c_str());
		UpdateControls();
	}
}

void CChartDetailsView::UpdateLocationControls() {
	ATLASSERT(m_Data);
	m_Location.Set(m_Data->Info(), m_LocationPending);
}

void CChartDetailsView::SetLocationPending(bool pending) {
	m_LocationPending = pending;
	if (m_Data)
		UpdateLocationControls();
}

bool CChartDetailsView::IsLocationEdited() const {
	return m_LocationEdited;
}

void CChartDetailsView::SetNotifyWindow(HWND hWnd) {
	m_NotifyWnd = hWnd;
}

CString CChartDetailsView::GetColumnText(HWND h, int row, int col) const {
	PlanetPosition const* pos;
	HouseData const* houses;
	if (h == m_ctlPlanets)
		pos = &m_Planets[row];
	else
		houses = &m_Data->Houses();

	switch (GetColumnManager(h)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Planet: return DefaultFont::Get().GetPlanetGlyphAsString(pos->Planet);
		case ColumnType::Longitude: return Helpers::FormatLongitude(pos->Longitude, FormatOptions::ShowSeconds | FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph);
		case ColumnType::Latitude: return Helpers::FormatLatitude(pos->Latitude);
		case ColumnType::Speed: return std::format(L"{:6.4f}", pos->Speed).c_str();
		case ColumnType::House:
			if(row < 12)
				return std::to_wstring(row + 1).c_str();
			else {
				switch (row) {
					case 12: return L"Asc";
					case 13: return L"MC";
					case 14: return L"Vx";
					case 15: return L"PAsc";
				}
			}
			break;
		case ColumnType::HouseLongitude: 
			if(row < 12)
				return Helpers::FormatLongitude(houses->Cusps[row], FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph);
			switch (row) {
				case 12: return Helpers::FormatLongitude(houses->Asc, FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph);
				case 13: return Helpers::FormatLongitude(houses->MC, FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph);
				case 14: return Helpers::FormatLongitude(houses->Vertex, FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph);
				case 15: return Helpers::FormatLongitude(houses->PolarAsc, FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph);
			}
			break;
	}
	return CString();
}

void CChartDetailsView::DoSort(SortInfo const* si) {
	auto compare = [&](auto& p1, auto& p2) {
		switch (GetColumnManager(m_ctlPlanets)->GetColumnTag<ColumnType>(si->SortColumn)) {
			case ColumnType::Planet: return SortHelper::Sort(p1.Planet, p2.Planet, si->SortAscending);
			case ColumnType::Longitude: return SortHelper::Sort(p1.Longitude, p2.Longitude, si->SortAscending);
			case ColumnType::Latitude: return SortHelper::Sort(p1.Latitude, p2.Latitude, si->SortAscending);
		}
		return false;
	};
	std::ranges::sort(m_Planets, compare);
}

DWORD CChartDetailsView::OnPrePaint(int, LPNMCUSTOMDRAW cd) noexcept {
	if (cd->hdr.hwndFrom == m_ctlPlanets || cd->hdr.hwndFrom == m_ctlHouses)
		return CDRF_NOTIFYITEMDRAW;
	SetMsgHandled(FALSE);
	return 0;
}

DWORD CChartDetailsView::OnItemPrePaint(int, LPNMCUSTOMDRAW cd) noexcept {
	auto h = cd->hdr.hwndFrom;
	if (h == m_ctlPlanets || h == m_ctlHouses) {
		auto lv = (LPNMLVCUSTOMDRAW)cd;
		COLORREF colors[] = {
			//
			// TODO: move to handle default colors
			//
			RGB(255, 69, 0),		// OrangeRed
			RGB(250, 250, 210),		// LightGoldenrodYellow
			RGB(144, 238, 144),		// LightGreen
			RGB(173, 216, 230)		// LightBlue
		};
		if (WTLHelper::IsDarkMode()) {
			for (auto& color : colors)
				color = ColorHelper::Darken(color, 40);
		}
		if (h == m_ctlPlanets) {
			lv->clrTextBk = colors[int(m_Data->GetPlanet((int)cd->dwItemSpec).Longitude.Sign()) % 4];
		}
		else if(cd->dwItemSpec < 12) {
			lv->clrTextBk = colors[int(m_Data->Houses().Cusps[((int)cd->dwItemSpec)].Sign()) % 4];
		}
		return CDRF_NOTIFYSUBITEMDRAW;
	}
	SetMsgHandled(FALSE);
	return 0;
}

DWORD CChartDetailsView::OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) const noexcept {
	auto hWnd = cd->hdr.hwndFrom;
	ATLASSERT(hWnd == m_ctlPlanets || hWnd == m_ctlHouses);
	auto lv = (LPNMLVCUSTOMDRAW)cd;
	auto cm = GetColumnManager(hWnd);
	ATLASSERT(cm);
	auto col = cm->GetColumnTag<ColumnType>(lv->iSubItem);
	auto astroFont = col == ColumnType::Planet || col == ColumnType::Longitude || col == ColumnType::HouseLongitude;
	::SelectObject(cd->hdc, astroFont ? m_Font.m_hFont : m_ctlPlanets.GetFont());
	return CDRF_NEWFONT;
}

void CChartDetailsView::UpdateControls(Recalc type) {
	ATLASSERT(m_Data);
	if (type == Recalc::All || type == Recalc::Houses) {
		m_ctlHouseSystem.SelectString(-1, StringHelper::HouseSystemToString(m_Data->GetHouseSystem()));
		m_ctlHouses.RedrawItems(0, m_ctlHouses.GetItemCount() - 1);
		m_ctlHouses.UpdateWindow();
	}
	m_Time.Set(m_Data->Info().Time, m_Data->Info().TimeZone);
	if (type == Recalc::All || type == Recalc::Planets) {
		m_Planets = m_Data->AllPlanets();
		Sort(GetSortInfo(m_ctlPlanets));
		m_ctlPlanets.SetItemCountEx((int)m_Planets.size(), LVSICF_NOSCROLL);
	}
	DisableEditors();		// (setting the time enables the zone controls again)
}

void CChartDetailsView::SetReadOnly() {
	m_ReadOnly = true;
	DisableEditors();
}

void CChartDetailsView::DisableEditors() {
	if (!m_ReadOnly)
		return;
	for (HWND child = GetWindow(GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT))
		if (child != m_ctlPlanets && child != m_ctlHouses)
			::EnableWindow(child, FALSE);
}

// The chart page forwards keyboard messages here (see CChartView::OnForwardMsg) so the dialog gets its
// keys: Tab moves between the fields and Enter presses the default button, Apply.
LRESULT CChartDetailsView::OnForwardMsg(UINT, WPARAM, LPARAM lParam, BOOL&) {
	return PreTranslateMessage(reinterpret_cast<LPMSG>(lParam));
}

LRESULT CChartDetailsView::OnInitView(UINT, WPARAM, LPARAM, BOOL&) {
	m_ctlHouseSystem.Attach(GetDlgItem(IDC_HOUSESYSTEM));
	m_ctlPlanets.Attach(GetDlgItem(IDC_PLANETS));
	m_ctlPlanets.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
	m_ctlPlanets.GetHeader().ModifyStyle(0, HDS_NOSIZING);
	m_ctlHouses.Attach(GetDlgItem(IDC_HOUSES));
	m_ctlHouses.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
	m_ctlHouses.GetHeader().ModifyStyle(0, HDS_NOSIZING);
	m_ctlHarmonicSpin.Attach(GetDlgItem(IDC_HARMONICUD));
	m_ctlHarmonicSpin.SetRange(1, 9999);
	AddIconToButton(IDC_NOW, IDI_CLOCK);
	AddIconToButton(IDC_HERE, IDI_PIN);
	AddIconToButton(IDC_LOOKUP, IDI_GLOBE);
	m_Location.Init(m_hWnd);
	m_Time.Init(m_hWnd);

	LOGFONT lf;
	CFontHandle(m_ctlPlanets.GetFont()).GetLogFont(lf);
	lf.lfHeight = lf.lfHeight * 120 / 100;
	CFontHandle local;
	local.CreateFontIndirect(&lf);
	m_ctlPlanets.SetFont(local);
	m_ctlHouses.SetFont(local);
	wcscpy_s(lf.lfFaceName, L"HamburgSymbols");
	m_Font.CreateFontIndirect(&lf);

	Helpers::FillHouseSystems(m_ctlHouseSystem);

	auto cm = GetColumnManager(m_ctlPlanets);
	cm->AddColumn(L"P", 0, 30, ColumnType::Planet);
	cm->AddColumn(L"Longitude", LVCFMT_FIXED_WIDTH, 115, ColumnType::Longitude);
	cm->AddColumn(L"Latitude", LVCFMT_FIXED_WIDTH, 70, ColumnType::Latitude);
	cm->AddColumn(L"Speed", LVCFMT_FIXED_WIDTH | LVCFMT_RIGHT, 75, ColumnType::Speed);
	cm->UpdateColumns();

	cm = GetColumnManager(m_ctlHouses);
	cm->AddColumn(L"", 0, 10);
	cm->AddColumn(L"H", LVCFMT_RIGHT | LVCFMT_FIXED_WIDTH, 40, ColumnType::House);
	cm->AddColumn(L"Longitude", LVCFMT_FIXED_WIDTH, 90, ColumnType::HouseLongitude);
	cm->UpdateColumns();
	cm->DeleteColumn(0);

	return 0;
}

LRESULT CChartDetailsView::OnHouseSystemChanged(WORD, WORD, HWND, BOOL&) {
	m_Data->SetHouseSystem((HouseSystem)m_ctlHouseSystem.GetItemData(m_ctlHouseSystem.GetCurSel()));
	if (m_NotifyWnd) {
		m_NotifyWnd.SendMessage(WM_RECALC, static_cast<WPARAM>(Recalc::Houses));
		m_ctlHouses.RedrawItems(0, m_ctlHouses.GetItemCount() - 1);
		m_ctlHouses.UpdateWindow();
	}
	return 0;
}

void CChartDetailsView::ApplyTimeFromControls() {
	if (m_Data == nullptr)
		return;

	auto& info = m_Data->Info();
	DateTime ut;
	TimeZoneInfo tz;
	if (m_Time.Get(ut, tz) != CTimeControls::Error::None) {
		// half-typed or impossible: put back what the chart has rather than calculate from it
		::MessageBeep(MB_ICONWARNING);
		UpdateControls();
		return;
	}

	bool changed = ut.Julian() != info.Time.Julian() || tz.Name != info.TimeZone.Name || tz.OffsetUT != info.TimeZone.OffsetUT;
	info.Time = ut;
	info.TimeZone = tz;
	if (changed && m_NotifyWnd)
		m_NotifyWnd.SendMessageW(WM_RECALC);
	UpdateControls();
}

LRESULT CChartDetailsView::OnTimeChanged(WORD, WORD, HWND, BOOL&) {
	ApplyTimeFromControls();
	return 0;
}

LRESULT CChartDetailsView::OnMonthOrYearChanged(WORD, WORD, HWND, BOOL&) {
	m_Time.UpdateDays();
	ApplyTimeFromControls();
	return 0;
}

LRESULT CChartDetailsView::OnManualToggled(WORD, WORD, HWND, BOOL&) {
	m_Time.ManualToggled();
	ApplyTimeFromControls();
	return 0;
}

LRESULT CChartDetailsView::OnHarmonicChanged(WORD, WORD, HWND, BOOL&) {
	if (m_Data == nullptr)
		return 0;

	int h = GetDlgItemInt(IDC_HARMONIC);
	if (h != m_Data->Harmonic() && h > 0 && h < 10000) {
		m_Data->Harmonic(h);
		if (m_NotifyWnd) {
			m_NotifyWnd.SendMessageW(WM_RECALC, static_cast<WPARAM>(Recalc::Planets));
			UpdateControls(Recalc::Planets);
		}
	}
	return 0;
}

LRESULT CChartDetailsView::OnNow(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_Data);
	auto& info = m_Data->Info();
	info.Time = DateTime::Now();
	if (m_NotifyWnd) {
		m_NotifyWnd.SendMessageW(WM_RECALC);
		UpdateControls();
	}

	return 0;
}

LRESULT CChartDetailsView::OnHere(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_Data);
	m_Location.BeginHere(m_Data->Info());
	return 0;
}

LRESULT CChartDetailsView::OnHereResult(UINT, WPARAM wParam, LPARAM lParam, BOOL&) {
	ChartInfo found;
	if (!m_Location.EndHere(wParam, lParam, found) || !m_Data)
		return 0;

	auto& info = m_Data->Info();
	info.Latitude = found.Latitude;
	info.Longitude = found.Longitude;
	info.Elevation = found.Elevation;
	info.City = found.City;
	info.State = found.State;
	info.Country = found.Country;

	m_LocationEdited = true;
	m_LocationPending = false;
	UpdateLocationControls();
	if (m_NotifyWnd) {
		m_NotifyWnd.SendMessageW(WM_RECALC, static_cast<WPARAM>(Recalc::Houses));
		m_ctlHouses.RedrawItems(0, m_ctlHouses.GetItemCount() - 1);
		m_ctlHouses.UpdateWindow();
	}

	return 0;
}

LRESULT CChartDetailsView::OnLookup(WORD, WORD, HWND, BOOL&) {
	CString text;
	GetDlgItemText(IDC_LOCATION, text);
	m_Location.BeginLookup(text);
	return 0;
}

LRESULT CChartDetailsView::OnLookupResult(UINT, WPARAM wParam, LPARAM lParam, BOOL&) {
	PlaceResult place;
	if (!m_Location.EndLookup(wParam, lParam, place) || !m_Data)
		return 0;

	// The location only: the chart's time zone stays as it is, since changing it would change the chart's UT.
	CLocationControls::ApplyPlace(place, m_Data->Info());
	m_LocationEdited = true;
	m_LocationPending = false;
	UpdateLocationControls();
	if (m_NotifyWnd) {
		m_NotifyWnd.SendMessageW(WM_RECALC, static_cast<WPARAM>(Recalc::Houses));
		m_ctlHouses.RedrawItems(0, m_ctlHouses.GetItemCount() - 1);
		m_ctlHouses.UpdateWindow();
	}
	return 0;
}

LRESULT CChartDetailsView::OnLocationChanged(WORD, WORD, HWND, BOOL&) {
	if (m_Data == nullptr || m_Location.IsUpdating())
		return 0;

	// The user has taken control of the location; a late geolocation result
	// must not overwrite it.
	m_LocationEdited = true;
	m_LocationPending = false;

	m_Location.GetCoordinates(m_Data->Info());
	if (m_NotifyWnd) {
		m_NotifyWnd.SendMessageW(WM_RECALC, static_cast<WPARAM>(Recalc::Houses));
		m_ctlHouses.RedrawItems(0, m_ctlHouses.GetItemCount() - 1);
		m_ctlHouses.UpdateWindow();
	}

	return 0;
}

LRESULT CChartDetailsView::OnApply(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_Data);
	auto& info = m_Data->Info();

	DateTime ut;
	TimeZoneInfo tz;
	auto error = m_Time.Get(ut, tz);
	if (error != CTimeControls::Error::None) {
		AtlMessageBox(m_hWnd, CTimeControls::Message(error), L"Astro Studio", MB_ICONWARNING);
		GetDlgItem(CTimeControls::ControlFor(error)).SetFocus();
		return 0;
	}
	info.Time = ut;
	info.TimeZone = tz;

	m_Location.GetCoordinates(info);

	m_Data->SetHouseSystem((HouseSystem)m_ctlHouseSystem.GetItemData(m_ctlHouseSystem.GetCurSel()));

	auto harmonic = GetDlgItemInt(IDC_HARMONIC);
	if (harmonic > 0 && harmonic < 10000)
		m_Data->Harmonic(harmonic);

	if (m_NotifyWnd) {
		m_NotifyWnd.SendMessageW(WM_RECALC, static_cast<WPARAM>(Recalc::All));
		UpdateControls();
	}

	return 0;
}
