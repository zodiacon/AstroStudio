#pragma once

#include "DateTime.h"
#include "TimeControls.h"

// A day drop-down, a month drop-down and a year box that a dialog has under control ids of its own, for a date (UT midnight):
// the days of the month follow the month and year, the years are the ones the ephemeris covers, and a date before 15 October
// 1582 is in the Julian calendar, like everywhere in the program.
class CDateBoxes {
public:
	void Init(CWindow parent, UINT day, UINT month, UINT year) {
		m_Parent = parent;
		m_DayId = day;
		m_MonthId = month;
		m_YearId = year;
		m_Day = parent.GetDlgItem(day);
		m_Month = parent.GetDlgItem(month);
		static PCWSTR const names[] = {
			L"January", L"February", L"March", L"April", L"May", L"June",
			L"July", L"August", L"September", L"October", L"November", L"December",
		};
		for (auto name : names)
			m_Month.AddString(name);
		for (int i = 1; i <= 31; i++) {
			CString text;
			text.Format(L"%d", i);
			m_Day.AddString(text);
		}
		parent.GetDlgItem(year).SendMessage(EM_LIMITTEXT, 5);		// "-3000"
	}

	void Set(DateTime const& date) {
		m_Parent.SetDlgItemInt(m_YearId, date.Year(), TRUE);
		m_Month.SetCurSel(date.Month() - 1);
		Update();
		m_Day.SetCurSel(date.Day() - 1);
	}

	// Reads the date (UT midnight). False if it isn't one, with the control to put right and what is wrong.
	bool Get(DateTime& date, UINT& control, PCWSTR& problem) const {
		long year;
		if (!GetYear(year)) {
			control = m_YearId;
			problem = L"The year must be a number from -3000 to 3000 (0 is 1 BC).";
			return false;
		}
		long month = m_Month.GetCurSel() + 1, day = m_Day.GetCurSel() + 1;
		if (year == 1582 && month == 10 && day > 4 && day < 15) {
			control = m_DayId;
			problem = L"The days from 5 to 14 October 1582 do not exist: the calendar reform skipped them.";
			return false;
		}
		date = DateTime(year, month, (double)day, 0, 0, 0, DateTime::AfterPapalReform(year, month, day));
		return true;
	}

	// To call when the month or the year changed: 29 February needs a leap year in whichever calendar the date falls in
	void Update() {
		int month = m_Month.GetCurSel() + 1;
		long year = 2000;
		if (!GetYear(year))
			year = 2000;
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

private:
	bool GetYear(long& year) const {
		CString text;
		m_Parent.GetDlgItemText(m_YearId, text);
		text.Trim();
		if (text.IsEmpty())
			return false;
		PWSTR end;
		year = wcstol(text, &end, 10);
		return *end == 0 && year >= CTimeControls::MinYear && year <= CTimeControls::MaxYear;
	}

	CWindow m_Parent;
	UINT m_DayId{ 0 }, m_MonthId{ 0 }, m_YearId{ 0 };
	CComboBox m_Day, m_Month;
};
