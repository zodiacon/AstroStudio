#pragma once

struct ChartInfo;

// One match of a place search
struct PlaceResult {
	std::wstring Name, State, Country;
	std::wstring TimeZone;		// IANA id, e.g. "America/New_York"
	double Latitude{ 0 }, Longitude{ 0 }, Elevation{ 0 };
	long long Population{ 0 };
};

struct NetworkHelper abstract final {
	static std::string GetExternalIP();
	static bool FillInfoFromLocal(ChartInfo& info);

	// Searches for a place by name (Open-Meteo geocoding, GeoNames data), best matches first. Blocks; call from
	// a worker thread. Returns false if the service couldn't be reached; true with no results means no match.
	static bool SearchPlaces(std::wstring const& query, std::vector<PlaceResult>& results);
	static bool FillInfoFromCurrentLocation(ChartInfo& info, HWND hParent = nullptr, DWORD gpsTimeoutMs = 5000);

private:
	static bool FillInfoFromGPS(ChartInfo& info, HWND hParent, DWORD timeoutMs);
};

