#include "pch.h"
#include "LocationControls.h"
#include "Helpers.h"
#include "NetworkHelper.h"
#include "resource.h"

namespace {
	struct HereRequest {
		HWND Wnd;
		ChartInfo Info;
	};

	struct LookupRequest {
		HWND Wnd;
		std::wstring Query;
		std::vector<PlaceResult> Results;
	};

	CString Describe(PlaceResult const& place) {
		CString text(place.Name.c_str());
		for (auto part : { &place.State, &place.Country }) {
			if (!part->empty()) {
				text += L", ";
				text += part->c_str();
			}
		}
		return text;
	}
}

void CLocationControls::Init(CWindow parent) {
	m_Parent = parent;

	CUpDownCtrl ud(m_Parent.GetDlgItem(IDC_LATDEGUD));
	ud.SetRange(0, 89);
	ud.Detach();
	ud.Attach(m_Parent.GetDlgItem(IDC_LATMINUD));
	ud.SetRange(0, 59);
	ud.Detach();
	ud.Attach(m_Parent.GetDlgItem(IDC_LONDEGUD));
	ud.SetRange(0, 179);
	ud.Detach();
	ud.Attach(m_Parent.GetDlgItem(IDC_LONMINUD));
	ud.SetRange(0, 59);
	ud.Detach();
}

void CLocationControls::Set(ChartInfo const& info, bool pending) {
	m_Updating = true;
	{
		auto [deg, min, _] = Helpers::GetDegMinSec(info.Latitude);
		m_Parent.SetDlgItemInt(IDC_LATDEG, deg);
		m_Parent.SetDlgItemInt(IDC_LATMIN, min);
		//
		// CheckRadioButton, not CheckDlgButton: the latter only sets the state
		// of the button named, leaving the other one checked too. Auto-radio
		// buttons clear their siblings when the *user* clicks, not on a
		// programmatic BM_SETCHECK - so switching to a southern or western
		// location used to light up both halves of the pair.
		//
		m_Parent.CheckRadioButton(IDC_NORTH, IDC_SOUTH, info.Latitude >= 0 ? IDC_NORTH : IDC_SOUTH);
	}
	{
		auto [deg, min, _] = Helpers::GetDegMinSec(info.Longitude);
		m_Parent.SetDlgItemInt(IDC_LONDEG, deg);
		m_Parent.SetDlgItemInt(IDC_LONMIN, min);
		m_Parent.CheckRadioButton(IDC_EAST, IDC_WEST, info.Longitude >= 0 ? IDC_EAST : IDC_WEST);
	}
	m_Updating = false;

	m_Parent.SetDlgItemText(IDC_LOCATION, pending ? CString(L"Locating...") : FormatLocation(info));
}

void CLocationControls::GetCoordinates(ChartInfo& info) const {
	auto latDeg = m_Parent.GetDlgItemInt(IDC_LATDEG);
	auto latMin = m_Parent.GetDlgItemInt(IDC_LATMIN);
	info.Latitude = (latDeg + latMin / 60.0) * (m_Parent.IsDlgButtonChecked(IDC_SOUTH) ? -1 : 1);

	auto lonDeg = m_Parent.GetDlgItemInt(IDC_LONDEG);
	auto lonMin = m_Parent.GetDlgItemInt(IDC_LONMIN);
	info.Longitude = (lonDeg + lonMin / 60.0) * (m_Parent.IsDlgButtonChecked(IDC_WEST) ? -1 : 1);
}

// Join only the parts we actually have. Concatenating unconditionally
// rendered an unknown location as a bare ", ", which is now visible
// whenever the lookup fails.
CString CLocationControls::FormatLocation(ChartInfo const& info) {
	std::wstring const* parts[] = { &info.City, &info.State, &info.Country };
	CString location;
	for (auto part : parts) {
		if (part->empty())
			continue;
		if (!location.IsEmpty())
			location += L", ";
		location += part->c_str();
	}
	return location;
}

