#pragma once

#include "Aspects.h"
#include "ChartData.h"
#include <optional>
#include <string>
#include <vector>

// Arabic parts (lots): a point worked out from three others as Base + Plus - Minus, on the ecliptic. Most of them turn round
// at night (Plus and Minus change places), which is what makes the Part of Fortune Asc + Moon - Sun by day and
// Asc + Sun - Moon by night. No UI here.

// What one of the three points is.
enum class PartPointKind {
	Planet,			// Body
	Ascendant,
	Midheaven,
	Descendant,
	Imum,			// the IC
	Cusp,			// the cusp of house Number (1-12)
	RulerOfCusp,	// the planet that rules the sign on the cusp of house Number (1-12): where that planet is in the chart
	Part,			// another part, by name: it must come earlier in the list being calculated
	Longitude,		// a fixed place in the zodiac, Degrees (0-360) from 0 Aries
};

struct PartPoint {
	PartPointKind Kind{ PartPointKind::Ascendant };
	Planet Body{ Planet::Sun };
	int Number{ 0 };
	std::wstring Part;
	double Degrees{ 0 };

	static PartPoint Of(Planet planet) {
		return { PartPointKind::Planet, planet };
	}
	static PartPoint Asc() {
		return { PartPointKind::Ascendant };
	}
	static PartPoint MC() {
		return { PartPointKind::Midheaven };
	}
	static PartPoint Desc() {
		return { PartPointKind::Descendant };
	}
	static PartPoint IC() {
		return { PartPointKind::Imum };
	}
	static PartPoint Cusp(int house) {
		return { PartPointKind::Cusp, Planet::Sun, house };
	}
	static PartPoint RulerOfCusp(int house) {
		return { PartPointKind::RulerOfCusp, Planet::Sun, house };
	}
	static PartPoint OtherPart(std::wstring name) {
		return { PartPointKind::Part, Planet::Sun, 0, std::move(name) };
	}
	// a fixed longitude, e.g. At(19) is 19 Aries and At(33) 3 Taurus
	static PartPoint At(double degrees) {
		return { PartPointKind::Longitude, Planet::Sun, 0, {}, degrees };
	}
};

enum class PartReversal {
	Never,			// the same by day and by night
	AtNight,		// Plus and Minus change places at night
	Replaced,		// at night NightPlus and NightMinus are used instead of Plus and Minus (Exaltation: the Sun by day, the Moon by night)
};

struct PartDefinition {
	std::wstring Name;
	PartPoint Base, Plus, Minus;
	PartReversal Reversal{ PartReversal::AtNight };
	PartPoint NightPlus, NightMinus;		// for PartReversal::Replaced
};

// Whether the chart is a day chart or a night chart.
enum class SectMode {
	Chart,			// by the Sun: above the horizon is day
	Day,			// as if it were a day chart
	Night,
};

struct PartOptions {
	SectMode Sect{ SectMode::Chart };
	// off: no part turns round at night (the way some modern texts give them)
	bool ReverseAtNight{ true };
	// Uranus, Neptune and Pluto rule Aquarius, Pisces and Scorpio (for RulerOfCusp); otherwise the seven traditional planets do
	bool ModernRulers{ false };
};

struct PartData {
	std::wstring Name;
	AstroPoint Longitude;
	bool Night{ false };		// the chart counted as a night chart
	bool Reversed{ false };		// so this part turned round
	int House{ 0 };				// the house it falls in (0 if the chart has no houses)
	std::wstring Formula;		// as it was used: "Asc + Moon - Sun"
};

// An aspect between a part and a planet of the chart.
struct PartAspect {
	size_t Part;				// the index of the part in the list that was searched
	PlanetPosition Planet;
	AspectType Type;
	float Angle;				// the angle between the two, 0-180
	float Orb;					// how far from exact
	float MaxOrb;				// the widest orb the settings allow for this aspect and planet
};

