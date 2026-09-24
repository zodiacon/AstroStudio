#include "pch.h"
#include "NewChartDlg.h"
#include "Helpers.h"
#include "StringHelper.h"
#include "TimeZones.h"
#include <cmath>

namespace {
	struct ChartTypeItem {
		PCWSTR Text;
		InfoType Type;
	};
	const ChartTypeItem ChartTypes[] = {
		{ L"Unknown", InfoType::Unknown },
		{ L"Male", InfoType::Male },
		{ L"Female", InfoType::Female },
		{ L"Event", InfoType::Event },
	};

	CString FullName(ChartInfo const& info) {
		CString name(info.LastName.c_str());
		if (!info.FirstName.empty()) {
			if (!name.IsEmpty())
				name += L", ";
			name += info.FirstName.c_str();
		}
		return name;
	}
}

void CNewChartDlg::SetChartInfo(ChartInfo const& info) {
	m_Info = info;
}

void CNewChartDlg::SetHouseSystem(HouseSystem system) {
	m_HouseSystem = system;
}

ChartInfo const& CNewChartDlg::GetChartInfo() const {
	return m_Info;
}

HouseSystem CNewChartDlg::GetHouseSystem() const {
	return m_HouseSystem;
}

CString CNewChartDlg::GetTitle() const {
	return FullName(m_Info);
}

LRESULT CNewChartDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	CenterWindow(GetParent());
	AddIconToButton(IDC_NOW, IDI_CLOCK);
	AddIconToButton(IDC_HERE, IDI_PIN);
	AddIconToButton(IDC_LOOKUP, IDI_GLOBE);

	m_Location.Init(m_hWnd);
	m_Time.Init(m_hWnd);

	m_ctlHouseSystem.Attach(GetDlgItem(IDC_HOUSESYSTEM));
	Helpers::FillHouseSystems(m_ctlHouseSystem);
	m_ctlHouseSystem.SelectString(-1, StringHelper::HouseSystemToString(m_HouseSystem));

	m_ctlType.Attach(GetDlgItem(IDC_CHARTTYPE));
	for (auto const& item : ChartTypes) {
		int n = m_ctlType.AddString(item.Text);
		m_ctlType.SetItemData(n, (DWORD_PTR)item.Type);
		if (item.Type == m_Info.Type)
			m_ctlType.SetCurSel(n);
	}

	SetDlgItemText(IDC_NAME, FullName(m_Info));
	m_Location.Set(m_Info);
	m_OriginalLocation = CLocationControls::FormatLocation(m_Info);
	m_Time.Set(m_Info.Time, m_Info.TimeZone);
	return TRUE;
}

LRESULT CNewChartDlg::OnHere(WORD, WORD, HWND, BOOL&) {
	m_Location.BeginHere(m_Info);
	return 0;
}

LRESULT CNewChartDlg::OnHereResult(UINT, WPARAM wParam, LPARAM lParam, BOOL&) {
	ChartInfo found;
	if (!m_Location.EndHere(wParam, lParam, found))
		return 0;

	m_Info.Latitude = found.Latitude;
	m_Info.Longitude = found.Longitude;
	m_Info.Elevation = found.Elevation;
	m_Info.City = found.City;
	m_Info.State = found.State;
	m_Info.Country = found.Country;
	m_Location.Set(m_Info);
	m_OriginalLocation = CLocationControls::FormatLocation(m_Info);
	return 0;
}

LRESULT CNewChartDlg::OnLookup(WORD, WORD, HWND, BOOL&) {
	CString text;
	GetDlgItemText(IDC_LOCATION, text);
	m_Location.BeginLookup(text);
	return 0;
}

