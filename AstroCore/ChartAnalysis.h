#pragma once

#include "Aspects.h"
#include "ChartData.h"
#include "Midpoints.h"
#include <array>
#include <optional>
#include <vector>

// The structure of a natal chart (no UI, no interpretation): the aspect patterns its planets make, how they are spread over the
// elements, modalities, hemispheres and quadrants, their essential dignities and mutual receptions, and the chains of dispositors.

enum class PatternKind {
	GrandCross,			// two oppositions square each other
	Kite,				// a grand trine with a fourth planet opposite one corner and sextile the other two
	MysticRectangle,	// two oppositions joined by two trines and two sextiles
	GrandTrine,			// three planets in trine to each other
	TSquare,			// an opposition with a planet square both ends (the apex)
	Yod,				// two planets in sextile, both quincunx a third (the apex)
	Stellium,			// several planets in one sign, or in one house
};

enum class Element { Fire, Earth, Air, Water };
enum class Modality { Cardinal, Fixed, Mutable };

enum class RulerSystem {
	Traditional,		// the seven planets rule the signs
	Modern,				// Pluto, Uranus and Neptune rule Scorpio, Aquarius and Pisces (for dignity the old rulers keep them too)
};

struct ChartAnalysisOptions {
	// the bodies that take part (those the chart has); the ten planets by default - nodes, Lilith, Chiron and the rest are left out
	std::vector<Planet> Bodies{ Planet::Sun, Planet::Moon, Planet::Mercury, Planet::Venus, Planet::Mars, Planet::Jupiter,
		Planet::Saturn, Planet::Uranus, Planet::Neptune, Planet::Pluto };
	RulerSystem Rulers{ RulerSystem::Traditional };
	// the Sun, the Moon, the Ascendant and the Midheaven count twice in the balance; otherwise every point counts once
	bool Weighted{ true };
	// the fewest bodies in one sign (or house) that make a stellium
	int StelliumSize{ 3 };
	// a planet within this many degrees of the Ascendant, Descendant, Midheaven or IC is on that angle
	double AngleOrb{ 8 };
};

// Marc Edmund Jones's shapes: how the planets (the ten) are spread round the wheel.
enum class ChartShape {
	Bundle,			// all within a trine (120 degrees)
	Bowl,			// all within an opposition (180 degrees)
	Bucket,			// a bowl and a single planet (the handle) alone in the empty half
	Locomotive,		// an empty trine: all within 240 degrees, give or take ten (an empty space of 110 degrees or more)
	Seesaw,			// two groups facing each other across two empty spaces of 60 degrees or more (and no third)
	Splash,			// spread round the whole wheel, no empty space of 60 degrees, at least seven signs taken
	Splay,			// none of these: irregular clusters (three or more empty spaces of 60 degrees, or too few signs for a splash)
};

struct ShapeData {
	ChartShape Shape;
	// the planet that leads the group (the one rising first, just after the empty space in the order of the signs): bundle, bowl,
	// bucket and locomotive
	std::optional<Planet> Leading;
	std::optional<Planet> Handle;		// a bucket's
	double Span{ 0 };					// the arc the planets take up (360 less the largest empty space)
	double LargestGap{ 0 };
};

// the four angles of a chart
enum class AngleKind { Ascendant, Midheaven, Descendant, Imum };

struct AngularPlanet {
	Planet Body;
	AngleKind Angle;
	double Orb;
};

struct AspectPattern {
	PatternKind Kind;
	// the planets of the pattern, in Planet order (the apex among them)
	std::vector<Planet> Planets;
	// the focal planet: the apex of a T-square or a yod, the corner of a kite's grand trine opposite its fourth planet
	std::optional<Planet> Apex;
	// the aspects that make the pattern (none for a stellium)
	std::vector<AspectData> Aspects;
	// the loosest of those aspects (0 for a stellium)
	float MaxOrb{ 0 };
	// a grand trine whose planets are all in signs of one element; a stellium in one sign has its sign's element
	std::optional<::Element> Element;
	// a T-square or grand cross whose planets are all in signs of one modality; a stellium in one sign has its sign's modality
	std::optional<::Modality> Modality;
	// a grand trine, T-square or grand cross whose planets are not all in signs of one element (modality): made across the signs
	bool OutOfSign{ false };
	// a stellium: in this sign (and, if all its planets are in one house, that house), or in this house only
	std::optional<ZodiacSign> Sign;
	int House{ 0 };
};

// How the points are spread: one of the kinds of division of the chart.
enum class BalanceKind {
	Element,		// groups: Fire, Earth, Air, Water
	Modality,		// Cardinal, Fixed, Mutable
	Hemisphere,		// Above the horizon (houses 7-12), Below (1-6)
	Side,			// Eastern (houses 10-12 and 1-3), Western (4-9)
	Quadrant,		// houses 1-3, 4-6, 7-9, 10-12
	Angularity,		// angular (houses 1, 4, 7, 10), succedent (2, 5, 8, 11), cadent (3, 6, 9, 12)
	House,			// the twelve houses
};

struct BalancePoint {
	PointKind Kind{ PointKind::Planet };		// the Ascendant and the Midheaven count in the elements and modalities only
	Planet Body{ Planet::Sun };
	double Weight{ 1 };
};

struct BalanceGroup {
	std::vector<BalancePoint> Points;
	double Score{ 0 };		// the sum of the weights
	// the planets in it (not the angles)
	int Planets() const;
};