void CLocationControls::BeginHere(ChartInfo const& base) {
	m_Parent.GetDlgItem(IDC_HERE).EnableWindow(FALSE);

	auto req = new HereRequest{ m_Parent, base };
	if (!::TrySubmitThreadpoolCallback([](auto, auto ctx) {
		auto req = static_cast<HereRequest*>(ctx);
		auto success = NetworkHelper::FillInfoFromCurrentLocation(req->Info, req->Wnd);
		if (!::PostMessage(req->Wnd, WM_HERE_RESULT, (WPARAM)success, (LPARAM)req))
			delete req;
		}, req, nullptr)) {
		delete req;
		m_Parent.GetDlgItem(IDC_HERE).EnableWindow(TRUE);
	}
}

bool CLocationControls::EndHere(WPARAM success, LPARAM lParam, ChartInfo& result) {
	std::unique_ptr<HereRequest> req(reinterpret_cast<HereRequest*>(lParam));
	m_Parent.GetDlgItem(IDC_HERE).EnableWindow(TRUE);

	if (!success) {
		AtlMessageBox(m_Parent, L"Failed to determine current location.", L"Astro Studio", MB_ICONWARNING);
		return false;
	}

	result.Latitude = req->Info.Latitude;
	result.Longitude = req->Info.Longitude;
	result.Elevation = req->Info.Elevation;
	result.City = req->Info.City;
	result.State = req->Info.State;
	result.Country = req->Info.Country;
	return true;
}

bool CLocationControls::BeginLookup(PCWSTR query) {
	CString text(query);
	text.Trim();
	if (text.IsEmpty() || text == L"Locating...") {
		AtlMessageBox(m_Parent, L"Type the name of a place in the Location box first.", L"Astro Studio", MB_ICONINFORMATION);
		return false;
	}

	m_Parent.GetDlgItem(IDC_LOOKUP).EnableWindow(FALSE);
	auto req = new LookupRequest{ m_Parent, (PCWSTR)text, {} };
	if (!::TrySubmitThreadpoolCallback([](auto, auto ctx) {
		auto req = static_cast<LookupRequest*>(ctx);
		auto success = NetworkHelper::SearchPlaces(req->Query, req->Results);
		if (!::PostMessage(req->Wnd, WM_LOOKUP_RESULT, (WPARAM)success, (LPARAM)req))
			delete req;
		}, req, nullptr)) {
		delete req;
		m_Parent.GetDlgItem(IDC_LOOKUP).EnableWindow(TRUE);
		return false;
	}
	return true;
}

bool CLocationControls::EndLookup(WPARAM success, LPARAM lParam, PlaceResult& chosen) {
	std::unique_ptr<LookupRequest> req(reinterpret_cast<LookupRequest*>(lParam));
	CWindow button(m_Parent.GetDlgItem(IDC_LOOKUP));
	button.EnableWindow(TRUE);

	if (!success) {
		AtlMessageBox(m_Parent, L"The place lookup service could not be reached.", L"Astro Studio", MB_ICONWARNING);
		return false;
	}
	if (req->Results.empty()) {
		CString message;
		message.Format(L"No place was found for \"%s\".", req->Query.c_str());
		AtlMessageBox(m_Parent, (PCWSTR)message, L"Astro Studio", MB_ICONINFORMATION);
		return false;
	}
	if (req->Results.size() == 1) {
		chosen = req->Results[0];
		return true;
	}

	// several matches: let the user pick, right under the button
	CMenu menu;
	menu.CreatePopupMenu();
	for (size_t i = 0; i < req->Results.size(); i++) {
		auto const& place = req->Results[i];
		CString text = Describe(place);
		if (place.Population > 0) {
			CString population;
			population.Format(L"  (population %lld)", place.Population);
			text += population;
		}
		menu.AppendMenu(MF_STRING, i + 1, text);
	}

	CRect rc;
	button.GetWindowRect(&rc);
	auto choice = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN, rc.left, rc.bottom, m_Parent);
	if (choice <= 0)
		return false;

	chosen = req->Results[choice - 1];
	return true;
}

void CLocationControls::ApplyPlace(PlaceResult const& place, ChartInfo& info) {
	info.Latitude = place.Latitude;
	info.Longitude = place.Longitude;
	info.Elevation = place.Elevation;
	info.City = place.Name;
	info.State = place.State;
	info.Country = place.Country;
}
