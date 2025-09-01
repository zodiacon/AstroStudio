#pragma once

#include "AstroFont.h"

struct DefaultFont : AstroFontBase {
	WCHAR GetRetroGlyph() const override;
	WCHAR GetDirectGlyph() const override;
	WCHAR GetSignGlyph(ZodiacSign sign) const override;
	WCHAR GetAspectGlyph(AspectType type) const override;
	WCHAR GetPlanetGlyph(Planet type) const override;

	static AstroFontBase& Get();
};
