#include "pch.h"
#include "MidpointSettings.h"
#include "AppSettings.h"
#include "DefaultFont.h"
#include <format>
#include <sstream>
#include <cmath>

int MidpointSettings::NearestOrb(int hundredths) {
	int best = 0;
	for (int i = 1; i < _countof(Orbs); i++)
		if (std::abs(Orbs[i] - hundredths) < std::abs(Orbs[best] - hundredths))
			best = i;
	return best;
}

MidpointOptions MidpointSettings::Points(bool angles) const {
	MidpointOptions options;
	options.Angles = angles && (Ascendant || Midheaven);
	options.Ascendant = Ascendant;
	options.Midheaven = Midheaven;
	for (size_t i = 0; i < HiddenPlanets.size(); i++)
		if (HiddenPlanets[i])
			options.Except.push_back(static_cast<Planet>(i));
	return options;
}

ContactOptions MidpointSettings::ListContacts() const {
	ContactOptions options;
	options.Orb = Orbs[NearestOrb(ListOrb)] / 100.0;
	options.Kind = ListKind;
	return options;
}

ContactOptions MidpointSettings::TreeContacts() const {
	ContactOptions options;
	options.Orb = Orbs[NearestOrb(TreeOrb)] / 100.0;
	options.Kind = TreeKind;
	return options;
}

CString MidpointSettings::PointGlyph(ChartPoint const& point) {
	switch (point.Kind) {
		case PointKind::Ascendant: return L"Z";
		case PointKind::Midheaven: return L"X";
		default: return DefaultFont::Get().GetPlanetGlyphAsString(point.Body);
	}
}

AspectType MidpointSettings::AspectOfAngle(int angle) {
	switch (angle) {
		case 45: return AspectType::SemiSquare;
		case 90: return AspectType::Square;
		case 135: return AspectType::SesquiQuadrate;
		case 180: return AspectType::Opposition;
	}
	return AspectType::Conjunction;
}

PCWSTR MidpointSettings::AngleWords(int angle) {
	switch (angle) {
		case 0: return L"on";
		case 45: return L"semi-square";
		case 90: return L"square";
		case 135: return L"sesquiquadrate";
		case 180: return L"opposite";
	}
	return L"";
}

PCWSTR MidpointSettings::ShortAngleWords(int angle) {
	switch (angle) {
		case 45: return L"semi-sq";
		case 90: return L"sq";
		case 135: return L"sesq";
		case 180: return L"opp";
	}
	return L"";
}

std::wstring MidpointSettings::ToText() const {
	std::wstring hidden;
	for (size_t i = 0; i < HiddenPlanets.size(); i++)
		if (HiddenPlanets[i])
			hidden += (hidden.empty() ? L"" : L",") + std::to_wstring(i);
	auto kind = [](ContactKind k) { return k == ContactKind::Axis ? L"axis" : k == ContactKind::Dial45 ? L"dial45" : L"dial"; };
	return std::format(L"except={};asc={};mc={};listorb={};listkind={};treeorb={};treekind={}", hidden, Ascendant ? 1 : 0, Midheaven ? 1 : 0,
		ListOrb, kind(ListKind), TreeOrb, kind(TreeKind));
}

void MidpointSettings::FromText(std::wstring const& text) {
	auto number = [](std::wstring const& value, long& result) {
		wchar_t* end;
		result = wcstol(value.c_str(), &end, 10);
		return end != value.c_str() && *end == 0;
	};
	std::wstringstream stream(text);
	std::wstring part;
	while (std::getline(stream, part, L';')) {
		auto equals = part.find(L'=');
		if (equals == std::wstring::npos)
			continue;
		auto key = part.substr(0, equals), value = part.substr(equals + 1);
		long n;
		if (key == L"except") {
			HiddenPlanets.reset();
			std::wstringstream items(value);
			std::wstring item;
			while (std::getline(items, item, L','))
				if (number(item, n) && n >= 0 && n < static_cast<long>(HiddenPlanets.size()))
					HiddenPlanets[n] = true;
		}
		else if (key == L"asc")
			Ascendant = value != L"0";
		else if (key == L"mc")
			Midheaven = value != L"0";
		else if (key == L"listorb" && number(value, n))
			ListOrb = Orbs[NearestOrb(static_cast<int>(n))];
		else if (key == L"treeorb" && number(value, n))
			TreeOrb = Orbs[NearestOrb(static_cast<int>(n))];
		else if (key == L"listkind")
			ListKind = value == L"axis" ? ContactKind::Axis : value == L"dial45" ? ContactKind::Dial45 : ContactKind::Dial90;
		else if (key == L"treekind")
			TreeKind = value == L"axis" ? ContactKind::Axis : value == L"dial45" ? ContactKind::Dial45 : ContactKind::Dial90;
	}
}

MidpointSettings& MidpointSettings::Current() {
	static MidpointSettings settings;
	return settings;
}

void MidpointSettings::LoadFromSettings() {
	Current().FromText(AppSettings::Get().MidpointSettingsText());
}

void MidpointSettings::StoreInSettings() {
	AppSettings::Get().MidpointSettingsText(Current().ToText());
}
