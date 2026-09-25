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

namespace {
	// "[Section] Key (line 12): what is wrong"
	struct Reader {
		IniDocument const& Ini;
		std::wstring& Error;

		bool Problem(std::wstring_view section, std::wstring_view key, std::wstring const& what) const {
			Error = L"[" + std::wstring(section) + L"] " + std::wstring(key);
			if (int line = Ini.LineOf(section, key))
				Error += L" (line " + std::to_wstring(line) + L")";
			Error += L": " + what;
			return false;
		}
		bool Missing(std::wstring_view section, std::wstring_view key) const {
			Error = L"[" + std::wstring(section) + L"] " + std::wstring(key) + L" is missing";
			return false;
		}
	};

	// the local date, time and zone of a moment, in a section of its own: Date, Time, TimeZone, UtOffset
	void WriteWhen(IniDocument& ini, std::wstring const& section, DateTime const& ut, TimeZoneInfo const& zone) {
		int offset = 0;
		auto local = TimeZones::UtToLocal(ut, zone, &offset);
		ini.SetString(section, L"Date", FormatDate(local));
		ini.SetString(section, L"Time", FormatTime(local));
		ini.SetString(section, L"TimeZone", zone.Name);
		ini.SetString(section, L"UtOffset", (PCWSTR)TimeZones::FormatOffset(offset));
	}

	bool ReadWhen(Reader const& r, std::wstring const& section, DateTime& time, TimeZoneInfo& zoneInfo) {
		auto const& ini = r.Ini;
		auto dateText = ini.Get(section, L"Date");
		if (!dateText)
			return r.Missing(section, L"Date");
		LocalDateTime local;
		if (!ParseDate(*dateText, local.Year, local.Month, local.Day))
			return r.Problem(section, L"Date", L"'" + *dateText + L"' is not a date; write it as YYYY-MM-DD, e.g. 1950-03-15");
		bool gregorian = DateTime::AfterPapalReform(local.Year, local.Month, local.Day);
		bool skipped = local.Year == 1582 && local.Month == 10 && local.Day > 4 && local.Day < 15;
		if (local.Year < CTimeControls::MinYear || local.Year > CTimeControls::MaxYear)
			return r.Problem(section, L"Date", L"the year must be between " + std::to_wstring(CTimeControls::MinYear) + L" and " + std::to_wstring(CTimeControls::MaxYear));
		if (local.Month < 1 || local.Month > 12 || local.Day < 1 || skipped || local.Day > DateTime::DaysInMonth(local.Month, DateTime::IsLeap(local.Year, gregorian)))
			return r.Problem(section, L"Date", L"that date doesn't exist");

		int hour = 12, minute = 0, second = 0;		// noon when no time is given
		if (auto timeText = ini.Get(section, L"Time")) {
			if (!CTimeControls::ParseTime(timeText->c_str(), hour, minute, second))
				return r.Problem(section, L"Time", L"'" + *timeText + L"' is not a time; write it as HH:MM:SS, e.g. 21:45:10");
		}
		local.Hour = hour;
		local.Minute = minute;
		local.Second = second;

		std::optional<int> storedOffset;
		if (auto text = ini.Get(section, L"UtOffset"); text && !text->empty()) {
			int minutes;
			if (!TimeZones::ParseOffset(text->c_str(), minutes))
				return r.Problem(section, L"UtOffset", L"'" + *text + L"' is not an offset from UT; write it like +05:30 or -4");
			storedOffset = minutes;
		}

		TimeZoneInfo zone;
		auto zoneName = ini.GetString(section, L"TimeZone");
		if (!zoneName.empty()) {
			if (auto known = TimeZones::Find(zoneName)) {
				zone.Name = known->Key;
			}
			else if (storedOffset) {
				zone.OffsetUT = *storedOffset;		// not a zone this computer has: keep the time by its offset
			}
			else {
				return r.Problem(section, L"TimeZone", L"'" + zoneName + L"' is not a time zone on this computer; add a UtOffset to use a fixed offset");
			}
		}
		else if (storedOffset) {
			zone.OffsetUT = *storedOffset;		// no zone: a fixed offset
		}
		else {
			r.Error = L"[" + section + L"] needs a TimeZone or a UtOffset";
			return false;
		}

		time = TimeZones::LocalToUt(local, zone);
		if (!zone.Name.empty() && storedOffset && *storedOffset != zone.OffsetUT) {
			// The zone's rules gave another offset than the one saved. If the saved one is also a valid reading
			// of this local time (the repeated hour when DST ends) it is the one that was meant.
			double jd = TimeZones::FieldsToDateTime(local).Julian() - *storedOffset / 1440.0;
			DateTime other(jd, DateTime::AfterPapalReform(jd));
			if (SameLocalTime(TimeZones::UtToLocal(other, zone), local)) {
				time = other;
				zone.OffsetUT = *storedOffset;
			}
		}
		zoneInfo = zone;
		return true;
	}

