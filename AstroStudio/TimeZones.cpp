#include "pch.h"
#include "TimeZones.h"
#include <cmath>

// pch.h targets Windows 8.1, but icu.h is only there from Windows 10 1709 (NTDDI_WIN10_RS3). The legacy
// import libraries load the system's icuuc.dll/icuin.dll (1703+); icu.lib would need icu.dll (1903+).
#pragma push_macro("NTDDI_VERSION")
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN10_RS3
#include <icu.h>
#pragma pop_macro("NTDDI_VERSION")

#pragma comment(lib, "icuuc.lib")
#pragma comment(lib, "icuin.lib")

namespace {
	constexpr long MinSystemYear = 1601, MaxSystemYear = 30827;

	DateTime ToDateTime(LocalDateTime const& l) {
		return DateTime(l.Year, l.Month, (double)l.Day, l.Hour, l.Minute, l.Second, DateTime::AfterPapalReform(l.Year, l.Month, l.Day));
	}

	// Julian day to fields, with half a second added so a whole-second time that lands a hair below
	// its value in floating point doesn't come back as 59.999 seconds and truncate to the second before
	LocalDateTime ToFields(double jd) {
		jd += 0.5 / 86400;
		DateTime dt(jd, DateTime::AfterPapalReform(jd));
		LocalDateTime l;
		double sec;
		dt.Get(l.Year, l.Month, l.Day, l.Hour, l.Minute, sec);
		l.Second = (long)sec;
		return l;
	}

	bool ToSystemTime(LocalDateTime const& l, SYSTEMTIME& st) {
		if (l.Year < MinSystemYear || l.Year > MaxSystemYear)
			return false;
		st = {};
		st.wYear = (WORD)l.Year;
		st.wMonth = (WORD)l.Month;
		st.wDay = (WORD)l.Day;
		st.wHour = (WORD)l.Hour;
		st.wMinute = (WORD)l.Minute;
		st.wSecond = (WORD)l.Second;
		return true;
	}

