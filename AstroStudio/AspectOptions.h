#pragma once

#include "Aspects.h"

// How aspects are worked out: one set of AspectSettings for the aspects within a chart and one for transits (the
// aspects from the planets of another moment to the chart's). The current options are kept in the app settings and
// can be saved to and loaded from files, as a set of configured aspects and planets.
//
// A file is UTF-8 INI text (see IniDocument), meant to be readable and editable by hand:
//
//   [Aspects]                 Version
//   [Chart]  [Transit]        MajorOrb, MinorOrb (degrees), MajorOnly (true/false)
//   [Chart.Aspects]           one key per aspect, by name: its orb in degrees, "off", or "default" (the general orb of its kind)
//   [Chart.Planets]           one key per planet, by name: "on" or "off" (left out of the aspects)
//   [Chart.ExtraOrbs]         planets that have extra orb, by name: degrees added for the aspects they take part in
//
// (and the same again for [Transit...]). Whatever is missing keeps its default, so a file can hold just what differs.
struct AspectOptions {
	static constexpr PCWSTR Extension = L"aspects";
	// for the Open and Save dialogs
	static constexpr wchar_t Filter[] = L"Aspect settings (*.aspects)\0*.aspects\0All files (*.*)\0*.*\0";
	static constexpr float MaxOrb = 30;

	// the defaults: the usual orbs for a chart; for transits the major aspects only, within 3 degrees
	AspectOptions();

	AspectSettings Chart;
	AspectSettings Transit;

	// UTF-8 INI text
	std::string ToText() const;
	// Replaces the options with what the text says (whatever it doesn't say is the default). On failure nothing changes and
	// error says why, in words for the user.
	bool FromText(std::string_view text, std::wstring& error);
	bool Save(PCWSTR path, std::wstring& error) const;
	bool Load(PCWSTR path, std::wstring& error);

	// the options in force, which the chart views calculate their aspects by
	static AspectOptions& Current();
	// Reads Current from / writes it to the app settings (AppSettings::AspectSets).
	static void LoadFromSettings();
	static void StoreInSettings();
};
