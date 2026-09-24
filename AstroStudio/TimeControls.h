#pragma once

#include "TimeZones.h"

// The date/time/time zone controls, shared by every window that has them:
//   IDC_DAY, IDC_MONTH (drop-down lists), IDC_YEAR (edit), IDC_TIME (edit, "21:45:10"),
//   IDC_TIMEZONE (drop-down list of Windows time zones), and the manual override -
//   IDC_MANUALTZ (check box) with IDC_TZOFFSET (edit, "+05:30"), which replaces the zone with a fixed UT offset.
// The date and time shown are wall-clock time in the chosen zone; the chart itself keeps UT.
// The owner keeps the message handling and calls into this.
class CTimeControls {
public:
	// Years the built-in ephemeris can calculate (astronomical numbering: 0 is 1 BC).
	static constexpr long MinYear = MinChartYear, MaxYear = MaxChartYear;

	enum class Error {
		None,
		Year,		// not a number, or outside MinYear..MaxYear
		Date,		// no such day (30 February, or one of the days skipped by the Gregorian reform)
		Time,		// not a time (see ParseTime)
		Offset,		// manual offset isn't in the form [+-]h[h][:mm]
	};

	void Init(CWindow parent);

	// shows a UT time as wall-clock time in the given zone
	void Set(DateTime const& ut, TimeZoneInfo const& tz);
	// Reads the controls back into UT and the zone used. tz.OffsetUT is the offset in effect at that time.
	Error Get(DateTime& ut, TimeZoneInfo& tz) const;
	// Selects a Windows time zone, unless the manual override is on. Returns false if it wasn't selected.
	bool SetZone(std::wstring const& key);
	bool IsManual() const;

	// Reads a time as people write it: 21:45:10, 21:45, 2145, 214510, 9.30, 9:30 pm, 9:30:15 PM, 12 am, ...
	// Without am/pm it is a 24-hour time; with it the hour is 1-12. Missing minutes and seconds are 0.
	static bool ParseTime(PCWSTR text, int& hour, int& minute, int& second);
	// Rewrites the time box as 24-hour HH:MM:SS if it holds a time; leaves it alone (for the user to fix) if not.
	void NormalizeTime();

	// control that has the problem, for focusing
	static UINT ControlFor(Error error);
	static PCWSTR Message(Error error);

	// To call when the month or year changed: rebuilds the list of days
	void UpdateDays();
	// To call when the manual check box was clicked
	void ManualToggled();

private:
	void UpdateEnabled();
	bool SelectZone(std::wstring const& key);
	bool GetYear(long& year) const;

	CWindow m_Parent;
	CComboBox m_Day, m_Month, m_Zone;
};
