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
    return glyphs[(int)sign];
}

WCHAR DefaultFont::GetAspectGlyph(AspectType type) const {
    static const WCHAR aspects[] = L"qtrewiy\x98\x9a\xdc\xdd\x6f\x75\x23\x23";
    ATLASSERT((int)type < _countof(aspects));
    return aspects[(int)type];
}

WCHAR DefaultFont::GetPlanetGlyph(Planet planet) const {
    static const WCHAR planets[] = L"QWERTYUIOP\x8b{\x60\x7e\x89M";
    ATLASSERT((int)planet < _countof(planets));
    return planets[(int)planet];
}

AstroFontBase& DefaultFont::Get() {
    static DefaultFont font;
    return font;
}
