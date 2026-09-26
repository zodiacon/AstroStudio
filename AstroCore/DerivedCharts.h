#pragma once

#include "ChartData.h"
#include "Aspects.h"
#include <optional>

// A tropical year in days, and the yearly arcs the directions use.
constexpr double TropicalYear = 365.242189;
constexpr double NaibodArc = 0.98564736;		// degrees a year: the mean motion of the Sun
constexpr double PtolemyArc = 1.0;				// degrees a year

enum class ProgressionMethod {
	// Secondary progressions: a day stands for a year. The planets are where they are on the day after birth that is as many
	// days after birth as the target is years.
	Secondary,
	// Primary progressions (directions): the sky's daily turning carries the angles on by an arc of right ascension that grows
	// with the years, while the planets stay where they were at birth. Only the houses change.
	Primary,
	// Solar arc directions: every point of the chart (planets and angles) is moved on by the same arc, the distance the
	// progressed Sun has travelled (or a mean arc, see ArcKey).
	SolarArc,
};

// How the angles and houses of secondary progressions are found.
enum class ProgressedAngles {
	Calculated,		// cast for the progressed moment, at the birth place
	SolarArc,		// the birth Midheaven moved on by the solar arc, and the houses that go with it at the birth place
	Natal,			// the birth houses, unchanged
};

// The yearly arc by which primary and solar arc directions move.
enum class ArcKey {
	Actual,			// solar arc: the true distance the progressed Sun has moved (for primary directions this is the Naibod arc)
	Naibod,			// 0.98564736 degrees a year
	Ptolemy,		// 1 degree a year
};

struct ProgressionOptions {
	ProgressionMethod Method{ ProgressionMethod::Secondary };
	ProgressedAngles Angles{ ProgressedAngles::Calculated };		// secondary progressions only
	ArcKey Key{ ArcKey::Actual };									// primary and solar arc directions
};

enum class CompositeHouses {
	MidpointMC,			// the Midheaven is the midpoint of the two Midheavens
	MidpointARMC,		// the sidereal time at the Midheaven is the midpoint of the two
};

enum class ReturnSearch {
	Next,				// the first time after the starting moment
	Previous,			// the last time before it
	Nearest,			// whichever of those is closer to it
};

// What a chart worked out from others is made of, which is all that needs saving of it: the charts it was made from (as their
// birth details - positions are recalculated) and what was chosen. DerivedCharts::Build makes the chart again.
enum class DerivedKind {
	Composite,		// of A and B
	Davison,		// of A and B
	SolarArc,		// of A, moved to a date by the solar arc
	Progressed,		// of A, secondary progressed to a date
	Primary,		// of A, primary directions to a date (the planets stay, the angles and houses move)
};

struct DerivedRecipe {
	DerivedKind Kind{ DerivedKind::Composite };
	ChartData A, B;							// B only for a composite or a Davison chart
	CompositeHouses Houses{ CompositeHouses::MidpointMC };		// composite
	DateTime Target;						// solar arc, progressed, primary: the date it is moved to (UT) ...
	TimeZoneInfo Zone;						// ... and the zone it is shown in
	ArcKey Key{ ArcKey::Actual };			// solar arc and primary directions
	ProgressedAngles Angles{ ProgressedAngles::Calculated };		// progressed

	// made of two charts (A and B) rather than moved on to a date from one (A)
	bool IsPair() const {
		return Kind == DerivedKind::Composite || Kind == DerivedKind::Davison;
	}
};

// Charts worked out from other charts, and the calculations behind them.
struct DerivedCharts abstract final {
	// --- progressions and directions

	// the years (tropical) from one moment to another; negative going back
	static double YearsBetween(DateTime const& from, DateTime const& to);
	// The moment whose sky secondary progressions use for a target date: the birth plus as many days as the target is years after.
	static DateTime ProgressedTime(DateTime const& natal, DateTime const& target);
	// The arc, in degrees, that the solar arc directions (and, with a mean key, primary directions) move by at the target date.
	static double Arc(AstroCalculator const& calc, ChartData const& natal, DateTime const& target, ArcKey key);
	// The chart for the target date by the method. Everything but what the method moves stays as it is in the natal chart,
	// and Info().Time is the moment whose sky the planets are at: the progressed moment for secondary progressions, the
	// birth for the other two.
	static ChartData Progress(AstroCalculator& calc, ChartData const& natal, DateTime const& target, ProgressionOptions const& options = {});

	// --- returns

	// The time a planet is at a longitude, searching from a moment (any planet; a planet that turns retrograde can be there
	// more than once, and each of the times counts). Nothing if it never gets there within four centuries.
	static std::optional<DateTime> FindLongitude(AstroCalculator const& calc, Planet planet, double longitude, DateTime const& from, ReturnSearch search);
	// the same for a planet returning to where it was in a chart
	static std::optional<DateTime> FindReturn(AstroCalculator const& calc, ChartData const& natal, Planet planet, DateTime const& from, ReturnSearch search);

	// --- composite

	// The composite of two charts: each planet at the midpoint (on the shorter arc) of its two places, and houses for the
	// midpoint of the two birth places, at the midpoint of the two times. The planets are those both charts have.
	static ChartData Composite(AstroCalculator& calc, ChartData const& a, ChartData const& b, CompositeHouses method = CompositeHouses::MidpointMC);

	// The Davison relationship chart: an ordinary chart cast for the midpoint of the two births in time and in place (the
	// planets are where they really were then). It has the first chart's house system, harmonic and planets.
	static ChartData Davison(AstroCalculator& calc, ChartData const& a, ChartData const& b);
	// the midpoint of two charts' times and places, as chart details (no names, no time zone)
	static ChartInfo MidpointInfo(ChartInfo const& a, ChartInfo const& b);

	// The chart a recipe describes. The sources need only their details (their planets are calculated here).
	static ChartData Build(AstroCalculator& calc, DerivedRecipe const& recipe);

	// --- helpers

	// the houses that have this ecliptic longitude at the Midheaven, at a latitude and (for the obliquity) a moment
	static HouseData HousesFromMC(AstroCalculator const& calc, double midheaven, double latitude, DateTime const& time, HouseSystem system);
	// the house (1 to 12) a longitude falls in; 0 if the houses aren't usable
	static int HouseOf(HouseData const& houses, AstroPoint const& longitude);
	// the house of each of the planets, in their order
	static std::vector<int> HouseOverlay(HouseData const& houses, std::vector<PlanetPosition> const& planets);
};
