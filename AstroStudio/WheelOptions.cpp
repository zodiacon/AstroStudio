#include "pch.h"
#include "WheelOptions.h"
#include "AppSettings.h"
#include <format>
#include <sstream>

namespace {
	template<size_t N>
	std::wstring List(std::bitset<N> const& bits) {
		std::wstring text;
		for (size_t i = 0; i < N; i++)
			if (bits[i])
				text += (text.empty() ? L"" : L",") + std::to_wstring(i);
		return text;
	}

	template<size_t N>
	void ReadList(std::wstring const& text, std::bitset<N>& bits) {
		bits.reset();
		std::wstringstream stream(text);
		std::wstring item;
		while (std::getline(stream, item, L',')) {
			wchar_t* end;
			auto value = wcstol(item.c_str(), &end, 10);
			if (end != item.c_str() && *end == 0 && value >= 0 && value < static_cast<long>(N))
				bits[value] = true;
		}
	}
}

std::wstring WheelOptions::ToText() const {
	return std::format(L"planets={};aspects={};beyond={}", List(HiddenPlanets), List(HiddenAspects), BeyondPluto ? 1 : 0);
}

void WheelOptions::FromText(std::wstring const& text) {
	std::wstringstream stream(text);
	std::wstring part;
	while (std::getline(stream, part, L';')) {
		auto equals = part.find(L'=');
		if (equals == std::wstring::npos)
			continue;
		auto key = part.substr(0, equals), value = part.substr(equals + 1);
		if (key == L"planets")
			ReadList(value, HiddenPlanets);
		else if (key == L"aspects")
			ReadList(value, HiddenAspects);
		else if (key == L"beyond")
			BeyondPluto = value == L"1";
	}
}

WheelOptions& WheelOptions::Current() {
	static WheelOptions options;
	return options;
}

void WheelOptions::LoadFromSettings() {
	Current().FromText(AppSettings::Get().WheelOptions());
}

void WheelOptions::StoreInSettings() {
	AppSettings::Get().WheelOptions(Current().ToText());
}