	// a chart in the four sections [<prefix>Chart], [<prefix>Person], [<prefix>Time], [<prefix>Location]
	void WriteChart(IniDocument& ini, ChartData const& chart, std::wstring const& prefix) {
		auto const& info = chart.Info();
		auto section = [&](PCWSTR name) { return prefix + name; };

		ini.SetInt(section(L"Chart"), L"Version", CurrentVersion);
		ini.SetString(section(L"Chart"), L"HouseSystem", StringHelper::HouseSystemToString(chart.GetHouseSystem()));
		ini.SetInt(section(L"Chart"), L"Harmonic", chart.Harmonic());
		std::wstring planets;
		for (auto const& planet : chart.AllPlanets()) {
			if (planet.Planet == Planet::PartOfFortune)
				continue;		// (that is a matter of the option, not of the chart)
			if (!planets.empty())
				planets += L", ";
			planets += Helpers::GetPlanetName(planet.Planet);
		}
		ini.SetString(section(L"Chart"), L"Planets", planets);

		ini.SetString(section(L"Person"), L"FirstName", info.FirstName);
		ini.SetString(section(L"Person"), L"MiddleName", info.MiddleName);
		ini.SetString(section(L"Person"), L"LastName", info.LastName);
		for (auto const& type : InfoTypes)
			if (type.Type == info.Type)
				ini.SetString(section(L"Person"), L"Type", type.Name);

		WriteWhen(ini, section(L"Time"), info.Time, info.TimeZone);

		ini.SetString(section(L"Location"), L"City", info.City);
		ini.SetString(section(L"Location"), L"State", info.State);
		ini.SetString(section(L"Location"), L"Country", info.Country);
		ini.SetDouble(section(L"Location"), L"Latitude", info.Latitude, 9);
		ini.SetDouble(section(L"Location"), L"Longitude", info.Longitude, 9);
		ini.SetDouble(section(L"Location"), L"Elevation", info.Elevation, 3);
	}

