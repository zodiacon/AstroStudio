#include "pch.h"
#include "AstroHelpers.h"

bool AstroHelpers::LoadAstroFont() {
	static bool loaded = [] {
		// Named resource, declared in AstroStudioWx.rc. The WTL build uses the
		// numeric IDR_FONT with the same custom "TTF" resource type and the
		// same .ttf file.
		auto res = ::FindResource(nullptr, L"hamburgfont", L"TTF");
		if (!res)
			return false;

		auto handle = ::LoadResource(nullptr, res);
		if (!handle)
			return false;

		auto data = ::LockResource(handle);
		auto size = ::SizeofResource(nullptr, res);
		if (!data || size == 0)
			return false;

		DWORD count = 0;
		return ::AddFontMemResourceEx(data, size, nullptr, &count) != nullptr;
		}();

	return loaded;
}

wxString const& AstroHelpers::GlyphFontName() {
	static wxString name("HamburgSymbols");
	return name;
}

wxFont AstroHelpers::GlyphFont(int pixelHeight) {
	LoadAstroFont();
	return wxFont(wxFontInfo(wxSize(0, pixelHeight)).FaceName(GlyphFontName()));
}

void AstroHelpers::DrawGlyphRuns(wxDC& dc, std::vector<GlyphRun> const& runs) {
	// Otherwise each glyph paints an opaque rectangle over the zodiac belt.
	dc.SetBackgroundMode(wxBRUSHSTYLE_TRANSPARENT);

	for (auto const& run : runs) {
		dc.SetFont(run.Font);
		dc.SetTextForeground(run.Colour);

		// GDI+ StringFormat centred on both axes; wxDC::DrawText takes the top
		// left, so the extent has to be measured and subtracted.
		auto extent = dc.GetTextExtent(run.Text);
		dc.DrawText(run.Text,
			wxRound(run.X - extent.x / 2.0),
			wxRound(run.Y - extent.y / 2.0));
	}
}

std::vector<Planet> const& AstroHelpers::GetStandardPlanets() {
	static std::vector<Planet> planets = [] {
		std::vector<Planet> v;
		v.reserve(static_cast<size_t>(Planet::Pluto) + 1);
		for (auto p = Planet::Sun; p <= Planet::Pluto; p = static_cast<Planet>(static_cast<int>(p) + 1))
			v.push_back(p);
		return v;
		}();
	return planets;
}
