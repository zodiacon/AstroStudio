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

	m_Parent.GetDlgItem(IDC_TIME).SendMessage(EM_LIMITTEXT, 14);		// "12:30:45 p.m."
	m_Parent.GetDlgItem(IDC_TIME).SendMessage(EM_SETCUEBANNER, TRUE, (LPARAM)L"hh:mm:ss");
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

	CString time;
	time.Format(L"%02d:%02d:%02d", local.Hour, local.Minute, local.Second);
	m_Parent.SetDlgItemText(IDC_TIME, time);

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

	CString time;
	m_Parent.GetDlgItemText(IDC_TIME, time);
	int hour, minute, second;
	if (!ParseTime(time, hour, minute, second))
		return Error::Time;
	local.Hour = hour;
	local.Minute = minute;
	local.Second = second;

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
		case Error::Time: return IDC_TIME;
		case Error::Offset: return IDC_TZOFFSET;
	}
	return 0;
}

PCWSTR CTimeControls::Message(Error error) {
	switch (error) {
		case Error::Year: return L"The year must be a number between -3000 and 3000 (0 is 1 BC, -1 is 2 BC).";
		case Error::Date: return L"That date doesn't exist.";
		case Error::Time: return L"The time must look like 21:45:10 or 9:30 pm (hours, minutes and seconds; 24-hour unless am/pm is given).";
		case Error::Offset: return L"The UT offset must look like +05:30, -4 or +0:45 (up to 15:59).";
	}
	return L"";
}

bool CTimeControls::ParseTime(PCWSTR text, int& hour, int& minute, int& second) {
	CString s(text);
	s.Trim();
	s.MakeLower();
	if (s.IsEmpty())
		return false;

	// an optional am/pm at the end: "pm", "p.m.", "p", with or without a space before it
	int meridiem = 0;		// 1 = am, 2 = pm
	int letters = 0;
	while (letters < s.GetLength() && !iswalpha(s[letters]))
		letters++;
	if (letters < s.GetLength()) {
		CString suffix;
		for (int i = letters; i < s.GetLength(); i++)
			if (s[i] != L'.' && s[i] != L' ')
				suffix += s[i];
		if (suffix == L"am" || suffix == L"a")
			meridiem = 1;
		else if (suffix == L"pm" || suffix == L"p")
			meridiem = 2;
		else
			return false;
		s = s.Left(letters);
		s.Trim();
	}

	// the numbers: either one run of digits (930, 2145, 214510) or up to three groups split by : . or space
	int values[3] = { 0, 0, 0 };
	int count = 0;
	bool digitsOnly = true;
	for (int i = 0; i < s.GetLength(); i++)
		if (!iswdigit(s[i]))
			digitsOnly = false;
	if (s.IsEmpty())
		return false;

	if (digitsOnly) {
		int length = s.GetLength();
		if (length > 6)
			return false;
		if (length <= 2) {
			values[0] = _wtoi(s);
			count = 1;
		}
		else {
			// the last two digits are the seconds if there are five or six of them, else the minutes
			int hourDigits = length % 2 == 0 ? length - (length == 6 ? 4 : 2) : length - (length == 5 ? 4 : 2);
			values[0] = _wtoi(s.Left(hourDigits));
			CString rest = s.Mid(hourDigits);
			values[1] = _wtoi(rest.Left(2));
			count = 2;
			if (rest.GetLength() == 4) {
				values[2] = _wtoi(rest.Mid(2));
				count = 3;
			}
		}
	}
	else {
		int pos = 0;
		while (pos <= s.GetLength()) {
			int start = pos;
			while (pos < s.GetLength() && iswdigit(s[pos]))
				pos++;
			int digits = pos - start;
			if (digits < 1 || digits > 2 || count >= 3)
				return false;
			values[count++] = _wtoi(s.Mid(start, digits));
			if (pos == s.GetLength())
				break;
			if (s[pos] != L':' && s[pos] != L'.' && s[pos] != L' ')
				return false;
			pos++;
		}
	}

	if (values[1] > 59 || values[2] > 59)
		return false;
	if (meridiem) {
		if (values[0] < 1 || values[0] > 12)
			return false;
		values[0] %= 12;		// 12 am is 0, 12 pm is 12
		if (meridiem == 2)
			values[0] += 12;
	}
	else if (values[0] > 23) {
		return false;
	}

	hour = values[0];
	minute = values[1];
	second = values[2];
	return true;
}

void CTimeControls::NormalizeTime() {
	CString text;
	m_Parent.GetDlgItemText(IDC_TIME, text);
	int hour, minute, second;
	if (!text.IsEmpty() && ParseTime(text, hour, minute, second)) {
		text.Format(L"%02d:%02d:%02d", hour, minute, second);
		m_Parent.SetDlgItemText(IDC_TIME, text);
	}
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
