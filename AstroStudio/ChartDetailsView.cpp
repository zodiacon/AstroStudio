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
	auto& info = m_Data->Info();

	m_UpdatingLocationControls = true;
	{
		auto [deg, min, _] = Helpers::GetDegMinSec(info.Latitude);
		SetDlgItemInt(IDC_LATDEG, deg);
		SetDlgItemInt(IDC_LATMIN, min);
		//
		// CheckRadioButton, not CheckDlgButton: the latter only sets the state
		// of the button named, leaving the other one checked too. Auto-radio
		// buttons clear their siblings when the *user* clicks, not on a
		// programmatic BM_SETCHECK - so switching to a southern or western
		// location used to light up both halves of the pair.
		//
		CheckRadioButton(IDC_NORTH, IDC_SOUTH, info.Latitude >= 0 ? IDC_NORTH : IDC_SOUTH);
	}
	{
		auto [deg, min, _] = Helpers::GetDegMinSec(info.Longitude);
		SetDlgItemInt(IDC_LONDEG, deg);
		SetDlgItemInt(IDC_LONMIN, min);
		CheckRadioButton(IDC_EAST, IDC_WEST, info.Longitude >= 0 ? IDC_EAST : IDC_WEST);
	}
	m_UpdatingLocationControls = false;

	if (m_LocationPending) {
		SetDlgItemText(IDC_LOCATION, L"Locating...");
	}
	else {
		// Join only the parts we actually have. Concatenating unconditionally
		// rendered an unknown location as a bare ", ", which is now visible
		// whenever the lookup fails.
		std::wstring const* parts[] = { &info.City, &info.State, &info.Country };
		CString location;
		for (auto part : parts) {
			if (part->empty())
				continue;
			if (!location.IsEmpty())
				location += L", ";
			location += part->c_str();
		}
		SetDlgItemText(IDC_LOCATION, location);
	}
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
			Gdiplus::Color(Gdiplus::Color::OrangeRed).ToCOLORREF(),
			Gdiplus::Color(Gdiplus::Color::LightGoldenrodYellow).ToCOLORREF(),
			Gdiplus::Color(Gdiplus::Color::LightGreen).ToCOLORREF(),
			Gdiplus::Color(Gdiplus::Color::LightBlue).ToCOLORREF()
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
	auto st = m_Data->Info().Time.AsSystemTime();
	SystemTimeToTzSpecificLocalTime(nullptr, &st, &st);
	m_ctlDate.SetSystemTime(GDT_VALID, &st);
	m_ctlTime.SetSystemTime(GDT_VALID, &st);
	if (type == Recalc::All || type == Recalc::Planets) {
		m_Planets = m_Data->AllPlanets();
		Sort(GetSortInfo(m_ctlPlanets));
		m_ctlPlanets.SetItemCountEx((int)m_Planets.size(), LVSICF_NOSCROLL);
	}
}

