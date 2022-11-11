#pragma once

struct ChartInfo;

struct NetworkHelper abstract final {
	static std::string GetExternalIP();
	static bool FillInfoFromLocal(ChartInfo& info);
};