class ArabicParts final {
public:
	// A starting set (each "from X to Y" is Asc + Y - X by day, turned round at night unless said otherwise); the sources differ
	// on many of these, so it is a set of definitions to use as is, change or add to. The formulas were checked against
	// Wikipedia ("Arabic parts": the seven Hermetic lots), Paulus Alexandrinus as given by Seven Stars Astrology and
	// astrology-x-files.com, Astrolium, and, for the medieval parts, al-Biruni and Lilly as collected by Sarah's Astrology.
	//   Hermetic lots (Paulus): Fortune (Sun to Moon), Spirit (Moon to Sun), Eros (Spirit to Venus), Necessity (Mercury to Fortune),
	//   Courage (Mars to Fortune), Victory (Spirit to Jupiter), Nemesis (Saturn to Fortune); Exaltation (Valens: Asc + 19 Aries -
	//   Sun by day, Asc + 3 Taurus - Moon by night).
	//   Family: Father (Sun to Saturn), Mother (Venus to Moon), Affliction (Saturn to Mars), Destroyer (the ruler of the Ascendant to
	//   the Moon) - and, not turned round, Brethren (Saturn to Jupiter), Children (Jupiter to Saturn), Sons (Jupiter to Mercury),
	//   Daughters (Jupiter to Venus), Marriage for men (Saturn to Venus) and for women (Venus to Saturn) as Paulus has them.
	//   Medieval, not turned round: Marriage (Asc + 7th cusp - Venus), Death (Asc + 8th cusp - Moon), Sickness (Saturn to Mars),
	//   Servants (Mercury to Moon), Debt (Mercury to Saturn), Discord (Mars to Jupiter), Merchandise (Spirit to Fortune), Travel
	//   (the ruler of the 9th cusp to the 9th cusp).
	// The parts that build on Fortune and Spirit come after them.
	static std::vector<PartDefinition> const& Standard();

	// true if the Sun is above the horizon: from the Descendant forward to the Ascendant (the houses 7 to 12)
	static bool IsDay(AstroPoint const& sun, AstroPoint const& ascendant);
	// The chart's sect by its Sun; nothing if it has none.
	static std::optional<bool> IsDayChart(ChartData const& chart);

	// The planet that rules a sign: the traditional ruler, or the modern one (which for Scorpio, Aquarius and Pisces is
	// Pluto, Uranus and Neptune).
	static Planet Ruler(ZodiacSign sign, bool modern = false);

	// The longitude of a point in a chart; nothing if the chart lacks it (a planet it doesn't have, houses out of range, a
	// part that hasn't been calculated). `parts` are the parts calculated so far, for PartPointKind::Part.
	static std::optional<AstroPoint> Position(PartPoint const& point, ChartData const& chart, PartOptions const& options = {},
		std::vector<PartData> const& parts = {});

	// One part. Nothing if a point it needs is missing.
	static std::optional<PartData> Calculate(ChartData const& chart, PartDefinition const& definition, PartOptions const& options = {},
		std::vector<PartData> const& parts = {});
	// A list of parts in the order given (so a part can use those before it); those whose points the chart lacks are left out.
	static std::vector<PartData> Calculate(ChartData const& chart, std::vector<PartDefinition> const& definitions, PartOptions const& options = {});
	// the standard set
	static std::vector<PartData> Calculate(ChartData const& chart, PartOptions const& options = {});

	// The aspects between the parts and the planets, by the aspect settings of the calculator. A part isn't a planet, so the
	// orb is the aspect's own plus the planet's extra; a planet or aspect the settings switch off makes none. A pair makes at
	// most one aspect (the first that fits, major aspects first), as in AspectCalculator. In the order of the parts, then the
	// planets. (Whether the aspect is applying is not told: a part has no speed.)
	static std::vector<PartAspect> Aspects(std::vector<PartData> const& parts, std::vector<PlanetPosition> const& planets,
		AspectCalculator const& calculator);

	// "Asc + Moon - Sun": the formula as it reads by day, or with the turn at night
	static std::wstring Describe(PartDefinition const& definition, bool reversed = false);
	static std::wstring Describe(PartPoint const& point);
};
