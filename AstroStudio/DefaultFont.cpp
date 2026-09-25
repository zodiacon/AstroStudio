#include "pch.h"
#include "DefaultFont.h"

WCHAR DefaultFont::GetRetroGlyph() const {
    return '>';
}

WCHAR DefaultFont::GetDirectGlyph() const {
    return L'*';
}

WCHAR DefaultFont::GetSignGlyph(ZodiacSign sign) const {
    static const WCHAR glyphs[] = L"asdfghjklzxc";
    constexpr int count = _countof(glyphs) - 1;    // less the terminator
    auto index = (int)sign;
    ATLASSERT(index >= 0 && index < count);
    return index >= 0 && index < count ? glyphs[index] : L' ';
}

WCHAR DefaultFont::GetAspectGlyph(AspectType type) const {
    // \x98 and \x9a are Windows-1252 byte values, not raw Unicode code points; the font's
    // Windows cmap only exposes those glyphs at their proper Unicode equivalents (U+02DC,
    // U+0161). Using the raw bytes directly looks up nothing there (GDI and GDI+ alike),
    // even though the glyphs exist in the font.
    static const WCHAR aspects[] = L"qtrewiy\x2dc\x161\xdc\xdd\x6f\x75\x23\x23";

    // The lower bound matters here: AspectType::None is -1, which passed the
    // old "< _countof" test and indexed aspects[-1].
    constexpr int count = _countof(aspects) - 1;    // less the terminator
    auto index = (int)type;
    ATLASSERT(index >= 0 && index < count);
    return index >= 0 && index < count ? aspects[index] : L' ';
}

WCHAR DefaultFont::GetPlanetGlyph(Planet planet) const {
    // see the note in GetAspectGlyph - \x8b/\x89 are Windows-1252 bytes, mapped to their
    // real Unicode equivalents (U+2039, U+2030) here
    //
    // The trailing five come from the "INDEX #" column of HamburgSymbols.pdf:
    // Pholus 172 ("centaur 5154-Pholus" there; 5145 is the real number),
    // Ceres 67, Pallas 86 ("the generally accepted symbol for Pallas-Athena"),
    // Juno 66, Vesta 78 ("preferred symbol of North American astrologers" -
    // three alternates exist at 166-168).
    //
    // The Part of Fortune is the circle with a cross, code 60 (the less-than sign) of the font.
    //
    // 172 is 0xAC, which sits in the Latin-1 half of Windows-1252 and so needs
    // no remapping, unlike the 0x80-0x9F entries above. The literal has to be
    // split before "CVBN": \xacC would otherwise be swallowed as a single hex
    // escape, since C is a hex digit.
    static const WCHAR planets[] = L"QWERTYUIOP\x2039{\x60\x7e\x2030M\xac" L"CVBN<";

    // The table used to stop at Chiron while Planet ran on to Vesta, so asking
    // for any of the last five read past the end - ATLASSERT caught it in debug
    // and nothing caught it in release. This keeps the two in step by
    // construction: adding a Planet now fails to compile until it has a glyph.
    static_assert(_countof(planets) - 1 == (int)Planet::NumPlanets,
        "planet glyph table must cover every Planet value");

    auto index = (int)planet;
    ATLASSERT(index >= 0 && index < (int)Planet::NumPlanets);
    return index >= 0 && index < (int)Planet::NumPlanets ? planets[index] : L' ';
}

AstroFontBase& DefaultFont::Get() {
    static DefaultFont font;
    return font;
}
