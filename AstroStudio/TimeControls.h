#pragma once

#include "TimeZones.h"

// The date/time/time zone controls, shared by every window that has them:
//   IDC_DAY, IDC_MONTH (drop-down lists), IDC_YEAR (edit), IDC_TIME (time picker),
//   IDC_TIMEZONE (drop-down list of Windows time zones), and the manual override -
//   IDC_MANUALTZ (check box) with IDC_TZOFFSET (edit, "+05:30"), which replaces the zone with a fixed UT offset.
// The date and time shown are wall-clock time in the chosen zone; the chart itself keeps UT.
// The owner keeps the message handling and calls into this.
class CTimeControls {
public:
	// Years the built-in ephemeris can calculate (astronomical numbering: 0 is 1 BC).
	static constexpr long MinYear = -3000, MaxYear = 3000;

	enum class Error {
		None,
		Year,		// not a number, or outside MinYear..MaxYear
		Date,		// no such day (30 February, or one of the days skipped by the Gregorian reform)
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
	CDateTimePickerCtrl m_Time;
};
