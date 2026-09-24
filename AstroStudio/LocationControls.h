#pragma once

#include "ChartData.h"
#include "NetworkHelper.h"

// Posted to the window that started a "Here" lookup (see CLocationControls::BeginHere).
const UINT WM_HERE_RESULT = WM_APP + 100;
// Posted to the window that started a place lookup (see CLocationControls::BeginLookup).
const UINT WM_LOOKUP_RESULT = WM_APP + 101;

// The latitude/longitude controls (degrees, minutes, hemisphere), the location text and the Here
// button, shared by every window that has them (IDC_LATDEG ... IDC_HERE). The owner keeps the message
// handling and calls into this.
class CLocationControls {
public:
	void Init(CWindow parent);

	// fills the coordinate controls and the location text from the info; while pending the text reads "Locating..."
	void Set(ChartInfo const& info, bool pending = false);
	// reads the coordinates back
	void GetCoordinates(ChartInfo& info) const;

	// True while Set is writing the controls, so the owner can ignore the change notifications that causes.
	bool IsUpdating() const noexcept {
		return m_Updating;
	}

	// City, State, Country - only the parts that are present
	static CString FormatLocation(ChartInfo const& info);

	// Looks up the current location (GPS, else IP) on a worker thread and posts WM_HERE_RESULT to the parent.
	void BeginHere(ChartInfo const& base);
	// Handles WM_HERE_RESULT: re-enables the button and, on success, copies the location into result.
	// Returns false (after telling the user) if the lookup failed.
	bool EndHere(WPARAM success, LPARAM lParam, ChartInfo& result);

	// Looks the text up as a place name (IDC_LOOKUP) on a worker thread and posts WM_LOOKUP_RESULT to the
	// parent. Returns false, with a message, if there is nothing to look up.
	bool BeginLookup(PCWSTR query);
	// Handles WM_LOOKUP_RESULT: re-enables the button, and lets the user pick when there are several matches.
	// Returns true with the chosen place; false if nothing was chosen (no match, failure - both reported - or
	// the menu was dismissed).
	bool EndLookup(WPARAM success, LPARAM lParam, PlaceResult& chosen);

	// Copies a place into the info: coordinates, elevation and the city, state and country names.
	static void ApplyPlace(PlaceResult const& place, ChartInfo& info);

private:
	CWindow m_Parent;
	bool m_Updating{ false };
};
