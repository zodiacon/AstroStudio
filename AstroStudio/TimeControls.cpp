#include "pch.h"
#include "TimeControls.h"
#include "resource.h"

namespace {
	PCWSTR const MonthNames[] = {
		L"January", L"February", L"March", L"April", L"May", L"June",
		L"July", L"August", L"September", L"October", L"November", L"December",
	};
}

void CTimeControls::Init(CWindow parent) {
	m_Parent = parent;
	m_Day.Attach(m_Parent.GetDlgItem(IDC_DAY));
	m_Month.Attach(m_Parent.GetDlgItem(IDC_MONTH));
	m_Zone.Attach(m_Parent.GetDlgItem(IDC_TIMEZONE));
	m_Time.Attach(m_Parent.GetDlgItem(IDC_TIME));

	for (auto name : MonthNames)
		m_Month.AddString(name);
	m_Month.SetCurSel(0);

	for (int day = 1; day <= 31; day++) {
		CString text;
		text.Format(L"%d", day);
		m_Day.AddString(text);
	}
	m_Day.SetCurSel(0);

	auto const& zones = TimeZones::All();
	for (int i = 0; i < (int)zones.size(); i++) {
		int n = m_Zone.AddString(zones[i].Display);
		m_Zone.SetItemData(n, i);
	}

	m_Parent.GetDlgItem(IDC_YEAR).SendMessage(EM_LIMITTEXT, 5);		// "-3000"
	m_Parent.GetDlgItem(IDC_TZOFFSET).SendMessage(EM_LIMITTEXT, 6);	// "+05:30"
	UpdateEnabled();
}

void CTimeControls::Set(DateTime const& ut, TimeZoneInfo const& tz) {
	auto local = TimeZones::UtToLocal(ut, tz);

	m_Parent.SetDlgItemInt(IDC_YEAR, local.Year, TRUE);
	m_Month.SetCurSel(local.Month - 1);
	UpdateDays();
	m_Day.SetCurSel(local.Day - 1);

	SYSTEMTIME st{};
	st.wYear = 2000;
	st.wMonth = 1;
	st.wDay = 1;
	st.wHour = (WORD)local.Hour;
	st.wMinute = (WORD)local.Minute;
	st.wSecond = (WORD)local.Second;
	m_Time.SetSystemTime(GDT_VALID, &st);

	// A manual chart has no zone of its own; the list still shows one, so that switching the override
	// off lands on something sensible.
	SelectZone(tz.Name.empty() ? TimeZones::Machine().Name : tz.Name);

	bool manual = tz.Name.empty() || !TimeZones::Find(tz.Name);
	m_Parent.CheckDlgButton(IDC_MANUALTZ, manual ? BST_CHECKED : BST_UNCHECKED);
	m_Parent.SetDlgItemText(IDC_TZOFFSET, TimeZones::FormatOffset(tz.OffsetUT));
	UpdateEnabled();
}

bool CTimeControls::SelectZone(std::wstring const& key) {
	auto zone = TimeZones::Find(key);
	for (int i = 0; zone && i < m_Zone.GetCount(); i++) {
		if (&TimeZones::All()[m_Zone.GetItemData(i)] == zone) {
			m_Zone.SetCurSel(i);
			return true;
		}
	}
	m_Zone.SetCurSel(-1);
	return false;
}

bool CTimeControls::SetZone(std::wstring const& key) {
	if (m_Parent.IsDlgButtonChecked(IDC_MANUALTZ))
		return false;
	return SelectZone(key);
}

bool CTimeControls::IsManual() const {
	return m_Parent.IsDlgButtonChecked(IDC_MANUALTZ) != 0;
}

bool CTimeControls::GetYear(long& year) const {
	CString text;
	m_Parent.GetDlgItemText(IDC_YEAR, text);
	text.Trim();
	if (text.IsEmpty())
		return false;

	PWSTR end;
	year = wcstol(text, &end, 10);
	return *end == 0 && year >= MinYear && year <= MaxYear;
}