struct BalanceSet {
	BalanceKind Kind;
	std::vector<BalanceGroup> Groups;
	// the indexes of the groups with the highest score (several if they tie)
	std::vector<int> Dominant;
	// the groups with no planet in them (an angle alone doesn't fill one), and those with exactly one
	std::vector<int> Missing;
	std::vector<int> Singletons;
};

enum class Dignity { Domicile, Exaltation, Triplicity, Term, Face, Detriment, Fall };

// The essential dignities of a planet where it is. The minor ones are the seven planets' only: triplicity (Dorothean: the day
// ruler in a day chart, the night ruler in a night chart, and the participating ruler in either; with the sect unknown - no houses -
// both the day and the night ruler), the Egyptian terms and the faces (decans) in the Chaldean order.
struct DignityData {
	Planet Body;
	ZodiacSign Sign;
	std::vector<Dignity> Dignities;		// in the order of the enum (Mercury in Virgo is in both its domicile and its exaltation)
	// Lilly's: domicile +5, exaltation +4, triplicity +3, term +2, face +1, detriment -5, fall -4, peregrine -5
	int Score{ 0 };
	bool Peregrine{ false };			// one of the seven planets with no dignity at all (domicile to face)
	// whose term and face the planet is in, and the rulers of its sign's triplicity (day, night, participating)
	Planet TermRuler{ Planet::Sun };
	Planet FaceRuler{ Planet::Sun };
	std::array<Planet, 3> TriplicityRulers{};
};

// Two planets each in a sign the other rules (or, if Exaltation, each in the other's exaltation).
struct Reception {
	Planet A, B;		// A before B in Planet order
	bool Exaltation{ false };
};

struct DispositorNode {
	Planet Body;
	Planet Ruler;			// the planet that rules the sign Body is in
	bool RulerInChart;		// whether that planet takes part (a chain stops where it doesn't)
};

struct Dispositors {
	std::vector<DispositorNode> Nodes;		// one for every body that takes part, in their order
	// where the chains end: each a cycle of planets - one planet in its own sign, or a loop of two or more (Venus in Aries, Mars in
	// Libra) - in the order the chain goes round
	std::vector<std::vector<Planet>> Ends;
	// the planet every chain ends in, if there is one: it is in its own sign and nothing ends anywhere else
	std::optional<Planet> Final;
};

struct ChartRuler {
	Planet Body;					// the ruler of the Ascendant's sign
	ZodiacSign AscendantSign;
	std::optional<ZodiacSign> Sign;		// where the ruler is (nothing if the chart doesn't have it)
	int House{ 0 };
};

struct ChartAnalysisResult {
	std::vector<AspectPattern> Patterns;
	BalanceSet Elements, Modalities;
	// only for a chart with houses (empty groups otherwise)
	BalanceSet Hemispheres, Sides, Quadrants, Angularity, Houses;
	bool HasHouses{ false };
	// the chart's sect, by the Sun above or below the horizon (nothing without houses, or without the Sun)
	std::optional<bool> DayChart;
	std::vector<DignityData> Dignities;
	std::vector<Reception> Receptions;
	::Dispositors Dispositors;
	std::optional<ChartRuler> Ruler;
	// how the planets are spread (nothing with fewer than seven of them)
	std::optional<ShapeData> Shape;
	// the planets that make no major aspect (conjunction to opposition) with another
	std::vector<Planet> Unaspected;
	// the planets on an angle, tightest first (only with houses)
	std::vector<AngularPlanet> OnAngles;
	std::vector<Planet> Retrograde;
};

class ChartAnalysis final {
public:
	// The whole analysis of a chart; the aspects are those the calculator finds between the bodies that take part (so the aspect
	// settings decide which patterns can appear - a yod needs the quincunx).
	static ChartAnalysisResult Run(ChartData const& chart, AspectCalculator const& calculator, ChartAnalysisOptions const& options = {});

	static std::vector<AspectPattern> Patterns(ChartData const& chart, std::vector<PlanetPosition> const& bodies,
		AspectCalculator const& calculator, int stelliumSize = 3);
	static ::Element ElementOf(ZodiacSign sign);
	static ::Modality ModalityOf(ZodiacSign sign);

	// the planets that rule a sign: the traditional one, and with modern rulers also the modern one if it is another
	static std::vector<Planet> DomicileRulers(ZodiacSign sign, RulerSystem rulers);
	// the sign a planet is exalted in (the seven planets only)
	static std::optional<ZodiacSign> Exaltation(Planet planet);
	// the dignities of a planet at a longitude; day: the chart's sect (nothing: unknown)
	static DignityData DignityOf(Planet planet, AstroPoint const& longitude, RulerSystem rulers, std::optional<bool> day = true);
	// the planet whose Egyptian term (bound) a longitude is in, and whose face (decan, in the Chaldean order from Mars at 0 Aries)
	static Planet TermRuler(AstroPoint const& longitude);
	static Planet FaceRuler(AstroPoint const& longitude);
	// the Dorothean rulers of an element's triplicity: by day, by night, participating
	static std::array<Planet, 3> TriplicityRulers(::Element element);
	// the shape of a set of longitudes (at least seven)
	static std::optional<ShapeData> ShapeOf(std::vector<PlanetPosition> const& bodies);
	// whether the dignities of a planet are looked at with these rulers: the seven, and the three modern rulers with modern rulers
	static bool HasDignities(Planet planet, RulerSystem rulers);
};
