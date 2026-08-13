#include "pch.h"
#include "AstroFont.h"

namespace {
	const wchar_t SignGlyphs[] = L"asdfghjklzxc";

	// \x98 and \x9a are Windows-1252 byte values, not raw Unicode code points; the font's
	// Windows cmap only exposes those glyphs at their proper Unicode equivalents (U+02DC,
	// U+0161). Using the raw bytes directly looks up nothing there (GDI and GDI+ alike),
	// even though the glyphs exist in the font.
	const wchar_t AspectGlyphs[] = L"qtrewiy\x2dc\x161\xdc\xdd\x6f\x75\x23\x23";

	// see the note above - \x8b/\x89 are Windows-1252 bytes, mapped to their
	// real Unicode equivalents (U+2039, U+2030) here
	const wchar_t PlanetGlyphs[] = L"QWERTYUIOP\x2039{\x60\x7e\x2030M";

	static_assert(WXSIZEOF(PlanetGlyphs) - 1 == DefaultFont::PlanetGlyphCount);
	static_assert(WXSIZEOF(AspectGlyphs) - 1 == DefaultFont::AspectGlyphCount);
	static_assert(WXSIZEOF(SignGlyphs) - 1 == 12);
}

wchar_t DefaultFont::GetRetroGlyph() const {
	return L'>';
}

wchar_t DefaultFont::GetDirectGlyph() const {
	return L'*';
}

wchar_t DefaultFont::GetSignGlyph(ZodiacSign sign) const {
	auto index = static_cast<int>(sign);
	wxASSERT(index >= 0 && index < 12);
	return index >= 0 && index < 12 ? SignGlyphs[index] : L' ';
}

wchar_t DefaultFont::GetAspectGlyph(AspectType type) const {
	auto index = static_cast<int>(type);
	wxASSERT(index >= 0 && index < AspectGlyphCount);
	return index >= 0 && index < AspectGlyphCount ? AspectGlyphs[index] : L' ';
}

wchar_t DefaultFont::GetPlanetGlyph(Planet planet) const {
	auto index = static_cast<int>(planet);
	wxASSERT(index >= 0 && index < PlanetGlyphCount);
	// Falls back to a blank rather than reading past the table: the Planet enum
	// runs to Vesta but the font stops at Chiron.
	return index >= 0 && index < PlanetGlyphCount ? PlanetGlyphs[index] : L' ';
}

AstroFontBase& DefaultFont::Get() {
	static DefaultFont font;
	return font;
}
