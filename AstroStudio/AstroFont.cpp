#include "pch.h"
#include "AstroFont.h"

CString AstroFontBase::GetDirectGlyphAsString() const {
	return CString(GetDirectGlyph());
}

CString AstroFontBase::GetSignGlyphAsString(ZodiacSign sign) const {
	return CString(GetSignGlyph(sign));
}
