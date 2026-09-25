#include "pch.h"
#include "ChartColors.h"
#include "AppSettings.h"
#include <IniDocument.h>
#include <format>
#include <sstream>

namespace {
	D2D1_COLOR_F& Member(ChartDrawingParameters& p, ChartColor color) {
		switch (color) {
			case ChartColor::Background: return p.BackColor;
			case ChartColor::Fire: return p.ElementColor[0];
			case ChartColor::Earth: return p.ElementColor[1];
			case ChartColor::Air: return p.ElementColor[2];
			case ChartColor::Water: return p.ElementColor[3];
			case ChartColor::SoftAspect: return p.SoftAspectColor;
			case ChartColor::HardAspect: return p.HardAspectColor;
			case ChartColor::MinorAspect: return p.MinorAspectColor;
			case ChartColor::Text: return p.TextColor;
			case ChartColor::Grid: return p.GridColor;
			case ChartColor::Dot: return p.DotColor;
			case ChartColor::OverlayBand: return p.OverlayBandColor;
			default: return p.OverlayColor;
		}
	}

	D2D1_COLOR_F ToD2D(COLORREF color) {
		return ColorFromRgb(GetRValue(color), GetGValue(color), GetBValue(color));
	}

	COLORREF ToColorRef(D2D1_COLOR_F const& color) {
		auto channel = [](float value) { return static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255)); };
		return RGB(channel(color.r), channel(color.g), channel(color.b));
	}

	std::wstring Hex(COLORREF color) {
		return std::format(L"{:02x}{:02x}{:02x}", GetRValue(color), GetGValue(color), GetBValue(color));
	}

	std::wstring List(ChartColors::Set const& set) {
		std::wstring text;
		for (size_t i = 0; i < set.size(); i++)
			if (set[i])
				text += std::format(L"{}{}:{}", text.empty() ? L"" : L",", i, Hex(*set[i]));
		return text;
	}

	void ReadList(std::wstring const& text, ChartColors::Set& set) {
		set = {};
		std::wstringstream stream(text);
		std::wstring item;
		while (std::getline(stream, item, L',')) {
			auto colon = item.find(L':');
			if (colon == std::wstring::npos)
				continue;
			wchar_t* end;
			auto index = wcstol(item.substr(0, colon).c_str(), &end, 10);
			auto digits = item.substr(colon + 1);
			auto rgb = wcstoul(digits.c_str(), &end, 16);
			if (*end == 0 && digits.size() == 6 && index >= 0 && index < static_cast<long>(set.size()))
				set[index] = RGB((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
		}
	}
}

namespace {
	void WriteSet(IniDocument& ini, PCWSTR section, ChartColors::Set const& set) {
		for (size_t i = 0; i < set.size(); i++) {
			std::wstring value = L"default";
			if (set[i])
				value = L"#" + Hex(*set[i]);
			ini.SetString(section, ChartColors::Key(static_cast<ChartColor>(i)), value);
		}
	}

	// Reads a section into a set; false, with the error, if a key isn't an element or a value isn't a colour.
	bool ReadSet(IniDocument const& ini, PCWSTR section, ChartColors::Set& set, std::wstring& error) {
		ChartColors::Set result;
		for (auto const& key : ini.Keys(section)) {
			size_t index = 0;
			while (index < result.size() && _wcsicmp(key.c_str(), ChartColors::Key(static_cast<ChartColor>(index))) != 0)
				index++;
			auto where = std::format(L"[{}] {} (line {}): ", section, key, ini.LineOf(section, key));
			if (index == result.size()) {
				error = where + L"there is no such element.";
				return false;
			}
			auto value = ini.GetString(section, key);
			if (_wcsicmp(value.c_str(), L"default") == 0)
				continue;
			auto digits = !value.empty() && value[0] == L'#' ? value.substr(1) : value;
			wchar_t* end = nullptr;
			auto rgb = wcstoul(digits.c_str(), &end, 16);
			if (digits.size() != 6 || *end != 0 || digits.find_first_of(L"+- ") != std::wstring::npos) {
				error = where + L"\"" + value + L"\" is not a colour (write #RRGGBB, or default).";
				return false;
			}
			result[index] = RGB((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
		}
		set = result;
		return true;
	}

	bool Read(IniDocument const& ini, ChartColors& colors, std::wstring& error) {
		if (int version = ini.GetInt(L"Colors", L"Version", 1); version != 1) {
			error = L"These colours are from a newer version of the program.";
			return false;
		}
		ChartColors loaded = colors;
		if (ini.HasSection(L"Light") && !ReadSet(ini, L"Light", loaded.Light, error))
			return false;
		if (ini.HasSection(L"Dark") && !ReadSet(ini, L"Dark", loaded.Dark, error))
			return false;
		colors = loaded;
		return true;
	}

	void Write(IniDocument& ini, ChartColors const& colors) {
		ini.SetHeader(L"Astro Studio chart colours");
		ini.SetInt(L"Colors", L"Version", 1);
		WriteSet(ini, L"Light", colors.Light);
		WriteSet(ini, L"Dark", colors.Dark);
	}
}

PCWSTR ChartColors::Key(ChartColor color) {
	switch (color) {
		case ChartColor::Background: return L"Background";
		case ChartColor::Fire: return L"Fire";
		case ChartColor::Earth: return L"Earth";
		case ChartColor::Air: return L"Air";
		case ChartColor::Water: return L"Water";
		case ChartColor::SoftAspect: return L"SoftAspects";
		case ChartColor::HardAspect: return L"HardAspects";
		case ChartColor::MinorAspect: return L"OtherAspects";
		case ChartColor::Text: return L"Text";
		case ChartColor::Grid: return L"HouseLines";
		case ChartColor::Dot: return L"PlanetDots";
		case ChartColor::OverlayBand: return L"OverlayBand";
		default: return L"OverlayPlanets";
	}
}

std::string ChartColors::ToIni() const {
	IniDocument ini;
	Write(ini, *this);
	return ini.ToString();
}

bool ChartColors::FromIni(std::string_view text, std::wstring& error) {
	IniDocument ini;
	if (!ini.Parse(text)) {
		error = ini.Error();
		return false;
	}
	return Read(ini, *this, error);
}

bool ChartColors::Save(PCWSTR path, std::wstring& error) const {
	IniDocument ini;
	Write(ini, *this);
	if (!ini.Save(path)) {
		error = ini.Error();
		return false;
	}
	return true;
}

bool ChartColors::Load(PCWSTR path, std::wstring& error) {
	IniDocument ini;
	if (!ini.Load(path)) {
		error = ini.Error();
		return false;
	}
	return Read(ini, *this, error);
}

PCWSTR ChartColors::Name(ChartColor color) {
	switch (color) {
		case ChartColor::Background: return L"Background";
		case ChartColor::Fire: return L"Fire signs";
		case ChartColor::Earth: return L"Earth signs";
		case ChartColor::Air: return L"Air signs";
		case ChartColor::Water: return L"Water signs";
		case ChartColor::SoftAspect: return L"Soft aspects (sextile, trine)";
		case ChartColor::HardAspect: return L"Hard aspects (square, opposition)";
		case ChartColor::MinorAspect: return L"Other aspects";
		case ChartColor::Text: return L"Glyphs, outlines and angles";
		case ChartColor::Grid: return L"House lines";
		case ChartColor::Dot: return L"Planet dots";
		case ChartColor::OverlayBand: return L"Overlay band";
		default: return L"Overlay planets and caption";
	}
}

COLORREF ChartColors::Default(ChartColor color, bool dark) {
	auto params = dark ? ChartDrawingParameters::Dark() : ChartDrawingParameters();
	return ToColorRef(Member(params, color));
}

void ChartColors::Apply(ChartDrawingParameters& params, bool dark) const {
	auto const& set = For(dark);
	for (size_t i = 0; i < set.size(); i++)
		if (set[i])
			Member(params, static_cast<ChartColor>(i)) = ToD2D(*set[i]);
}

std::wstring ChartColors::ToText() const {
	return std::format(L"light={};dark={}", List(Light), List(Dark));
}

void ChartColors::FromText(std::wstring const& text) {
	std::wstringstream stream(text);
	std::wstring part;
	while (std::getline(stream, part, L';')) {
		auto equals = part.find(L'=');
		if (equals == std::wstring::npos)
			continue;
		auto key = part.substr(0, equals), value = part.substr(equals + 1);
		if (key == L"light")
			ReadList(value, Light);
		else if (key == L"dark")
			ReadList(value, Dark);
	}
}

ChartColors& ChartColors::Current() {
	static ChartColors colors;
	return colors;
}

void ChartColors::LoadFromSettings() {
	Current().FromText(AppSettings::Get().ChartColors());
}

void ChartColors::StoreInSettings() {
	AppSettings::Get().ChartColors(Current().ToText());
}
