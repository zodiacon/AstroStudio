#pragma once

#include "D2DChartDrawing.h"
#include <array>
#include <optional>
#include <string>
#include <string_view>

// The colours of the chart wheel's elements that the user can change.
enum class ChartColor {
	Background,
	Fire, Earth, Air, Water,		// the belts of the zodiac signs
	SoftAspect, HardAspect, MinorAspect,
	Text,			// glyphs, the outlines of the belts and the lines of the angles
	Grid,			// the house lines and the edges of the band around the chart
	Dot,			// where the chart's planets are
	OverlayBand,	// the band around the chart (transits...)
	Overlay,		// the planets in it, and the caption
	Count,
};

// The user's colours for the chart wheel, for the light and for the dark look each: an element without one has the
// program's own colour. A set of colours can also be saved to and loaded from a file, below. Kept in the app settings (AppSettings::ChartColors) as text like  light=0:f5f5f5,3:10c0a0;dark=1:8c802d  -
// the numbers are the elements (ChartColor), the colours are RRGGBB in hex.
//
// A file is UTF-8 INI text (see IniDocument), meant to be readable and editable by hand:
//
//   [Colors]           Version
//   [Light]  [Dark]    one key per element (Background, Fire, Earth, Air, Water, SoftAspects, HardAspects, OtherAspects, Text,
//                      HouseLines, PlanetDots, OverlayBand, OverlayPlanets): its colour as #RRGGBB, or "default"
//
// A look that a file has a section for is replaced by it (what the section leaves out is the default); a look it has no section
// for is not touched, so a file can hold the colours of one look only.
struct ChartColors {
	static constexpr PCWSTR Extension = L"colors";
	// for the Open and Save dialogs
	static constexpr wchar_t Filter[] = L"Chart colours (*.colors)\0*.colors\0All files (*.*)\0*.*\0";

	using Set = std::array<std::optional<COLORREF>, static_cast<size_t>(ChartColor::Count)>;
	Set Light, Dark;

	Set& For(bool dark) {
		return dark ? Dark : Light;
	}
	Set const& For(bool dark) const {
		return dark ? Dark : Light;
	}

	static PCWSTR Name(ChartColor color);
	// the name of the element in a file
	static PCWSTR Key(ChartColor color);
	// the program's colour for an element in the light or the dark look
	static COLORREF Default(ChartColor color, bool dark);
	// the colour in use: the user's, or the default
	COLORREF Get(ChartColor color, bool dark) const {
		return For(dark)[static_cast<size_t>(color)].value_or(Default(color, dark));
	}
	// puts the user's colours for the look into the drawing parameters (which start out as that look's defaults)
	void Apply(ChartDrawingParameters& params, bool dark) const;

	std::wstring ToText() const;
	// Reads what ToText wrote; anything it doesn't understand is left as it was.
	void FromText(std::wstring const& text);

	// Files. On failure error says why, in words for the user, and Load changes nothing.
	bool Save(PCWSTR path, std::wstring& error) const;
	bool Load(PCWSTR path, std::wstring& error);
	std::string ToIni() const;
	bool FromIni(std::string_view text, std::wstring& error);

	// the colours in force, which every chart wheel draws by
	static ChartColors& Current();
	static void LoadFromSettings();
	static void StoreInSettings();
};
