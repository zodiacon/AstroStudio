#pragma once

#include "TimeZones.h"

enum class StepUnit {
	Minute, Hour, Day, Week, Month, Year,
};

// Moves a chart's time forward or backward by whole units.
//
// Minutes and hours are exact durations of UT. Days, weeks, months and years are calendar steps in the
// chart's own wall-clock time, so 21:00 stays 21:00 across a DST change (the day is then 23 or 25 hours
// of UT). A month or year step that lands on a day the target month doesn't have goes to that month's
// last day (31 January + 1 month = 28 or 29 February); one that lands in the days the Gregorian reform
// skipped (5-14 October 1582) goes to 15 October.
struct TimeStep abstract final {
	// Steps by count units (negative = backward). On success ut and tz (whose OffsetUT is refreshed) are
	// updated and true is returned; if the result would leave the years the ephemeris covers, nothing changes
	// and false is returned.
	static bool Step(DateTime& ut, TimeZoneInfo& tz, StepUnit unit, int count);
};
