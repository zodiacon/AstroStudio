#include "pch.h"
#include "Helpers.h"
#include "DateTime.h"

bool Helpers::LoadAstroFont(UINT id) {
	auto res = ::FindResource(nullptr, MAKEINTRESOURCE(id), L"TTF");
	ATLASSERT(res);
	auto hGlobal = ::LoadResource(nullptr, res);
	ATLASSERT(hGlobal);
	auto size = ::SizeofResource(nullptr, res);
	auto p = ::LockResource(hGlobal);
	DWORD count;
	auto handle = ::AddFontMemResourceEx(p, size, nullptr, &count);
	ATLASSERT(handle);
	return true;
}

CString Helpers::FormatDateTime(DateTime const& dt, DateTimeFormatOptions options) {
	CString text;
	if ((options & DateTimeFormatOptions::TimeOnly) == DateTimeFormatOptions::None) {
		CString date;
		date.Format(L"%5d/%02d/%02d ", dt.Year(), dt.Month(), dt.Day());
		text += date;
	}
	if ((options & DateTimeFormatOptions::DateOnly) == DateTimeFormatOptions::None) {
		CString time;
		time.Format(L"%02d.%02d ", dt.Hour(), dt.Minute());
		text += time;
	}
	return text;
}

CString Helpers::FormatLongitude(AstroPoint const& value, FormatOptions options, AstroFontBase const& font) {
	CString text;
	text.Format(L"%02d%s %s %02d%s",
		(int)value.DegreeInSign(),
		(options & FormatOptions::ShowDegreeGlyph) == FormatOptions::ShowDegreeGlyph ? (PCWSTR)CString((WCHAR)
			((options & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs ? 59 : 0xb0)) : L"",
		(options & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs ?
		(PCWSTR)font.GetSignGlyphAsString(value.Sign()) : (PCWSTR)GetZodiacSignName(value.Sign()).Left(3),
		(int)(value.Minutes() + .5),
		(options & FormatOptions::ShowDegreeGlyph) == FormatOptions::ShowDegreeGlyph ? (PCWSTR)CString((WCHAR)39) : L"");
	if ((options & FormatOptions::ShowSeconds) == FormatOptions::ShowSeconds) {
		CString sec;
		sec.Format(L"%02d", int(value.Seconds() + .5));
		text += sec + L"\"";
	}
	if ((value.Flags & AstroPointFlags::Retro) == AstroPointFlags::Retro) {
		text += (options & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs ? L">" : L"R";
	}
	return text;
}
PCWSTR Helpers::GetPlanetName(PlanetType type) {
	static PCWSTR names[] = {
		L"Sun", L"Moon", L"Mercury", L"Venus", L"Mars", L"Jupiter", L"Saturn", L"Uranus", L"Neptune", L"Pluto",
		L"Mean Node", L"True Node", L"Lilith", L"True Lilith", L"Earth", L"Chiron", L"Pholus",
		L"Ceres", L"Pallas", L"Juno", L"Vesta",
	};
	ATLASSERT((int)type < _countof(names));
	return names[(int)type];
}

CString Helpers::GetZodiacSignName(ZodiacSign sign) {
	static PCWSTR signs[] = {
		L"Aries",
		L"Taurus",
		L"Gemini",
		L"Cancer",
		L"Leo",
		L"Virgo",
		L"Libra",
		L"Scorpio",
		L"Sagittarius",
		L"Capricorn",
		L"Aquarius",
		L"Pisces"
	};
	return signs[(int)sign];
}

std::vector<PlanetType> Helpers::GetStandardPlanets() {
	static std::vector<PlanetType> planets;
	if (planets.empty()) {
		planets.reserve(10);
		for (PlanetType type = PlanetType::Sun; type <= PlanetType::Pluto; ((int&)type)++)
			planets.push_back(type);
	}
	return planets;
}

COLORREF Helpers::Darken(COLORREF color, int offset) {
	auto r = GetRValue(color), g = GetGValue(color), b = GetBValue(color);
	r = std::max(0, r - offset);
	g = std::max(0, g - offset);
	b = std::max(0, b - offset);

	return RGB(r, g, b);
}

COLORREF Helpers::Lighten(COLORREF color, int offset) {
	auto r = GetRValue(color), g = GetGValue(color), b = GetBValue(color);
	r = std::min(255, r + offset);
	g = std::min(255, g + offset);
	b = std::min(255, b + offset);

	return RGB(r, g, b);
}
