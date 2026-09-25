#pragma once

#include "ChartData.h"
#include "DerivedCharts.h"

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
//
// A chart worked out from others (a composite, a Davison chart, a solar arc chart) is saved as its *recipe*: the charts it was
// made from, each in the four sections above under a prefix, and what was chosen. Loading rebuilds the chart from them.
//
//   [Derived]        Version, Kind (Composite, Davison or SolarArc), Houses (MidpointMC or MidpointARMC: composite),
//                    ArcKey (Actual, Naibod or Ptolemy: solar arc)
//   [Derived.Time]   solar arc: the date it is moved to, like [Time]
//   [A.Chart] [A.Person] [A.Time] [A.Location]     the first chart (the only one for a solar arc chart)
//   [B.Chart] [B.Person] [B.Time] [B.Location]     the second chart (composite and Davison)
struct ChartFile abstract final {
	static constexpr PCWSTR Extension = L"chart";
	// for the Open and Save dialogs
	static constexpr wchar_t Filter[] = L"Astro Studio charts (*.chart)\0*.chart\0All files (*.*)\0*.*\0";

	// what a file holds
	struct Loaded {
		bool Derived{ false };
		ChartData Chart;			// a plain chart
		DerivedRecipe Recipe;		// a derived one: build it with DerivedCharts::Build
	};

	// On failure they return false and say why in error (a message fit to show the user).
	static bool Save(ChartData const& chart, PCWSTR path, std::wstring& error);
	static bool SaveDerived(DerivedRecipe const& recipe, PCWSTR path, std::wstring& error);
	// a plain chart or a derived one
	static bool LoadFile(PCWSTR path, Loaded& loaded, std::wstring& error);
	// a plain chart only: a derived chart's file is an error here
	static bool Load(PCWSTR path, ChartData& chart, std::wstring& error);
};
