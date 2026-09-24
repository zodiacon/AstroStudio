#pragma once

#include "ChartData.h"

// A chart on disk: a UTF-8 INI text file (see IniDocument) that is meant to be readable and editable by hand.
//
//   [Chart]      Version, HouseSystem (by name), Harmonic, Planets (names, comma separated)
//   [Person]     FirstName, MiddleName, LastName, Type (Unknown, Male, Female, Event)
//   [Time]       Date (YYYY-MM-DD), Time (HH:MM:SS), TimeZone (a Windows time zone name) and UtOffset (+HH:MM)
//   [Location]   City, State, Country, Latitude, Longitude (decimal degrees, north/east positive), Elevation
//
// The time is the local time in the time zone, as it is entered and shown in the program; UT is worked out
// on loading. UtOffset is always written: it is used when TimeZone is empty (a fixed offset) or not known on
// the computer, and it settles which of the two identical local times of a DST change is meant.
// Only what is needed to recreate the chart is stored - positions are always recalculated.
struct ChartFile abstract final {
	static constexpr PCWSTR Extension = L"chart";
	// for the Open and Save dialogs
	static constexpr wchar_t Filter[] = L"Astro Studio charts (*.chart)\0*.chart\0All files (*.*)\0*.*\0";

	// On failure they return false and say why in error (a message fit to show the user).
	static bool Save(ChartData const& chart, PCWSTR path, std::wstring& error);
	static bool Load(PCWSTR path, ChartData& chart, std::wstring& error);
};
