#include "pch.h"
#include "Helpers.h"
#include "DateTime.h"
#include "Aspects.h"

bool Helpers::LoadAstroFont(UINT id) {
	auto res = ::FindResource(nullptr, MAKEINTRESOURCE(id), L"TTF");
	ATLASSERT(res);
	auto hGlobal = ::LoadResource(nullptr, res);
	ATLASSERT(hGlobal);
	auto size = ::SizeofResource(nullptr, res);
	auto p = ::LockResource(hGlobal);
	DWORD count = 0;
	auto handle = ::AddFontMemResourceEx(p, size, nullptr, &count);
	ATLASSERT(handle);
	GetAstroFontFamily(id);
	return true;
}

Gdiplus::FontFamily const& Helpers::GetAstroFontFamily(UINT id) {
	static Gdiplus::PrivateFontCollection collection;
	static Gdiplus::FontFamily family;
	static bool loaded = false;
	if (!loaded) {
		loaded = true;
		auto res = ::FindResource(nullptr, MAKEINTRESOURCE(id), L"TTF");
		ATLASSERT(res);
		auto hGlobal = ::LoadResource(nullptr, res);
		ATLASSERT(hGlobal);
		auto size = ::SizeofResource(nullptr, res);
		auto p = ::LockResource(hGlobal);
		ATLVERIFY(Gdiplus::Ok == collection.AddMemoryFont(p, size));
		INT found = 0;
		ATLVERIFY(Gdiplus::Ok == collection.GetFamilies(1, &family, &found));
		ATLASSERT(found == 1);
	}
	return family;
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
	bool showSeconds = (options & FormatOptions::ShowSeconds) == FormatOptions::ShowSeconds;
	text.Format(L"%02d%s %s %02d%s",
		(int)value.DegreeInSign(),
		(options & FormatOptions::ShowDegreeGlyph) == FormatOptions::ShowDegreeGlyph ? (PCWSTR)CString((WCHAR)
			((options & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs ? 59 : 0xb0)) : L"",
		(options & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs ?
		(PCWSTR)font.GetSignGlyphAsString(value.Sign()) : (PCWSTR)GetZodiacSignName(value.Sign()).Left(3),
		(int)(value.Minutes() + (showSeconds ? 0 : .5)),
		(options & FormatOptions::ShowDegreeGlyph) == FormatOptions::ShowDegreeGlyph ? (PCWSTR)CString((WCHAR)39) : L"");
	if (showSeconds) {
		CString sec;
		sec.Format(L"%02d\"", int(value.Seconds() + .5));
		text += sec;
	}
	if ((value.Flags & AstroPointFlags::Retro) == AstroPointFlags::Retro) {
		text += (options & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs ? L">" : L"R";
	}
	return text;
}

CString Helpers::FormatLatitude(double lat) {
	CString text;
	text.Format(L"%d%c %02d' %c", int(abs(lat)), 0xb0, int(60 * (abs(lat) - int(abs(lat)))), lat < 0 ? 'S' : 'N');
	return text;
}

std::tuple<int, int, int> Helpers::GetDegMinSec(double angle, bool sign) {
	if (!sign)
		angle = abs(angle);
	int deg = (int)angle;
	int min = int((angle - deg) * 60);
	return { deg, min, 0 };
}

PCWSTR Helpers::GetPlanetName(Planet type) {
	static PCWSTR names[] = {
		L"Sun", L"Moon", L"Mercury", L"Venus", L"Mars", L"Jupiter", L"Saturn", L"Uranus", L"Neptune", L"Pluto",
		L"Mean Node", L"True Node", L"Lilith", L"True Lilith", L"Earth", L"Chiron", L"Pholus",
		L"Ceres", L"Pallas", L"Juno", L"Vesta",
	};
	ATLASSERT((int)type < _countof(names));
	return names[(int)type];
}

PCWSTR Helpers::GetAspectName(AspectType type) {
	static PCWSTR names[] = {
		L"Conjunction", L"Sextile", L"Square", L"Trine", L"Opposition",
		L"Semi-Sextile", L"Semi-Square", L"Quintile", L"Bi-Quintile", L"Septile", 
		L"Bi-Septile", L"Quincunx", L"Sesqui-Quadrate", L"Novile", L"Bi-Novile",
	};
	ATLASSERT((int)type >= 0 && (int)type < _countof(names));
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

std::vector<Planet> Helpers::GetStandardPlanets() {
	static std::vector<Planet> planets;
	if (planets.empty()) {
		planets.reserve(10);
		for (Planet type = Planet::Sun; type <= Planet::Pluto; ((int&)type)++)
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
