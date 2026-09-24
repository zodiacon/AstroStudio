#include "pch.h"
#include "Helpers.h"
#include "StringHelper.h"
#include "DateTime.h"
#include "Aspects.h"
#include <cmath>

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
	bool showSeconds = (options & FormatOptions::ShowSeconds) == FormatOptions::ShowSeconds;
	bool useGlyphs = (options & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs;
	bool showDegreeGlyph = (options & FormatOptions::ShowDegreeGlyph) == FormatOptions::ShowDegreeGlyph;

	//
	// Round the longitude once, at the precision actually being displayed, and
	// split the rounded result into components.
	//
	// Rounding each component on its own let the carry escape: without seconds,
	// a position like 11.9955 degrees into a sign has Minutes() == 59.73, and
	// "(int)(Minutes() + .5)" printed that as "60'" instead of carrying into
	// the degree. The same applied to Seconds() when seconds were shown.
	//
	// Doing the arithmetic in whole display units also keeps the split exact.
	// Deriving each component by multiplying and truncating doubles (as
	// AstroPoint::Minutes/Seconds do) can land a hair under the true value and
	// lose a whole unit.
	//
	const long long unitsPerDegree = showSeconds ? 3600 : 60;
	const long long unitsPerSign = 30 * unitsPerDegree;
	const long long unitsPerCircle = 12 * unitsPerSign;

	auto units = std::llround(value.Value * unitsPerDegree);
	units = ((units % unitsPerCircle) + unitsPerCircle) % unitsPerCircle;

	//
	// The sign is taken from the rounded value, so a position within half a
	// display unit of a cusp reads as the next sign - 29 Pisces 59'40" shows as
	// 00 Aries 00' when seconds are hidden. That is what rounding means; to
	// keep it in the sign it actually occupies, floor instead of rounding above.
	//
	auto sign = static_cast<ZodiacSign>(units / unitsPerSign);
	auto inSign = units % unitsPerSign;

	CString text;
	text.Format(L"%02d%s %s %02d%s",
		(int)(inSign / unitsPerDegree),
		showDegreeGlyph ? (PCWSTR)CString((WCHAR)(useGlyphs ? 59 : 0xb0)) : L"",
		useGlyphs ? (PCWSTR)font.GetSignGlyphAsString(sign) : (PCWSTR)GetZodiacSignName(sign).Left(3),
		(int)(showSeconds ? (inSign / 60) % 60 : inSign % 60),
		showDegreeGlyph ? (PCWSTR)CString((WCHAR)39) : L"");

	if (showSeconds) {
		CString sec;
		sec.Format(L"%02d\"", (int)(inSign % 60));
		text += sec;
	}
	if ((value.Flags & AstroPointFlags::Retro) == AstroPointFlags::Retro) {
		text += useGlyphs ? L">" : L"R";
	}
	return text;
}

CString Helpers::FormatLatitude(double lat) {
	CString text;
	text.Format(L"%d%c %02d' %c", int(abs(lat)), 0xb0, int(60 * (abs(lat) - int(abs(lat)))), lat < 0 ? 'S' : 'N');
	return text;
}

std::tuple<int, int, int> Helpers::GetDegMinSec(double angle, bool sign) {
	// Whole minutes, truncated. The tolerance keeps a value that was built from degrees and minutes
	// (40 + 53/60) from coming back as 52 because it lands a hair below 53 in floating point.
	int totalMinutes = (int)std::floor(std::abs(angle) * 60 + 1e-6);
	int deg = totalMinutes / 60, min = totalMinutes % 60;
	if (sign && angle < 0)
		return { -deg, -min, 0 };
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
		L"Bi-Septile", L"Quincunx", L"Sesquiquadrate", L"Novile", L"Bi-Novile",
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
		planets.reserve(16);
		for (Planet type = Planet::Sun; type <= Planet::Pluto; ((int&)type)++)
			planets.push_back(type);
	}
	return planets;
}

std::vector<HouseSystem> const& Helpers::HouseSystems() {
	static const std::vector<HouseSystem> systems = {
		HouseSystem::Placidus,
		HouseSystem::Koch,
		HouseSystem::Porphyrius,
		HouseSystem::Regiomontanus,
		HouseSystem::Campanus,
		HouseSystem::Equal,
		HouseSystem::Morinus,
		HouseSystem::Topocentric,
		HouseSystem::Alcabitus,
		HouseSystem::Horizontal,
		HouseSystem::Krusinski,
		HouseSystem::EqualWholeSign,
		HouseSystem::CarterPoliEqu,
		HouseSystem::EqualMC,
		HouseSystem::Sunshine,
		HouseSystem::SunshineAlt,
		HouseSystem::APCHouses,
	};
	return systems;
}

void Helpers::FillHouseSystems(CComboBox combo) {
	for (auto system : HouseSystems()) {
		int n = combo.AddString(StringHelper::HouseSystemToString(system));
		combo.SetItemData(n, (int)system);
	}
}

ChartData Helpers::CreateChartData(ChartInfo info, HouseSystem houseSystem) {
	ChartData data;
	data.Info() = std::move(info);
	data.SetHouseSystem(houseSystem);
	data.AddPlanets(GetStandardPlanets());
	data.AddPlanets({ Planet::Chiron, Planet::TrueNode, Planet::Lilith });
	return data;
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

bool Helpers::CopyTextToClipboard(HWND owner, PCWSTR text) {
	auto bytes = (wcslen(text) + 1) * sizeof(wchar_t);
	HGLOBAL mem = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
	if (!mem)
		return false;
	memcpy(::GlobalLock(mem), text, bytes);
	::GlobalUnlock(mem);
	if (!::OpenClipboard(owner)) {
		::GlobalFree(mem);
		return false;
	}
	::EmptyClipboard();
	bool ok = ::SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
	::CloseClipboard();
	if (!ok)
		::GlobalFree(mem);		// the clipboard owns it only if it took it
	return ok;
}
