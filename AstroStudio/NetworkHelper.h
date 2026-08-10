#pragma once

struct ChartInfo;

struct NetworkHelper abstract final {
	static std::string GetExternalIP();
	static bool FillInfoFromLocal(ChartInfo& info);
	static bool FillInfoFromCurrentLocation(ChartInfo& info, HWND hParent = nullptr, DWORD gpsTimeoutMs = 5000);

private:
	static bool FillInfoFromGPS(ChartInfo& info, HWND hParent, DWORD timeoutMs);
};

