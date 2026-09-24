#pragma once

#include "AstroPoint.h"
#include "DefaultFont.h"
#include "ChartData.h"
class DateTime;
enum class AspectType;

enum class FormatOptions {
	None = 0,
	ShowSeconds = 1,
	UseGlyphs = 2,
	ShowDegreeGlyph = 4,
	Default = ShowDegreeGlyph | UseGlyphs,
};
DEFINE_ENUM_FLAG_OPERATORS(FormatOptions);

enum class DateTimeFormatOptions {
	None = 0,
	DateOnly = 1,
	TimeOnly = 2,
	Default = DateOnly,
};
DEFINE_ENUM_FLAG_OPERATORS(DateTimeFormatOptions);

struct Helpers abstract final {
	static bool LoadAstroFont(UINT id);
	static CString FormatDateTime(DateTime const& dt, DateTimeFormatOptions options = DateTimeFormatOptions::Default);
	static CString FormatLongitude(AstroPoint const& longitude, FormatOptions options = FormatOptions::Default, AstroFontBase const& font = DefaultFont::Get());
	static CString FormatLatitude(double lat);
	static std::tuple<int, int, int> GetDegMinSec(double angle, bool sign = false);
	static PCWSTR GetPlanetName(Planet type);
	static PCWSTR GetAspectName(AspectType type);
	static CString GetZodiacSignName(ZodiacSign sign);
	static std::vector<Planet> GetStandardPlanets();
	// a chart for the info with the usual planet set (the standard planets plus Chiron, True Node and Lilith)
	// fills a house system drop-down list; the item data of each entry is the HouseSystem
	static void FillHouseSystems(CComboBox combo);
	static ChartData CreateChartData(ChartInfo info, HouseSystem houseSystem = HouseSystem::Koch);
	static COLORREF Darken(COLORREF color, int offset);
	static COLORREF Lighten(COLORREF color, int offset);
};

