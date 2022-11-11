#pragma once

#include "AstroPoint.h"
#include "DefaultFont.h"
class DateTime;

enum class FormatOptions {
	None = 0,
	UseGlyphs = 2,
	ShowDegreeGlyph = 4,
	ShowSeconds = ShowDegreeGlyph | 1,
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
	static PCWSTR GetPlanetName(PlanetType type);
	static CString GetZodiacSignName(ZodiacSign sign);
	static std::vector<PlanetType> GetStandardPlanets();
	static COLORREF Darken(COLORREF color, int offset);
	static COLORREF Lighten(COLORREF color, int offset);
};