LRESULT CNewChartDlg::OnLookupResult(UINT, WPARAM wParam, LPARAM lParam, BOOL&) {
	PlaceResult place;
	if (!m_Location.EndLookup(wParam, lParam, place))
		return 0;

	CLocationControls::ApplyPlace(place, m_Info);
	m_Location.Set(m_Info);
	m_OriginalLocation = CLocationControls::FormatLocation(m_Info);

	// The date and time typed stay as they are; they are now read in the new place's zone. With the
	// manual override on the zone is left to the user.
	if (!place.TimeZone.empty() && !m_Time.SetZone(TimeZones::WindowsKeyFromIana(place.TimeZone)) && !m_Time.IsManual()) {
		AtlMessageBox(m_hWnd, L"The place was found, but its time zone could not be matched to one of Windows' zones. Choose the time zone yourself.",
			L"New Chart", MB_ICONINFORMATION);
	}
	return 0;
}

LRESULT CNewChartDlg::OnNow(WORD, WORD, HWND, BOOL&) {
	// the current moment, shown in whichever zone is selected
	DateTime ut;
	TimeZoneInfo tz;
	if (m_Time.Get(ut, tz) != CTimeControls::Error::None)
		tz = TimeZones::Machine();
	m_Time.Set(DateTime::Now(), tz);
	return 0;
}

LRESULT CNewChartDlg::OnMonthOrYearChanged(WORD, WORD, HWND, BOOL&) {
	m_Time.UpdateDays();
	return 0;
}

LRESULT CNewChartDlg::OnTimeKillFocus(WORD, WORD, HWND, BOOL&) {
	m_Time.NormalizeTime();		// "9.30 pm" becomes 21:30:00
	return 0;
}

LRESULT CNewChartDlg::OnManualToggled(WORD, WORD, HWND, BOOL&) {
	m_Time.ManualToggled();
	return 0;
}

bool CNewChartDlg::Fail(UINT control, PCWSTR message) {
	AtlMessageBox(m_hWnd, message, L"New Chart", MB_ICONWARNING);
	if (control)
		GetDlgItem(control).SetFocus();
	return false;
}

LRESULT CNewChartDlg::OnOK(WORD, WORD, HWND, BOOL&) {
	// start from what the chart came in with, so anything the dialog doesn't edit (elevation, middle name) survives
	auto info = m_Info;

	CString name;
	GetDlgItemText(IDC_NAME, name);
	name.Trim();
	// "Last, First" - the way the details view shows it - or just a name
	int comma = name.Find(L',');
	info.LastName = comma < 0 ? L"" : (PCWSTR)name.Left(comma).Trim();
	info.FirstName = (PCWSTR)(comma < 0 ? name : name.Mid(comma + 1).Trim());
	info.Type = (InfoType)m_ctlType.GetItemData(m_ctlType.GetCurSel());

	if (GetDlgItemInt(IDC_LATMIN) > 59)
		return Fail(IDC_LATMIN, L"The minutes of latitude must be between 0 and 59."), 0;
	if (GetDlgItemInt(IDC_LONMIN) > 59)
		return Fail(IDC_LONMIN, L"The minutes of longitude must be between 0 and 59."), 0;
	m_Location.GetCoordinates(info);
	if (std::abs(info.Latitude) > 90)
		return Fail(IDC_LATDEG, L"The latitude must be between 0 and 90 degrees."), 0;
	if (std::abs(info.Longitude) > 180)
		return Fail(IDC_LONDEG, L"The longitude must be between 0 and 180 degrees."), 0;

	CString location;
	GetDlgItemText(IDC_LOCATION, location);
	location.Trim();
	if (location != m_OriginalLocation) {
		// typed by hand: it is just a label, kept whole
		info.City = (PCWSTR)location;
		info.State.clear();
		info.Country.clear();
	}

	DateTime ut;
	TimeZoneInfo tz;
	auto error = m_Time.Get(ut, tz);
	if (error != CTimeControls::Error::None)
		return Fail(CTimeControls::ControlFor(error), CTimeControls::Message(error)), 0;
	info.Time = ut;
	info.TimeZone = tz;

	m_Info = std::move(info);
	m_HouseSystem = (HouseSystem)m_ctlHouseSystem.GetItemData(m_ctlHouseSystem.GetCurSel());
	EndDialog(IDOK);
	return 0;
}

LRESULT CNewChartDlg::OnCancel(WORD, WORD, HWND, BOOL&) {
	EndDialog(IDCANCEL);
	return 0;
}
