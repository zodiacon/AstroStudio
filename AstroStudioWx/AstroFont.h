#pragma once

#include "AstroPoint.h"
#include "Aspects.h"

//
// Port of AstroStudio\AstroFont.h + DefaultFont.h.
//
// The glyph tables themselves are unchanged - they are plain WCHAR lookups
// with no ATL in them. Only the ...AsString() wrappers change type, from
// CString to wxString.
//
struct AstroFontBase {
	virtual ~AstroFontBase() = default;

	virtual wchar_t GetRetroGlyph() const = 0;
	virtual wchar_t GetDirectGlyph() const = 0;
	virtual wchar_t GetSignGlyph(ZodiacSign sign) const = 0;
	virtual wchar_t GetAspectGlyph(AspectType type) const = 0;
	virtual wchar_t GetPlanetGlyph(Planet type) const = 0;

	wxString GetRetroGlyphAsString() const { return wxString(GetRetroGlyph()); }
	wxString GetDirectGlyphAsString() const { return wxString(GetDirectGlyph()); }
	wxString GetSignGlyphAsString(ZodiacSign sign) const { return wxString(GetSignGlyph(sign)); }
	wxString GetAspectGlyphAsString(AspectType type) const { return wxString(GetAspectGlyph(type)); }
	wxString GetPlanetGlyphAsString(Planet type) const { return wxString(GetPlanetGlyph(type)); }
};

struct DefaultFont : AstroFontBase {
	//
	// How far the glyph tables actually reach. HamburgSymbols stops at Chiron,
	// so Planet::Pholus and everything after it (Ceres, Pallas, Juno, Vesta)
	// have no glyph - the Planet enum is larger than the font. Anything walking
	// the enum to build glyphs must stop here, not at Planet::NumPlanets.
	//
	static constexpr int PlanetGlyphCount = 16;
	static constexpr int AspectGlyphCount = 15;	// Conjunction..BiNovile

	wchar_t GetRetroGlyph() const override;
	wchar_t GetDirectGlyph() const override;
	wchar_t GetSignGlyph(ZodiacSign sign) const override;
	wchar_t GetAspectGlyph(AspectType type) const override;
	wchar_t GetPlanetGlyph(Planet type) const override;

	static AstroFontBase& Get();
};