CTimeControls::Error CTimeControls::Get(DateTime& ut, TimeZoneInfo& tz) const {
	LocalDateTime local;
	if (!GetYear(local.Year))
		return Error::Year;

	local.Month = m_Month.GetCurSel() + 1;
	local.Day = m_Day.GetCurSel() + 1;
	bool gregorian = DateTime::AfterPapalReform(local.Year, local.Month, local.Day);
	bool skipped = local.Year == 1582 && local.Month == 10 && local.Day > 4 && local.Day < 15;
	if (skipped || local.Day > DateTime::DaysInMonth(local.Month, DateTime::IsLeap(local.Year, gregorian)))
		return Error::Date;

	SYSTEMTIME st;
	m_Time.GetSystemTime(&st);
	local.Hour = st.wHour;
	local.Minute = st.wMinute;
	local.Second = st.wSecond;

	TimeZoneInfo zone;
	if (m_Parent.IsDlgButtonChecked(IDC_MANUALTZ)) {
		CString text;
		m_Parent.GetDlgItemText(IDC_TZOFFSET, text);
		if (!TimeZones::ParseOffset(text, zone.OffsetUT))
			return Error::Offset;
	}
	else {
		int n = m_Zone.GetCurSel();
		if (n >= 0)
			zone.Name = TimeZones::All()[m_Zone.GetItemData(n)].Key;
		else
			zone.OffsetUT = TimeZones::Machine().OffsetUT;
	}

	ut = TimeZones::LocalToUt(local, zone);
	tz = std::move(zone);
	return Error::None;
}

UINT CTimeControls::ControlFor(Error error) {
	switch (error) {
		case Error::Year: return IDC_YEAR;
		case Error::Date: return IDC_DAY;
		case Error::Offset: return IDC_TZOFFSET;
	}
	return 0;
}

PCWSTR CTimeControls::Message(Error error) {
	switch (error) {
		case Error::Year: return L"The year must be a number between -3000 and 3000 (0 is 1 BC, -1 is 2 BC).";
		case Error::Date: return L"That date doesn't exist.";
		case Error::Offset: return L"The UT offset must look like +05:30, -4 or +0:45 (up to 15:59).";
	}
	return L"";
}

void CTimeControls::UpdateDays() {
	int month = m_Month.GetCurSel() + 1;
	long year = 2000;
	if (!GetYear(year))
		year = 2000;

	// 29 February needs a leap year in whichever calendar the date falls in
	bool gregorian = DateTime::AfterPapalReform(year, month, 1);
	int count = DateTime::DaysInMonth(month, DateTime::IsLeap(year, gregorian));

	int selected = m_Day.GetCurSel();
	while (m_Day.GetCount() > count)
		m_Day.DeleteString(m_Day.GetCount() - 1);
	while (m_Day.GetCount() < count) {
		CString text;
		text.Format(L"%d", m_Day.GetCount() + 1);
		m_Day.AddString(text);
	}
	m_Day.SetCurSel(std::min(std::max(selected, 0), count - 1));
}

void CTimeControls::ManualToggled() {
	if (m_Parent.IsDlgButtonChecked(IDC_MANUALTZ)) {
		// Start from what the zone says for the date shown. Get reads the zone only while the override
		// is off, so switch it off for the moment.
		m_Parent.CheckDlgButton(IDC_MANUALTZ, BST_UNCHECKED);
		DateTime ut;
		TimeZoneInfo tz;
		if (Get(ut, tz) == Error::None)
			m_Parent.SetDlgItemText(IDC_TZOFFSET, TimeZones::FormatOffset(tz.OffsetUT));
		m_Parent.CheckDlgButton(IDC_MANUALTZ, BST_CHECKED);
	}
	UpdateEnabled();
}

void CTimeControls::UpdateEnabled() {
	bool manual = m_Parent.IsDlgButtonChecked(IDC_MANUALTZ) != 0;
	m_Zone.EnableWindow(!manual);
	m_Parent.GetDlgItem(IDC_TZOFFSET).EnableWindow(manual);
}