	// minutes from b to a, both read as plain calendar times
	bool MinutesBetween(SYSTEMTIME const& a, SYSTEMTIME const& b, int& minutes) {
		FILETIME fa, fb;
		if (!::SystemTimeToFileTime(&a, &fa) || !::SystemTimeToFileTime(&b, &fb))
			return false;
		auto ticks = [](FILETIME const& ft) { return (long long)(((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime); };
		minutes = (int)std::llround((ticks(fa) - ticks(fb)) / 600000000.0);
		return true;
	}

	CString RegistryDisplayName(PCWSTR key) {
		CString path(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones\\");
		path += key;
		WCHAR text[256];
		DWORD size = sizeof(text);
		if (::RegGetValue(HKEY_LOCAL_MACHINE, path, L"Display", RRF_RT_REG_SZ, nullptr, text, &size) == ERROR_SUCCESS)
			return text;
		return L"";
	}
}

std::vector<TimeZoneEntry> const& TimeZones::All() {
	static auto zones = [] {
		std::vector<TimeZoneEntry> list;
		for (DWORD i = 0;; i++) {
			DYNAMIC_TIME_ZONE_INFORMATION info{};
			if (::EnumDynamicTimeZoneInformation(i, &info) != ERROR_SUCCESS)
				break;
			if (info.TimeZoneKeyName[0] == 0)
				continue;
			TimeZoneEntry entry;
			entry.Key = info.TimeZoneKeyName;
			entry.Display = RegistryDisplayName(info.TimeZoneKeyName);
			if (entry.Display.IsEmpty())
				entry.Display = info.StandardName;
			entry.BaseOffset = -(int)(info.Bias + info.StandardBias);
			entry.Info = info;
			list.push_back(std::move(entry));
		}
		std::sort(list.begin(), list.end(), [](auto const& a, auto const& b) {
			return a.BaseOffset != b.BaseOffset ? a.BaseOffset < b.BaseOffset : a.Display < b.Display;
			});
		return list;
		}();
	return zones;
}

TimeZoneEntry const* TimeZones::Find(std::wstring const& key) {
	for (auto const& zone : All())
		if (_wcsicmp(zone.Key.c_str(), key.c_str()) == 0)
			return &zone;
	return nullptr;
}

std::wstring TimeZones::WindowsKeyFromIana(std::wstring const& ianaId) {
	if (ianaId.empty())
		return L"";

	UErrorCode status = U_ZERO_ERROR;
	char16_t key[128];
	auto length = ucal_getWindowsTimeZoneID(reinterpret_cast<char16_t const*>(ianaId.c_str()), (int32_t)ianaId.size(), key, _countof(key), &status);
	if (U_FAILURE(status) || length <= 0 || length >= (int32_t)_countof(key))
		return L"";

	std::wstring windowsKey(reinterpret_cast<wchar_t const*>(key), length);
	// only a zone this machine actually has can be selected
	return Find(windowsKey) ? windowsKey : L"";
}

TimeZoneInfo TimeZones::Machine() {
	DYNAMIC_TIME_ZONE_INFORMATION info{};
	auto id = ::GetDynamicTimeZoneInformation(&info);

	TimeZoneInfo tz;
	if (Find(info.TimeZoneKeyName))
		tz.Name = info.TimeZoneKeyName;
	tz.OffsetUT = -(int)(info.Bias + (id == TIME_ZONE_ID_DAYLIGHT ? info.DaylightBias : info.StandardBias));
	return tz;
}

DateTime TimeZones::LocalToUt(LocalDateTime const& local, TimeZoneInfo& tz) {
	int offset = tz.OffsetUT;
	if (auto zone = tz.Name.empty() ? nullptr : Find(tz.Name)) {
		offset = zone->BaseOffset;
		SYSTEMTIME st, utc;
		int found;
		if (ToSystemTime(local, st) && ::TzSpecificLocalTimeToSystemTimeEx(&zone->Info, &st, &utc) && MinutesBetween(st, utc, found))
			offset = found;
	}
	tz.OffsetUT = offset;

	auto jd = ToDateTime(local).Julian() - offset / 1440.0;
	return DateTime(jd, DateTime::AfterPapalReform(jd));
}

LocalDateTime TimeZones::UtToLocal(DateTime const& ut, TimeZoneInfo const& tz, int* offsetOut) {
	int offset = tz.OffsetUT;
	if (auto zone = tz.Name.empty() ? nullptr : Find(tz.Name)) {
		offset = zone->BaseOffset;
		SYSTEMTIME utc, local;
		int found;
		if (ToSystemTime(ToFields(ut.Julian()), utc) && ::SystemTimeToTzSpecificLocalTimeEx(&zone->Info, &utc, &local) && MinutesBetween(local, utc, found))
			offset = found;
	}
	if (offsetOut)
		*offsetOut = offset;
	return ToFields(ut.Julian() + offset / 1440.0);
}

DateTime TimeZones::FieldsToDateTime(LocalDateTime const& fields) {
	return ToDateTime(fields);
}

LocalDateTime TimeZones::DateTimeToFields(DateTime const& dt) {
	return ToFields(dt.Julian());
}

CString TimeZones::FormatOffset(int minutes) {
	CString text;
	text.Format(L"%c%02d:%02d", minutes < 0 ? L'-' : L'+', std::abs(minutes) / 60, std::abs(minutes) % 60);
	return text;
}

// [+-]h, [+-]hh:mm or [+-]h:mm, within the range of offsets that have ever been used (-15:59 .. +15:59)
bool TimeZones::ParseOffset(PCWSTR text, int& minutes) {
	CString s(text);
	s.Trim();
	if (s.IsEmpty())
		return false;

	int sign = 1, pos = 0;
	if (s[0] == L'+' || s[0] == L'-') {
		sign = s[0] == L'-' ? -1 : 1;
		pos = 1;
	}
	auto number = [&](int& value) {
		int digits = 0;
		value = 0;
		while (pos < s.GetLength() && iswdigit(s[pos]) && digits < 3) {
			value = value * 10 + (s[pos++] - L'0');
			digits++;
		}
		return digits > 0;
	};

	int hours, mins = 0;
	if (!number(hours))
		return false;
	if (pos < s.GetLength()) {
		if (s[pos++] != L':' || !number(mins))
			return false;
	}
	if (pos != s.GetLength() || hours > 15 || mins > 59)
		return false;

	minutes = sign * (hours * 60 + mins);
	return true;
}
