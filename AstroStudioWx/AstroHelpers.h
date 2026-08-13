#pragma once

#include "AstroPoint.h"
#include "AstroCalculator.h"
#include <vector>

struct AstroFontBase;
enum class AspectType;

//
// Ported from AstroStudio\Helpers.h unchanged.
//
enum class FormatOptions {
	None = 0,
	ShowSeconds = 1,
	UseGlyphs = 2,
	ShowDegreeGlyph = 4,
	Default = ShowDegreeGlyph | UseGlyphs,
};
DEFINE_ENUM_FLAG_OPERATORS(FormatOptions);

enum class DateTimeFormatOptions {
	None = 0,
	DateOnly = 1,
	TimeOnly = 2,
	Default = DateOnly,
};
DEFINE_ENUM_FLAG_OPERATORS(DateTimeFormatOptions);

class DateTime;

//
// The portable half of AstroStudio\Helpers.h.
//
// The WTL Helpers is a single struct mixing pure astrology utilities with
// ATL/GDI+ ones (CString returns, Gdiplus::FontFamily, COLORREF blending), so
// it cannot be shared as-is. Only what a phase actually needs gets brought
// across, retyped where necessary:
//
//   phase 2  GetStandardPlanets
//   phase 3  GetAstroFontFamily        -> wxFont from a private font
//   phase 4  FormatLongitude/Latitude  -> wxString instead of CString
//   phase 5  GetPlanetName/GetAspectName/GetZodiacSignName, Darken/Lighten
//
namespace AstroHelpers {
	// Sun through Pluto, in order. Identical to Helpers::GetStandardPlanets.
	std::vector<Planet> const& GetStandardPlanets();

	//
	// Makes HamburgSymbols available to this process without installing it
	// system-wide, from the TTF resource embedded in the executable.
	//
	// Helpers::LoadAstroFont did two things: AddFontMemResourceEx, and a
	// separate Gdiplus::PrivateFontCollection used only to hand ChartDrawing a
	// FontFamily. That duplication is not redundancy - AddFontMemResourceEx
	// registers the face with *GDI*, and GDI+ will not find it when asked for
	// a family by name, which is why the original needed both.
	//
	// Only the GDI registration is needed here, but it constrains how the font
	// is used: see GlyphFont below.
	//
	// Safe to call more than once; the work happens on the first call.
	bool LoadAstroFont();

	// Face name of the glyph font.
	wxString const& GlyphFontName();

	// A wxFont for the glyph face at the given height in device pixels.
	wxFont GlyphFont(int pixelHeight);

	//
	// A piece of text to be drawn centred on (X, Y), in device pixels.
	//
	// Glyphs cannot be drawn through wxGraphicsContext at all here, which is
	// the reason this exists. Measured behaviour on this machine:
	//
	//                                     GDI+      GDI
	//   AddFontMemResourceEx (memory)     falls     resolves
	//   AddFontResourceEx FR_PRIVATE      back      -
	//
	// GDI+ ignores private fonts entirely and silently substitutes Microsoft
	// Sans Serif, so HamburgSymbols renders as plain Latin letters. Its only
	// route to a private face is a Gdiplus::PrivateFontCollection passed to the
	// Gdiplus::Font constructor - which is exactly why AstroStudio\Helpers.cpp
	// keeps one alongside its AddFontMemResourceEx call - and wx exposes no way
	// to supply one. wxFont::AddPrivateFont would not help either; it is
	// file-based FR_PRIVATE, which GDI+ also ignores.
	//
	// So shapes go through wxGraphicsContext (antialiased, transformed) and
	// text is collected into these and drawn afterwards with wxDC::DrawText,
	// which is GDI and resolves the face correctly. Drawing all text after all
	// shapes also puts every glyph on top, which is what the chart wants.
	//
	struct GlyphRun {
		wxString Text;
		double X{}, Y{};
		wxFont Font;
		wxColour Colour;
	};

	void DrawGlyphRuns(wxDC& dc, std::vector<GlyphRun> const& runs);

	//
	// Formatting, ported from Helpers with CString retyped to wxString. The
	// degree/minute marks are built from wxUniChar rather than written as
	// literals, so the source stays pure ASCII and does not depend on the
	// compiler's source-charset guess.
	//
	wxString FormatLongitude(AstroPoint const& value,
		FormatOptions options = FormatOptions::Default,
		AstroFontBase const* font = nullptr);
	wxString FormatLatitude(double lat);
	wxString FormatDateTime(DateTime const& dt,
		DateTimeFormatOptions options = DateTimeFormatOptions::Default);

	// Per-channel clamped shifts, replacing ColorHelper::Darken/Lighten.
	wxColour Darken(wxColour const& colour, int offset);
	wxColour Lighten(wxColour const& colour, int offset);
	std::tuple<int, int, int> GetDegMinSec(double angle, bool sign = false);

	wxString GetPlanetName(Planet type);
	wxString GetAspectName(AspectType type);
	wxString GetZodiacSignName(ZodiacSign sign);
	wxString HouseSystemToString(HouseSystem system);

	// Element colour, matching ChartDrawingParameters::ElementColor. Darkened
	// when the app is in dark mode, as ChartDetailsView did with
	// ColorHelper::Darken. The index form is what the zodiac belt wants, which
	// walks elements 0-3 rather than signs.
	wxColour ElementColour(int elementIndex);
	wxColour ElementColour(ZodiacSign sign);
}