	bool ReadChart(Reader const& r, std::wstring const& prefix, ChartData& chart) {
		auto const& ini = r.Ini;
		auto section = [&](PCWSTR name) { return prefix + name; };
		auto chartSection = section(L"Chart"), personSection = section(L"Person"), locationSection = section(L"Location");

		//
		// [Chart]
		//
		auto version = ini.GetInt(chartSection, L"Version");
		if (!version) {
			r.Error = prefix.empty() ? L"this is not a chart file (there is no [Chart] Version)" : L"[" + chartSection + L"] Version is missing";
			return false;
		}
		if (*version > CurrentVersion) {
			r.Error = L"this chart was saved by a newer version of the program (file version " + std::to_wstring(*version) + L")";
			return false;
		}

		HouseSystem houseSystem = HouseSystem::Koch;
		if (auto name = ini.Get(chartSection, L"HouseSystem"); name && !ParseHouseSystem(*name, houseSystem))
			return r.Problem(chartSection, L"HouseSystem", L"'" + *name + L"' is not a house system this program knows");

		int harmonic = 1;
		if (ini.Has(chartSection, L"Harmonic")) {
			auto value = ini.GetInt(chartSection, L"Harmonic");
			if (!value || *value < 1 || *value > 9999)
				return r.Problem(chartSection, L"Harmonic", L"must be a whole number from 1 to 9999");
			harmonic = *value;
		}

		std::vector<Planet> planets;
		if (auto list = ini.Get(chartSection, L"Planets")) {
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
					return r.Problem(chartSection, L"Planets", L"'" + std::wstring(name) + L"' is not a planet or point this program knows");
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
		info.FirstName = ini.GetString(personSection, L"FirstName");
		info.MiddleName = ini.GetString(personSection, L"MiddleName");
		info.LastName = ini.GetString(personSection, L"LastName");
		if (auto name = ini.Get(personSection, L"Type")) {
			bool found = false;
			for (auto const& type : InfoTypes) {
				if (Normalize(type.Name) == Normalize(*name)) {
					info.Type = type.Type;
					found = true;
				}
			}
			if (!found)
				return r.Problem(personSection, L"Type", L"must be Unknown, Male, Female or Event");
		}

		//
		// [Location]
		//
		info.City = ini.GetString(locationSection, L"City");
		info.State = ini.GetString(locationSection, L"State");
		info.Country = ini.GetString(locationSection, L"Country");
		auto latitude = ini.GetDouble(locationSection, L"Latitude");
		if (!latitude)
			return ini.Has(locationSection, L"Latitude") ? r.Problem(locationSection, L"Latitude", L"is not a number (decimal degrees, north positive)") : r.Missing(locationSection, L"Latitude");
		auto longitude = ini.GetDouble(locationSection, L"Longitude");
		if (!longitude)
			return ini.Has(locationSection, L"Longitude") ? r.Problem(locationSection, L"Longitude", L"is not a number (decimal degrees, east positive)") : r.Missing(locationSection, L"Longitude");
		if (std::abs(*latitude) > 90)
			return r.Problem(locationSection, L"Latitude", L"must be between -90 and 90");
		if (std::abs(*longitude) > 180)
			return r.Problem(locationSection, L"Longitude", L"must be between -180 and 180");
		info.Latitude = *latitude;
		info.Longitude = *longitude;
		if (ini.Has(locationSection, L"Elevation")) {
			auto elevation = ini.GetDouble(locationSection, L"Elevation");
			if (!elevation)
				return r.Problem(locationSection, L"Elevation", L"is not a number (meters)");
			info.Elevation = *elevation;
		}

		//
		// [Time]: the local date and time, then the zone
		//
		if (!ReadWhen(r, section(L"Time"), info.Time, info.TimeZone))
			return false;

		ChartData data;
		data.Info() = std::move(info);
		data.SetHouseSystem(houseSystem);
		data.Harmonic(harmonic);
		data.AddPlanets(planets);
		chart = std::move(data);
		return true;
	}

	constexpr PCWSTR ChartHeader = L"Astro Studio chart\n"
		L"The time is local time in TimeZone; UtOffset is the offset from UT in effect at that moment.\n"
		L"Dates before 15 October 1582 are in the Julian calendar. Years are astronomical (0 is 1 BC).\n"
		L"Latitude and longitude are in decimal degrees: north and east are positive.";

	struct KindName {
		PCWSTR Name;
		DerivedKind Kind;
	};
	const KindName Kinds[] = { { L"Composite", DerivedKind::Composite }, { L"Davison", DerivedKind::Davison }, { L"SolarArc", DerivedKind::SolarArc } };
	struct HousesName {
		PCWSTR Name;
		CompositeHouses Houses;
	};
	const HousesName HouseMethods[] = { { L"MidpointMC", CompositeHouses::MidpointMC }, { L"MidpointARMC", CompositeHouses::MidpointARMC } };
	struct KeyName {
		PCWSTR Name;
		ArcKey Key;
	};
	const KeyName Keys[] = { { L"Actual", ArcKey::Actual }, { L"Naibod", ArcKey::Naibod }, { L"Ptolemy", ArcKey::Ptolemy } };
}

bool ChartFile::Save(ChartData const& chart, PCWSTR path, std::wstring& error) {
	IniDocument ini;
	ini.SetHeader(ChartHeader);
	WriteChart(ini, chart, L"");
	if (!ini.Save(path)) {
		error = ini.Error();
		return false;
	}
	return true;
}

bool ChartFile::SaveDerived(DerivedRecipe const& recipe, PCWSTR path, std::wstring& error) {
	IniDocument ini;
	ini.SetHeader(L"Astro Studio derived chart\n"
		L"A chart worked out from other charts: they are stored below (as in an ordinary chart file, with the prefix A. or B.) and\n"
		L"the chart is made from them again when this file is opened.");
	ini.SetInt(L"Derived", L"Version", CurrentVersion);
	for (auto const& kind : Kinds)
		if (kind.Kind == recipe.Kind)
			ini.SetString(L"Derived", L"Kind", kind.Name);
	if (recipe.Kind == DerivedKind::Composite)
		for (auto const& method : HouseMethods)
			if (method.Houses == recipe.Houses)
				ini.SetString(L"Derived", L"Houses", method.Name);
	if (recipe.Kind == DerivedKind::SolarArc) {
		for (auto const& key : Keys)
			if (key.Key == recipe.Key)
				ini.SetString(L"Derived", L"ArcKey", key.Name);
		WriteWhen(ini, L"Derived.Time", recipe.Target, recipe.Zone);
	}
	WriteChart(ini, recipe.A, L"A.");
	if (recipe.Kind != DerivedKind::SolarArc)
		WriteChart(ini, recipe.B, L"B.");

	if (!ini.Save(path)) {
		error = ini.Error();
		return false;
	}
	return true;
}

bool ChartFile::LoadFile(PCWSTR path, Loaded& loaded, std::wstring& error) {
	IniDocument ini;
	if (!ini.Load(path)) {
		error = ini.Error();
		return false;
	}
	Reader r{ ini, error };

	if (!ini.HasSection(L"Derived")) {
		loaded = {};
		return ReadChart(r, L"", loaded.Chart);
	}

	auto version = ini.GetInt(L"Derived", L"Version");
	if (!version)
		return r.Missing(L"Derived", L"Version");
	if (*version > CurrentVersion) {
		error = L"this chart was saved by a newer version of the program (file version " + std::to_wstring(*version) + L")";
		return false;
	}

	DerivedRecipe recipe;
	auto kindName = ini.Get(L"Derived", L"Kind");
	if (!kindName)
		return r.Missing(L"Derived", L"Kind");
	bool found = false;
	for (auto const& kind : Kinds)
		if (Normalize(kind.Name) == Normalize(*kindName)) {
			recipe.Kind = kind.Kind;
			found = true;
		}
	if (!found)
		return r.Problem(L"Derived", L"Kind", L"'" + *kindName + L"' is not a kind of derived chart; use Composite, Davison or SolarArc");

	if (recipe.Kind == DerivedKind::Composite)
		if (auto name = ini.Get(L"Derived", L"Houses")) {
			found = false;
			for (auto const& method : HouseMethods)
				if (Normalize(method.Name) == Normalize(*name)) {
					recipe.Houses = method.Houses;
					found = true;
				}
			if (!found)
				return r.Problem(L"Derived", L"Houses", L"must be MidpointMC or MidpointARMC");
		}
	if (recipe.Kind == DerivedKind::SolarArc) {
		if (auto name = ini.Get(L"Derived", L"ArcKey")) {
			found = false;
			for (auto const& key : Keys)
				if (Normalize(key.Name) == Normalize(*name)) {
					recipe.Key = key.Key;
					found = true;
				}
			if (!found)
				return r.Problem(L"Derived", L"ArcKey", L"must be Actual, Naibod or Ptolemy");
		}
		if (!ReadWhen(r, L"Derived.Time", recipe.Target, recipe.Zone))
			return false;
	}

	if (!ReadChart(r, L"A.", recipe.A))
		return false;
	if (recipe.Kind != DerivedKind::SolarArc && !ReadChart(r, L"B.", recipe.B))
		return false;

	loaded = {};
	loaded.Derived = true;
	loaded.Recipe = std::move(recipe);
	return true;
}

bool ChartFile::Load(PCWSTR path, ChartData& chart, std::wstring& error) {
	Loaded loaded;
	if (!LoadFile(path, loaded, error))
		return false;
	if (loaded.Derived) {
		error = L"this is a chart worked out from others (a composite, a Davison or a solar arc chart), not an ordinary one";
		return false;
	}
	chart = std::move(loaded.Chart);
	return true;
}
