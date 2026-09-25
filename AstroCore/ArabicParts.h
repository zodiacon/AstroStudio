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
};

struct PartPoint {
	PartPointKind Kind{ PartPointKind::Ascendant };
	Planet Body{ Planet::Sun };
	int Number{ 0 };
	std::wstring Part;

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
};

enum class PartReversal {
	Never,			// the same by day and by night
	AtNight,		// Plus and Minus change places at night
};

struct PartDefinition {
	std::wstring Name;
	PartPoint Base, Plus, Minus;
	PartReversal Reversal{ PartReversal::AtNight };
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
	// A starting set, from Hellenistic tradition (each "from X to Y" is Asc + Y - X, reversed at night): Fortune, Spirit,
	// Eros (Spirit to Venus), Courage (Fortune to Mars), Victory (Spirit to Jupiter), Nemesis (Fortune to Saturn),
	// Father (Sun to Saturn), Mother (Moon to Venus), and two from Lilly that do not turn round: Marriage (Asc + 7th cusp -
	// Venus) and Death (Asc + 8th cusp - Moon). The parts that build on Fortune and Spirit come after them. Sources differ on
	// many of these: the list is a set of definitions to use as is, change or add to.
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
