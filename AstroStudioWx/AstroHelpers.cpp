#include "pch.h"
#include "AstroHelpers.h"
#include "AstroFont.h"
#include "Aspects.h"
#include "DateTime.h"
#include <cmath>

namespace {
	const wxString DegreeSign(wxUniChar(0x00B0));	// the typographic degree mark
	const wxString GlyphDegree(wxUniChar(59));		// ';' - the degree mark in HamburgSymbols
	const wxString GlyphMinute(wxUniChar(39));		// '\'' - likewise for minutes
}

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

wxString AstroHelpers::FormatLongitude(AstroPoint const& value, FormatOptions options,
	AstroFontBase const* font) {
	if (!font)
		font = &DefaultFont::Get();

	auto showSeconds = (options & FormatOptions::ShowSeconds) == FormatOptions::ShowSeconds;
	auto useGlyphs = (options & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs;
	auto showDegree = (options & FormatOptions::ShowDegreeGlyph) == FormatOptions::ShowDegreeGlyph;

	auto text = wxString::Format("%02d%s %s %02d%s",
		static_cast<int>(value.DegreeInSign()),
		showDegree ? (useGlyphs ? GlyphDegree : DegreeSign) : wxString(),
		useGlyphs ? font->GetSignGlyphAsString(value.Sign())
				  : GetZodiacSignName(value.Sign()).Left(3),
		static_cast<int>(value.Minutes() + (showSeconds ? 0 : .5)),
		showDegree ? GlyphMinute : wxString());

	if (showSeconds)
		text += wxString::Format("%02d\"", static_cast<int>(value.Seconds() + .5));

	if ((value.Flags & AstroPointFlags::Retro) == AstroPointFlags::Retro)
		text += useGlyphs ? ">" : "R";	// '>' is the retrograde glyph in HamburgSymbols

	return text;
}

wxString AstroHelpers::FormatDateTime(DateTime const& dt, DateTimeFormatOptions options) {
	wxString text;
	if ((options & DateTimeFormatOptions::TimeOnly) == DateTimeFormatOptions::None)
		text += wxString::Format("%5d/%02d/%02d ", dt.Year(), dt.Month(), dt.Day());
	if ((options & DateTimeFormatOptions::DateOnly) == DateTimeFormatOptions::None)
		text += wxString::Format("%02d.%02d ", dt.Hour(), dt.Minute());
	return text;
}

wxColour AstroHelpers::Darken(wxColour const& colour, int offset) {
	return wxColour(std::max(0, colour.Red() - offset),
		std::max(0, colour.Green() - offset),
		std::max(0, colour.Blue() - offset));
}

wxColour AstroHelpers::Lighten(wxColour const& colour, int offset) {
	return wxColour(std::min(255, colour.Red() + offset),
		std::min(255, colour.Green() + offset),
		std::min(255, colour.Blue() + offset));
}

wxString AstroHelpers::FormatLatitude(double lat) {
	auto abs = std::fabs(lat);
	return wxString::Format("%d%s %02d' %c", static_cast<int>(abs), DegreeSign,
		static_cast<int>(60 * (abs - static_cast<int>(abs))), lat < 0 ? 'S' : 'N');
}

std::tuple<int, int, int> AstroHelpers::GetDegMinSec(double angle, bool sign) {
	if (!sign)
		angle = std::fabs(angle);
	auto deg = static_cast<int>(angle);
	auto min = static_cast<int>((angle - deg) * 60);
	return { deg, min, 0 };
}

wxString AstroHelpers::GetPlanetName(Planet type) {
	static const wxString names[] = {
		"Sun", "Moon", "Mercury", "Venus", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune", "Pluto",
		"Mean Node", "True Node", "Lilith", "True Lilith", "Earth", "Chiron", "Pholus",
		"Ceres", "Pallas", "Juno", "Vesta",
	};
	wxASSERT(static_cast<int>(type) < WXSIZEOF(names));
	return names[static_cast<int>(type)];
}

wxString AstroHelpers::GetAspectName(AspectType type) {
	static const wxString names[] = {
		"Conjunction", "Sextile", "Square", "Trine", "Opposition",
		"Semi-Sextile", "Semi-Square", "Quintile", "Bi-Quintile", "Septile",
		"Bi-Septile", "Quincunx", "Sesquiquadrate", "Novile", "Bi-Novile",
	};
	wxASSERT(static_cast<int>(type) >= 0 && static_cast<int>(type) < WXSIZEOF(names));
	return names[static_cast<int>(type)];
}

wxString AstroHelpers::GetZodiacSignName(ZodiacSign sign) {
	static const wxString signs[] = {
		"Aries", "Taurus", "Gemini", "Cancer", "Leo", "Virgo",
		"Libra", "Scorpio", "Sagittarius", "Capricorn", "Aquarius", "Pisces",
	};
	return signs[static_cast<int>(sign)];
}

wxString AstroHelpers::HouseSystemToString(HouseSystem system) {
	switch (system) {
		using enum HouseSystem;
		case Placidus:			return "Placidus";
		case Koch:				return "Koch";
		case Porphyrius:		return "Porphyrius";
		case Regiomontanus:		return "Regiomontanus";
		case Campanus:			return "Campanus";
		case Equal:				return "Equal";
		case Morinus:			return "Morinus";
		case Topocentric:		return "Topocentric";
		case Alcabitus:			return "Alcabitus";
		case Horizontal:		return "Horizontal";
		case Krusinski:			return "Krusinski";
		case EqualWholeSign:	return "Equal / Whole Sign";
		case CarterPoliEqu:		return "Carter Poli-Equal";
		case EqualMC:			return "Equal (MC)";
		case Sunshine:			return "Sunshine";
		case SunshineAlt:		return "Sunshine / Alt";
		case APCHouses:			return "APC Houses";
	}
	wxFAIL_MSG("unknown house system");
	return wxString();
}

wxColour AstroHelpers::ElementColour(ZodiacSign sign) {
	return ElementColour(static_cast<int>(sign) % 4);
}

wxColour AstroHelpers::ElementColour(int elementIndex) {
	static const wxColour colours[] = {
		wxColour(255, 69, 0),		// OrangeRed
		wxColour(250, 250, 210),	// LightGoldenrodYellow
		wxColour(144, 238, 144),	// LightGreen
		wxColour(173, 216, 230),	// LightBlue
	};

	auto colour = colours[elementIndex % 4];
	if (wxSystemSettings::GetAppearance().IsDark()) {
		// ColorHelper::Darken(colour, 40) in ChartDetailsView::OnItemPrePaint.
		constexpr int offset = 40;
		colour.Set(std::max(0, colour.Red() - offset),
			std::max(0, colour.Green() - offset),
			std::max(0, colour.Blue() - offset));
	}
	return colour;
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
