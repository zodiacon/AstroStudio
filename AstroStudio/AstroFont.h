#pragma once

#include "AstroPoint.h"
#include "Aspects.h"

struct AstroFontBase abstract {
	virtual WCHAR GetRetroGlyph() const = 0;
	CString GetRetroGlyphAsString() const {
		return CString(GetRetroGlyph());
	}	
	virtual WCHAR GetDirectGlyph() const = 0;
	CString GetDirectGlyphAsString() const;

	virtual WCHAR GetSignGlyph(ZodiacSign sign) const = 0;
	CString GetSignGlyphAsString(ZodiacSign sign) const;

	virtual WCHAR GetAspectGlyph(AspectType type) const = 0;
	CString GetAspectGlyphAsString(AspectType type) const {
		return CString(GetAspectGlyph(type));
	}
	virtual WCHAR GetPlanetGlyph(PlanetType type) const = 0;
	CString GetPlanetGlyphAsString(PlanetType type) const {
		return CString(GetPlanetGlyph(type));
	}
};
