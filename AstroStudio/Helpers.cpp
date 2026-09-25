#include "pch.h"
#include "Helpers.h"
#include "StringHelper.h"
#include "DateTime.h"
#include "Aspects.h"
#include "AppSettings.h"
#include <cmath>
#include <CommCtrl.h>

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
	text.TrimRight();		// the parts are separated by a space, which the last one doesn't need
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

void Helpers::SetColumnWidth(CListViewCtrl& list, int column, int width) {
	LVCOLUMN info{ LVCF_FMT };
	if (!list.GetColumn(column, &info))
		return;
	int format = info.fmt;
	info.fmt &= ~LVCFMT_FIXED_WIDTH;
	list.SetColumn(column, &info);
	list.SetColumnWidth(column, width);
	info.fmt = format;
	list.SetColumn(column, &info);
}

bool Helpers::UserTextFont(CFont& font, int deciPoints, int* size) {
	LOGFONT lf = AppSettings::Get().TextFont();
	if (lf.lfFaceName[0] == 0)
		return false;
	if (deciPoints <= 0)
		deciPoints = lf.lfHeight > 0 ? lf.lfHeight : 90;
	lf.lfHeight = deciPoints;
	if (font)
		font.DeleteObject();
	font.CreatePointFontIndirect(&lf);
	if (size)
		*size = deciPoints;
	return font != nullptr;
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

bool Helpers::SaveTextFileUtf8(HWND owner, PCWSTR path, CString const& text, PCWSTR what) {
	int length = ::WideCharToMultiByte(CP_UTF8, 0, text, text.GetLength(), nullptr, 0, nullptr, nullptr);
	std::string bytes("\xEF\xBB\xBF");
	bytes.resize(3 + length);
	::WideCharToMultiByte(CP_UTF8, 0, text, text.GetLength(), bytes.data() + 3, length, nullptr, nullptr);

	DWORD error = ERROR_SUCCESS;
	HANDLE file = ::CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		error = ::GetLastError();
	else {
		DWORD written = 0;
		if (!::WriteFile(file, bytes.data(), (DWORD)bytes.size(), &written, nullptr))
			error = ::GetLastError();
		::CloseHandle(file);
	}
	if (error == ERROR_SUCCESS)
		return true;
	WCHAR reason[256]{};
	::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0, reason, _countof(reason), nullptr);
	CString message;
	message.Format(L"%s could not be saved to %s:\n\n%s", what, path, reason);
	AtlMessageBox(owner, (PCWSTR)message, L"Astro Studio", MB_ICONWARNING);
	return false;
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

CString Helpers::TableText(TableSource const& table, std::vector<int> const& rows, bool csv) {
	auto quote = [&](CString text) {
		if (!csv)
			return text;
		text.Replace(L"\"", L"\"\"");
		return CString(L"\"") + text + L"\"";
	};
	PCWSTR separator = csv ? L"," : L"\t";
	CString text;
	for (int column = 0; column < static_cast<int>(table.Headers.size()); column++) {
		if (column)
			text += separator;
		text += quote(table.Headers[column]);
	}
	text += L"\r\n";
	for (int row : rows) {
		for (int column = 0; column < static_cast<int>(table.Headers.size()); column++) {
			if (column)
				text += separator;
			text += quote(table.Cell(row, column));
		}
		text += L"\r\n";
	}
	return text;
}

bool Helpers::CopyListRows(HWND owner, HWND list, TableSource const& table) {
	std::vector<int> rows;
	for (int row = ListView_GetNextItem(list, -1, LVNI_SELECTED); row >= 0; row = ListView_GetNextItem(list, row, LVNI_SELECTED))
		if (row < table.Rows)
			rows.push_back(row);
	if (rows.empty())
		return false;
	if (!CopyTextToClipboard(owner, TableText(table, rows, false)))
		AtlMessageBox(owner, L"The rows could not be copied to the clipboard.", L"Astro Studio", MB_ICONWARNING);
	return true;
}

bool Helpers::SaveTable(HWND owner, TableSource const& table, PCWSTR path, PCWSTR what) {
	std::vector<int> rows(table.Rows);
	for (int i = 0; i < table.Rows; i++)
		rows[i] = i;
	return SaveTextFileUtf8(owner, path, TableText(table, rows, true), what);
}
