#include "pch.h"
#include "ChartFile.h"
#include "Helpers.h"
#include "StringHelper.h"
#include "TimeZones.h"
#include "TimeControls.h"
#include <IniDocument.h>
#include <cmath>

namespace {
	constexpr int CurrentVersion = 1;

	struct InfoTypeName {
		PCWSTR Name;
		InfoType Type;
	};
	const InfoTypeName InfoTypes[] = {
		{ L"Unknown", InfoType::Unknown }, { L"Male", InfoType::Male }, { L"Female", InfoType::Female }, { L"Event", InfoType::Event },
	};

	// names match regardless of case and spaces ("True Node", "truenode")
	std::wstring Normalize(std::wstring_view text) {
		std::wstring result;
		for (auto ch : text)
			if (ch != L' ' && ch != L'\t')
				result += (wchar_t)towlower(ch);
		return result;
	}

	std::wstring FormatDate(LocalDateTime const& local) {
		wchar_t text[32];
		swprintf_s(text, L"%s%04ld-%02ld-%02ld", local.Year < 0 ? L"-" : L"", std::labs(local.Year), local.Month, local.Day);
		return text;
	}

	std::wstring FormatTime(LocalDateTime const& local) {
		wchar_t text[16];
		swprintf_s(text, L"%02ld:%02ld:%02ld", local.Hour, local.Minute, local.Second);
		return text;
	}

	// [-]YYYY-MM-DD (the year can be shorter, '/' works as well as '-')
	bool ParseDate(std::wstring_view text, long& year, long& month, long& day) {
		size_t pos = 0;
		auto number = [&](long& value, int maxDigits) {
			int digits = 0;
			value = 0;
			while (pos < text.size() && iswdigit(text[pos]) && digits < maxDigits) {
				value = value * 10 + (text[pos++] - L'0');
				digits++;
			}
			return digits > 0;
		};
		auto separator = [&]() {
			if (pos < text.size() && (text[pos] == L'-' || text[pos] == L'/')) {
				pos++;
				return true;
			}
			return false;
		};

		bool negative = false;
		if (pos < text.size() && (text[pos] == L'-' || text[pos] == L'+'))
			negative = text[pos++] == L'-';
		if (!number(year, 5) || !separator() || !number(month, 2) || !separator() || !number(day, 2) || pos != text.size())
			return false;
		if (negative)
			year = -year;
		return true;
	}

	bool ParseHouseSystem(std::wstring_view name, HouseSystem& system) {
		auto wanted = Normalize(name);
		for (auto candidate : Helpers::HouseSystems()) {
			if (Normalize(StringHelper::HouseSystemToString(candidate)) == wanted) {
				system = candidate;
				return true;
			}
		}
		return false;
	}

	bool ParsePlanet(std::wstring_view name, Planet& planet) {
		auto wanted = Normalize(name);
		for (int i = 0; i < (int)Planet::NumPlanets; i++) {
			if (Normalize(Helpers::GetPlanetName((Planet)i)) == wanted) {
				planet = (Planet)i;
				return true;
			}
		}
		return false;
	}

	bool SameLocalTime(LocalDateTime const& a, LocalDateTime const& b) {
		return a.Year == b.Year && a.Month == b.Month && a.Day == b.Day && a.Hour == b.Hour && a.Minute == b.Minute && a.Second == b.Second;
	}
}

bool ChartFile::Save(ChartData const& chart, PCWSTR path, std::wstring& error) {
	auto const& info = chart.Info();

	IniDocument ini;
	ini.SetHeader(L"Astro Studio chart\n"
		L"The time is local time in TimeZone; UtOffset is the offset from UT in effect at that moment.\n"
		L"Dates before 15 October 1582 are in the Julian calendar. Years are astronomical (0 is 1 BC).\n"
		L"Latitude and longitude are in decimal degrees: north and east are positive.");

	ini.SetInt(L"Chart", L"Version", CurrentVersion);
	ini.SetString(L"Chart", L"HouseSystem", StringHelper::HouseSystemToString(chart.GetHouseSystem()));
	ini.SetInt(L"Chart", L"Harmonic", chart.Harmonic());
	std::wstring planets;
	for (auto const& planet : chart.AllPlanets()) {
		if (!planets.empty())
			planets += L", ";
		planets += Helpers::GetPlanetName(planet.Planet);
	}
	ini.SetString(L"Chart", L"Planets", planets);

	ini.SetString(L"Person", L"FirstName", info.FirstName);
	ini.SetString(L"Person", L"MiddleName", info.MiddleName);
	ini.SetString(L"Person", L"LastName", info.LastName);
	for (auto const& type : InfoTypes)
		if (type.Type == info.Type)
			ini.SetString(L"Person", L"Type", type.Name);

	int offset = 0;
	auto local = TimeZones::UtToLocal(info.Time, info.TimeZone, &offset);
	ini.SetString(L"Time", L"Date", FormatDate(local));
	ini.SetString(L"Time", L"Time", FormatTime(local));
	ini.SetString(L"Time", L"TimeZone", info.TimeZone.Name);
	ini.SetString(L"Time", L"UtOffset", (PCWSTR)TimeZones::FormatOffset(offset));

	ini.SetString(L"Location", L"City", info.City);
	ini.SetString(L"Location", L"State", info.State);
	ini.SetString(L"Location", L"Country", info.Country);
	ini.SetDouble(L"Location", L"Latitude", info.Latitude, 9);
	ini.SetDouble(L"Location", L"Longitude", info.Longitude, 9);
	ini.SetDouble(L"Location", L"Elevation", info.Elevation, 3);

	if (!ini.Save(path)) {
		error = ini.Error();
		return false;
	}
	return true;
}

