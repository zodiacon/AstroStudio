#include "pch.h"
#include "AstroFont.h"

wchar_t DefaultFont::GetRetroGlyph() const {
	return L'>';
}

wchar_t DefaultFont::GetDirectGlyph() const {
	return L'*';
}

wchar_t DefaultFont::GetSignGlyph(ZodiacSign sign) const {
	static const wchar_t glyphs[] = L"asdfghjklzxc";
	return glyphs[(int)sign];
}

wchar_t DefaultFont::GetAspectGlyph(AspectType type) const {
	// \x98 and \x9a are Windows-1252 byte values, not raw Unicode code points; the font's
	// Windows cmap only exposes those glyphs at their proper Unicode equivalents (U+02DC,
	// U+0161). Using the raw bytes directly looks up nothing there (GDI and GDI+ alike),
	// even though the glyphs exist in the font.
	static const wchar_t aspects[] = L"qtrewiy\x2dc\x161\xdc\xdd\x6f\x75\x23\x23";
	wxASSERT((int)type < WXSIZEOF(aspects));
	return aspects[(int)type];
}

wchar_t DefaultFont::GetPlanetGlyph(Planet planet) const {
	// see the note in GetAspectGlyph - \x8b/\x89 are Windows-1252 bytes, mapped to their
	// real Unicode equivalents (U+2039, U+2030) here
	static const wchar_t planets[] = L"QWERTYUIOP\x2039{\x60\x7e\x2030M";
	wxASSERT((int)planet < WXSIZEOF(planets));
	return planets[(int)planet];
}

AstroFontBase& DefaultFont::Get() {
	static DefaultFont font;
	return font;
}
