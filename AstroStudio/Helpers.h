#pragma once

#include "AstroPoint.h"

class DateTime;

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
	static CString FormatLongitude(AstroPoint const& longitude, FormatOptions options = FormatOptions::Default);
	static WCHAR GetSignGlyph(ZodiacSign sign);
	static CString GetSignGlyphAsString(ZodiacSign sign);
	static WCHAR GetPlanetGlyph(PlanetType planet);
	static CString GetPlanetGlyphAsString(PlanetType planet);
	static PCWSTR GetPlanetName(PlanetType type);
	static CString GetZodiacSignName(ZodiacSign sign);
	static std::vector<PlanetType> GetStandardPlanets();
	static CString GetRetroGlyphAsString();
	static CString GetDirectGlyphAsString();
	static WCHAR GetDirectGlyph();
	static WCHAR GetRetroGlyph();
	static COLORREF Darken(COLORREF color, int offset);
};