LRESULT CChartDetailsView::OnInitView(UINT, WPARAM, LPARAM, BOOL&) {
	m_ctlHouseSystem.Attach(GetDlgItem(IDC_HOUSESYSTEM));
	m_ctlDate.Attach(GetDlgItem(IDC_DATE));
	m_ctlTime.Attach(GetDlgItem(IDC_TIME));
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

	{
		CUpDownCtrl ud(GetDlgItem(IDC_LATDEGUD));
		ud.SetRange(0, 89);
		ud.Detach();
		ud.Attach(GetDlgItem(IDC_LATMINUD));
		ud.SetRange(0, 59);
	}
	{
		CUpDownCtrl ud(GetDlgItem(IDC_LONDEGUD));
		ud.SetRange(0, 179);
		ud.Detach();
		ud.Attach(GetDlgItem(IDC_LONMINUD));
		ud.SetRange(0, 59);
	}
	LOGFONT lf;
	CFontHandle(m_ctlPlanets.GetFont()).GetLogFont(lf);
	lf.lfHeight = lf.lfHeight * 120 / 100;
	CFontHandle local;
	local.CreateFontIndirect(&lf);
	m_ctlPlanets.SetFont(local);
	m_ctlHouses.SetFont(local);
	wcscpy_s(lf.lfFaceName, L"HamburgSymbols");
	m_Font.CreateFontIndirect(&lf);

	HouseSystem systems[] = {
		HouseSystem::Placidus,
		HouseSystem::Koch,
		HouseSystem::Porphyrius,
		HouseSystem::Regiomontanus,
		HouseSystem::Campanus,
		HouseSystem::Equal,
		HouseSystem::Morinus,
		HouseSystem::Topocentric,
		HouseSystem::Alcabitus,
		HouseSystem::Horizontal,
		HouseSystem::Krusinski,
		HouseSystem::EqualWholeSign,
		HouseSystem::CarterPoliEqu,
		HouseSystem::EqualMC,
		HouseSystem::Sunshine,
		HouseSystem::SunshineAlt,
		HouseSystem::APCHouses,
	};

	for (int i = 0; i < _countof(systems); i++) {
		int n = m_ctlHouseSystem.AddString(StringHelper::HouseSystemToString(systems[i]));
		m_ctlHouseSystem.SetItemData(n, (int)systems[i]);
	}

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

LRESULT CChartDetailsView::OnDateChanged(int, LPNMHDR, BOOL&) {
	SYSTEMTIME st;
	m_ctlDate.GetSystemTime(&st);
	TzSpecificLocalTimeToSystemTime(nullptr, &st, &st);
	auto& info = m_Data->Info();
	info.Time.SetDate(st.wYear, st.wMonth, st.wDay);
	if (m_NotifyWnd) {
		m_NotifyWnd.SendMessageW(WM_RECALC);
		UpdateControls();
	}
	return 0;
}

LRESULT CChartDetailsView::OnTimeChanged(int, LPNMHDR, BOOL&) {
	SYSTEMTIME st;
	m_ctlTime.GetSystemTime(&st);
	TzSpecificLocalTimeToSystemTime(nullptr, &st, &st);
	auto& info = m_Data->Info();
	info.Time.SetTime(st.wHour, st.wMinute, st.wSecond);
	if (m_NotifyWnd) {
		m_NotifyWnd.SendMessageW(WM_RECALC);
		UpdateControls();
	}
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

namespace {
	struct HereRequest {
		HWND Wnd;
		ChartInfo Info;
	};
}

LRESULT CChartDetailsView::OnHere(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_Data);
	GetDlgItem(IDC_HERE).EnableWindow(FALSE);

	auto req = new HereRequest{ m_hWnd, m_Data->Info() };
	::TrySubmitThreadpoolCallback([](auto, auto ctx) {
		auto req = static_cast<HereRequest*>(ctx);
		auto success = NetworkHelper::FillInfoFromCurrentLocation(req->Info, req->Wnd);
		if (!::PostMessage(req->Wnd, WM_HERE_RESULT, (WPARAM)success, (LPARAM)req))
			delete req;
		}, req, nullptr);

	return 0;
}

LRESULT CChartDetailsView::OnHereResult(UINT, WPARAM wParam, LPARAM lParam, BOOL&) {
	std::unique_ptr<HereRequest> req(reinterpret_cast<HereRequest*>(lParam));
	GetDlgItem(IDC_HERE).EnableWindow(TRUE);

	if (!wParam) {
		AtlMessageBox(m_hWnd, L"Failed to determine current location.", L"Astro Studio", MB_ICONWARNING);
		return 0;
	}
	if (!m_Data)
		return 0;

	auto& info = m_Data->Info();
	info.Latitude = req->Info.Latitude;
	info.Longitude = req->Info.Longitude;
	info.Elevation = req->Info.Elevation;
	info.City = req->Info.City;
	info.State = req->Info.State;
	info.Country = req->Info.Country;

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

void CChartDetailsView::ApplyLocationFromControls() {
	ATLASSERT(m_Data);
	auto& info = m_Data->Info();

	auto latDeg = GetDlgItemInt(IDC_LATDEG);
	auto latMin = GetDlgItemInt(IDC_LATMIN);
	info.Latitude = (latDeg + latMin / 60.0) * (IsDlgButtonChecked(IDC_SOUTH) ? -1 : 1);

	auto lonDeg = GetDlgItemInt(IDC_LONDEG);
	auto lonMin = GetDlgItemInt(IDC_LONMIN);
	info.Longitude = (lonDeg + lonMin / 60.0) * (IsDlgButtonChecked(IDC_WEST) ? -1 : 1);
}

LRESULT CChartDetailsView::OnLocationChanged(WORD, WORD, HWND, BOOL&) {
	if (m_Data == nullptr || m_UpdatingLocationControls)
		return 0;

	// The user has taken control of the location; a late geolocation result
	// must not overwrite it.
	m_LocationEdited = true;
	m_LocationPending = false;

	ApplyLocationFromControls();
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

	ApplyLocationFromControls();

	SYSTEMTIME sdate, stime;
	m_ctlDate.GetSystemTime(&sdate);
	m_ctlTime.GetSystemTime(&stime);
	TzSpecificLocalTimeToSystemTime(nullptr, &sdate, &sdate);
	TzSpecificLocalTimeToSystemTime(nullptr, &stime, &stime);
	info.Time.SetDate(sdate.wYear, sdate.wMonth, sdate.wDay);
	info.Time.SetTime(stime.wHour, stime.wMinute, stime.wSecond);

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
