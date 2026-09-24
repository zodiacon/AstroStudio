#pragma once

#include "ChartData.h"

// A wall-clock date and time, in whatever time zone it was entered in.
struct LocalDateTime {
	long Year{ 2000 }, Month{ 1 }, Day{ 1 };
	long Hour{ 0 }, Minute{ 0 }, Second{ 0 };
};

struct TimeZoneEntry {
	std::wstring Key;					// registry key name, e.g. L"Eastern Standard Time"
	CString Display;					// e.g. "(UTC-05:00) Eastern Time (US & Canada)"
	int BaseOffset;						// standard (non-DST) offset, minutes east of UT
	DYNAMIC_TIME_ZONE_INFORMATION Info;
};

// Conversions between UT and wall-clock time for a TimeZoneInfo: either a Windows time zone, whose
// DST rules for the year in question are used, or a manual fixed offset (TimeZoneInfo::Name empty).
//
// The Windows APIs only cover years 1601-30827 (and only know the rules Windows ships, which get thin
// before the 1970s); outside that range, or for a zone that no longer exists, the zone's standard
// offset (or the stored offset) is used.
struct TimeZones abstract final {
	// all Windows time zones, ordered by UT offset then name
	static std::vector<TimeZoneEntry> const& All();
	static TimeZoneEntry const* Find(std::wstring const& key);

	// The Windows time zone key for an IANA id ("America/New_York" -> "Eastern Standard Time"), via ICU.
	// Empty if there is no such Windows zone. Several IANA zones share one Windows zone, so this loses
	// their separate histories. Needs COM initialized on the calling thread (the legacy ICU libraries do).
	static std::wstring WindowsKeyFromIana(std::wstring const& ianaId);

	// this machine's time zone, with the offset in effect right now
	static TimeZoneInfo Machine();

	// Wall-clock time in the zone to UT. tz.OffsetUT is updated to the offset in effect at that time.
	static DateTime LocalToUt(LocalDateTime const& local, TimeZoneInfo& tz);
	static LocalDateTime UtToLocal(DateTime const& ut, TimeZoneInfo const& tz);

	// "+05:30" style text
	static CString FormatOffset(int minutes);
	static bool ParseOffset(PCWSTR text, int& minutes);
};
