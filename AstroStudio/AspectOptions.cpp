#include "pch.h"
#include "AspectOptions.h"
#include "AppSettings.h"
#include "Helpers.h"
#include <IniDocument.h>
#include <cmath>

namespace {
	std::wstring Section(PCWSTR set, PCWSTR part = nullptr) {
		std::wstring name = set;
		if (part) {
			name += L'.';
			name += part;
		}
		return name;
	}

	std::wstring Number(double value) {
		return std::format(L"{:g}", value);
	}

	std::string ToUtf8(std::wstring const& text) {
		if (text.empty())
			return {};
		int length = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
		std::string utf8(length, 0);
		::WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), utf8.data(), length, nullptr, nullptr);
		return utf8;
	}

	std::wstring ToWide(std::string_view text) {
		if (text.empty())
			return {};
		int length = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0);
		std::wstring wide(length, 0);
		::MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), wide.data(), length);
		return wide;
	}

	void Write(IniDocument& ini, PCWSTR set, AspectSettings const& s) {
		auto general = Section(set);
		ini.SetDouble(general, L"MajorOrb", s.MajorAspectOrb, 6);
		ini.SetDouble(general, L"MinorOrb", s.MinorAspectOrb, 6);
		ini.SetBool(general, L"MajorOnly", s.MajorOnly);

		auto aspects = Section(set, L"Aspects");
		for (int i = 0; i < AspectSettings::AspectTypeCount; i++) {
			auto type = static_cast<AspectType>(i);
			std::wstring value = !s.AspectEnabled[i] ? L"off" : s.AspectOrb[i] >= 0 ? Number(s.AspectOrb[i]) : L"default";
			ini.SetString(aspects, Helpers::GetAspectName(type), value);
		}

		auto planets = Section(set, L"Planets"), extra = Section(set, L"ExtraOrbs");
		for (int i = 0; i < AspectSettings::PlanetCount; i++) {
			auto planet = static_cast<Planet>(i);
			if (planet == Planet::Earth)
				continue;
			ini.SetString(planets, Helpers::GetPlanetName(planet), s.PlanetEnabled[i] ? L"on" : L"off");
			if (s.PlanetOrbAdd[i] != 0)
				ini.SetString(extra, Helpers::GetPlanetName(planet), Number(s.PlanetOrbAdd[i]));
		}
	}

	// reads a number in 0..MaxOrb; false (with the message) if the key is there but is not one
	bool ReadOrb(IniDocument const& ini, std::wstring const& section, PCWSTR key, float& value, std::wstring& error) {
		auto text = ini.Get(section, key);
		if (!text)
			return true;
		PWSTR end;
		double number = wcstod(text->c_str(), &end);
		if (text->empty() || *end != 0 || !(number >= 0 && number <= AspectOptions::MaxOrb)) {
			error = std::format(L"[{}] {} (line {}): the orb must be a number from 0 to {:g}.", section, key, ini.LineOf(section, key), (double)AspectOptions::MaxOrb);
			return false;
		}
		value = (float)number;
		return true;
	}

	bool Read(IniDocument const& ini, PCWSTR set, AspectSettings& s, std::wstring& error) {
		auto general = Section(set);
		if (!ReadOrb(ini, general, L"MajorOrb", s.MajorAspectOrb, error) || !ReadOrb(ini, general, L"MinorOrb", s.MinorAspectOrb, error))
			return false;
		if (auto only = ini.Get(general, L"MajorOnly")) {
			auto value = ini.GetBool(general, L"MajorOnly");
			if (!value) {
				error = std::format(L"[{}] MajorOnly (line {}): this must be true or false.", general, ini.LineOf(general, L"MajorOnly"));
				return false;
			}
			s.MajorOnly = *value;
		}

		auto aspects = Section(set, L"Aspects");
		for (int i = 0; i < AspectSettings::AspectTypeCount; i++) {
			PCWSTR name = Helpers::GetAspectName(static_cast<AspectType>(i));
			auto text = ini.Get(aspects, name);
			if (!text)
				continue;
			if (_wcsicmp(text->c_str(), L"off") == 0)
				s.AspectEnabled[i] = false;
			else if (_wcsicmp(text->c_str(), L"default") == 0)
				s.AspectOrb[i] = -1;
			else if (!ReadOrb(ini, aspects, name, s.AspectOrb[i], error))
				return false;
		}

		auto planets = Section(set, L"Planets"), extra = Section(set, L"ExtraOrbs");
		for (int i = 0; i < AspectSettings::PlanetCount; i++) {
			PCWSTR name = Helpers::GetPlanetName(static_cast<Planet>(i));
			if (auto text = ini.Get(planets, name)) {
				if (_wcsicmp(text->c_str(), L"on") == 0)
					s.PlanetEnabled[i] = true;
				else if (_wcsicmp(text->c_str(), L"off") == 0)
					s.PlanetEnabled[i] = false;
				else {
					error = std::format(L"[{}] {} (line {}): this must be on or off.", planets, name, ini.LineOf(planets, name));
					return false;
				}
			}
			if (!ReadOrb(ini, extra, name, s.PlanetOrbAdd[i], error))
				return false;
		}
		return true;
	}
}

AspectOptions::AspectOptions() {
	Transit.MajorOnly = true;
	Transit.MajorAspectOrb = 3;
}

std::string AspectOptions::ToText() const {
	IniDocument ini;
	ini.SetHeader(L"Astro Studio aspect settings");
	ini.SetInt(L"Aspects", L"Version", 1);
	Write(ini, L"Chart", Chart);
	Write(ini, L"Transit", Transit);
	return ini.ToString();
}

bool AspectOptions::FromText(std::string_view text, std::wstring& error) {
	IniDocument ini;
	if (!ini.Parse(text)) {
		error = ini.Error();
		return false;
	}
	if (int version = ini.GetInt(L"Aspects", L"Version", 1); version != 1) {
		error = L"These aspect settings are from a newer version of the program.";
		return false;
	}

	AspectOptions loaded;
	if (!Read(ini, L"Chart", loaded.Chart, error) || !Read(ini, L"Transit", loaded.Transit, error))
		return false;
	*this = loaded;
	return true;
}

bool AspectOptions::Save(PCWSTR path, std::wstring& error) const {
	IniDocument ini;
	auto text = ToText();
	if (!ini.Parse(text) || !ini.Save(path)) {
		error = ini.Error();
		return false;
	}
	return true;
}

bool AspectOptions::Load(PCWSTR path, std::wstring& error) {
	IniDocument ini;
	if (!ini.Load(path)) {
		error = ini.Error();
		return false;
	}
	return FromText(ini.ToString(), error);
}

AspectOptions& AspectOptions::Current() {
	static AspectOptions options;
	return options;
}

void AspectOptions::LoadFromSettings() {
	auto text = AppSettings::Get().AspectSets();
	std::wstring error;
	if (!text.empty() && !Current().FromText(ToUtf8(text), error))
		ATLTRACE(L"The saved aspect settings could not be read: %s\n", error.c_str());
}

void AspectOptions::StoreInSettings() {
	AppSettings::Get().AspectSets(ToWide(Current().ToText()));
}