bool ChartFile::Load(PCWSTR path, ChartData& chart, std::wstring& error) {
	IniDocument ini;
	if (!ini.Load(path)) {
		error = ini.Error();
		return false;
	}

	// "[Section] Key (line 12): what is wrong"
	auto problem = [&](std::wstring_view section, std::wstring_view key, std::wstring const& what) {
		error = L"[" + std::wstring(section) + L"] " + std::wstring(key);
		if (int line = ini.LineOf(section, key))
			error += L" (line " + std::to_wstring(line) + L")";
		error += L": " + what;
		return false;
	};
	auto missing = [&](std::wstring_view section, std::wstring_view key) {
		error = L"[" + std::wstring(section) + L"] " + std::wstring(key) + L" is missing";
		return false;
	};

	//
	// [Chart]
	//
	auto version = ini.GetInt(L"Chart", L"Version");
	if (!version) {
		error = L"this is not a chart file (there is no [Chart] Version)";
		return false;
	}
	if (*version > CurrentVersion) {
		error = L"this chart was saved by a newer version of the program (file version " + std::to_wstring(*version) + L")";
		return false;
	}

	HouseSystem houseSystem = HouseSystem::Koch;
	if (auto name = ini.Get(L"Chart", L"HouseSystem"); name && !ParseHouseSystem(*name, houseSystem))
		return problem(L"Chart", L"HouseSystem", L"'" + *name + L"' is not a house system this program knows");

	int harmonic = 1;
	if (ini.Has(L"Chart", L"Harmonic")) {
		auto value = ini.GetInt(L"Chart", L"Harmonic");
		if (!value || *value < 1 || *value > 9999)
			return problem(L"Chart", L"Harmonic", L"must be a whole number from 1 to 9999");
		harmonic = *value;
	}

	std::vector<Planet> planets;
	if (auto list = ini.Get(L"Chart", L"Planets")) {
		size_t pos = 0;
		while (pos <= list->size()) {
			size_t end = list->find(L',', pos);
			if (end == std::wstring::npos)
				end = list->size();
			auto name = std::wstring_view(*list).substr(pos, end - pos);
			pos = end + 1;
			if (Normalize(name).empty())
				continue;
			Planet planet;
			if (!ParsePlanet(name, planet))
				return problem(L"Chart", L"Planets", L"'" + std::wstring(name) + L"' is not a planet or point this program knows");
			if (std::find(planets.begin(), planets.end(), planet) == planets.end())
				planets.push_back(planet);
		}
	}
	else {
		planets = Helpers::GetStandardPlanets();
		planets.insert(planets.end(), { Planet::Chiron, Planet::TrueNode, Planet::Lilith });
	}

	//
	// [Person]
	//
	ChartInfo info{};
	info.FirstName = ini.GetString(L"Person", L"FirstName");
	info.MiddleName = ini.GetString(L"Person", L"MiddleName");
	info.LastName = ini.GetString(L"Person", L"LastName");
	if (auto name = ini.Get(L"Person", L"Type")) {
		bool found = false;
		for (auto const& type : InfoTypes) {
			if (Normalize(type.Name) == Normalize(*name)) {
				info.Type = type.Type;
				found = true;
			}
		}
		if (!found)
			return problem(L"Person", L"Type", L"must be Unknown, Male, Female or Event");
	}

	//
	// [Location]
	//
	info.City = ini.GetString(L"Location", L"City");
	info.State = ini.GetString(L"Location", L"State");
	info.Country = ini.GetString(L"Location", L"Country");
	auto latitude = ini.GetDouble(L"Location", L"Latitude");
	if (!latitude)
		return ini.Has(L"Location", L"Latitude") ? problem(L"Location", L"Latitude", L"is not a number (decimal degrees, north positive)") : missing(L"Location", L"Latitude");
	auto longitude = ini.GetDouble(L"Location", L"Longitude");
	if (!longitude)
		return ini.Has(L"Location", L"Longitude") ? problem(L"Location", L"Longitude", L"is not a number (decimal degrees, east positive)") : missing(L"Location", L"Longitude");
	if (std::abs(*latitude) > 90)
		return problem(L"Location", L"Latitude", L"must be between -90 and 90");
	if (std::abs(*longitude) > 180)
		return problem(L"Location", L"Longitude", L"must be between -180 and 180");
	info.Latitude = *latitude;
	info.Longitude = *longitude;
	if (ini.Has(L"Location", L"Elevation")) {
		auto elevation = ini.GetDouble(L"Location", L"Elevation");
		if (!elevation)
			return problem(L"Location", L"Elevation", L"is not a number (meters)");
		info.Elevation = *elevation;
	}

	//
	// [Time]: the local date and time, then the zone
	//
	auto dateText = ini.Get(L"Time", L"Date");
	if (!dateText)
		return missing(L"Time", L"Date");
	LocalDateTime local;
	if (!ParseDate(*dateText, local.Year, local.Month, local.Day))
		return problem(L"Time", L"Date", L"'" + *dateText + L"' is not a date; write it as YYYY-MM-DD, e.g. 1950-03-15");
	bool gregorian = DateTime::AfterPapalReform(local.Year, local.Month, local.Day);
	bool skipped = local.Year == 1582 && local.Month == 10 && local.Day > 4 && local.Day < 15;
	if (local.Year < CTimeControls::MinYear || local.Year > CTimeControls::MaxYear)
		return problem(L"Time", L"Date", L"the year must be between " + std::to_wstring(CTimeControls::MinYear) + L" and " + std::to_wstring(CTimeControls::MaxYear));
	if (local.Month < 1 || local.Month > 12 || local.Day < 1 || skipped || local.Day > DateTime::DaysInMonth(local.Month, DateTime::IsLeap(local.Year, gregorian)))
		return problem(L"Time", L"Date", L"that date doesn't exist");

	int hour = 12, minute = 0, second = 0;		// noon when no time is given
	if (auto timeText = ini.Get(L"Time", L"Time")) {
		if (!CTimeControls::ParseTime(timeText->c_str(), hour, minute, second))
			return problem(L"Time", L"Time", L"'" + *timeText + L"' is not a time; write it as HH:MM:SS, e.g. 21:45:10");
	}
	local.Hour = hour;
	local.Minute = minute;
	local.Second = second;

	std::optional<int> storedOffset;
	if (auto text = ini.Get(L"Time", L"UtOffset"); text && !text->empty()) {
		int minutes;
		if (!TimeZones::ParseOffset(text->c_str(), minutes))
			return problem(L"Time", L"UtOffset", L"'" + *text + L"' is not an offset from UT; write it like +05:30 or -4");
		storedOffset = minutes;
	}

	TimeZoneInfo zone;
	auto zoneName = ini.GetString(L"Time", L"TimeZone");
	if (!zoneName.empty()) {
		if (auto known = TimeZones::Find(zoneName)) {
			zone.Name = known->Key;
		}
		else if (storedOffset) {
			zone.OffsetUT = *storedOffset;		// not a zone this computer has: keep the time by its offset
		}
		else {
			return problem(L"Time", L"TimeZone", L"'" + zoneName + L"' is not a time zone on this computer; add a UtOffset to use a fixed offset");
		}
	}
	else if (storedOffset) {
		zone.OffsetUT = *storedOffset;		// no zone: a fixed offset
	}
	else {
		error = L"[Time] needs a TimeZone or a UtOffset";
		return false;
	}

	info.Time = TimeZones::LocalToUt(local, zone);
	if (!zone.Name.empty() && storedOffset && *storedOffset != zone.OffsetUT) {
		// The zone's rules gave another offset than the one saved. If the saved one is also a valid reading
		// of this local time (the repeated hour when DST ends) it is the one that was meant.
		double jd = TimeZones::FieldsToDateTime(local).Julian() - *storedOffset / 1440.0;
		DateTime other(jd, DateTime::AfterPapalReform(jd));
		if (SameLocalTime(TimeZones::UtToLocal(other, zone), local)) {
			info.Time = other;
			zone.OffsetUT = *storedOffset;
		}
	}
	info.TimeZone = zone;

	ChartData data;
	data.Info() = std::move(info);
	data.SetHouseSystem(houseSystem);
	data.Harmonic(harmonic);
	data.AddPlanets(planets);
	chart = std::move(data);
	return true;
}
