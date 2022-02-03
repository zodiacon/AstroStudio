#pragma once

#include "AstroPoint.h"

struct AstroFontBase abstract {
	virtual WCHAR GetRetroGlyph() const = 0;
	CString GetRetroGlyphAsString() const {
		return CString(GetRetroGlyph());
	}	
	virtual WCHAR GetDirectGlyph() const = 0;
	CString GetDirectGlyphAsString() const;

	virtual WCHAR GetSignGlyph(ZodiacSign sign) const = 0;
	CString GetSignGlyphAsString(ZodiacSign sign) const;
};
