#pragma once

#include "AstroPoint.h"
#include "DateTime.h"
#include <atomic>
#include <string>

struct PlanetPosition {
	AstroPoint Longitude;
	double Speed;
	double Latitude;
	double LatitudeSpeed;
	Planet Planet;
};

struct PlanetPhenom {
	Planet Planet;
	DateTime Time;
};

struct IngressData : PlanetPhenom {
	ZodiacSign Sign;
	bool IsRetro;
};

struct StationData : PlanetPhenom {
	AstroPoint Position;
	bool IsTurningRetrograde;
};

enum class EclipseKind {
	Total, Annular, Hybrid, Partial, Penumbral,
};

struct EclipseData {
	DateTime Maximum;		// UT of the greatest eclipse (for a solar one, the greatest anywhere on Earth)
	bool Solar;
	EclipseKind Kind;
};

// A stretch during which the Moon makes no exact major aspect (conjunction, sextile, square, trine, opposition) to any
// planet, from the last one it made in its sign until it leaves the sign.
struct VoidOfCourseData {
	DateTime Start, End;	// End is the Moon's ingress into the next sign
	ZodiacSign Sign;		// the sign the Moon is in
	bool WholeSign;			// it made no aspect at all in the sign: the void starts at the ingress into the sign
	Planet LastPlanet;		// the last aspect: with this planet
	double LastAngle;		// and of this angle (0, 60, 90, 120 or 180 degrees)
};

// The moment two bodies are exactly at an aspect to each other.
struct ExactAspectData {
	DateTime Time;			// UT
	Planet Planet1, Planet2;	// in the order of the list the aspects were looked for in
	double Angle;			// 0, 60, 90, 120 or 180 degrees
};

enum class HouseSystem {
	Placidus = 'P', 
	Koch = 'K', 
	Porphyrius = 'O', 
	Regiomontanus = 'R', 
	Campanus = 'C',
	Equal = 'A', 
	Morinus = 'M', 
	Topocentric = 'T', 
	Alcabitus = 'B', 
	Horizontal = 'H',
	Krusinski = 'U', 
	EqualWholeSign = 'W',
	CarterPoliEqu = 'F',
	EqualMC = 'D',
	Sunshine = 'I',
	SunshineAlt = 'i',
	APCHouses = 'Y',
};

struct HouseData {
	AstroPoint Asc;
	AstroPoint MC;
	AstroPoint Cusps[12];
	AstroPoint Vertex;
	AstroPoint EquAsc;
	AstroPoint Armc;
	AstroPoint CoAsc1;
	AstroPoint CoAsc2;
	AstroPoint PolarAsc;
};

class ChartData;

// The zodiac positions are measured in: the tropical one (from the equinox), or a sidereal one (from the stars), which is
// the tropical zodiac less an ayanamsa. A zodiac is a number: Tropical, or one of the ephemeris's SE_SIDM_* ayanamsas.
namespace Zodiac {
	inline constexpr int Tropical = -1;
	inline constexpr int FaganBradley = 0, Lahiri = 1, DeLuce = 2, Raman = 3, Krishnamurti = 5, Yukteshwar = 7, TrueChitra = 27;
	// the ones the program offers, in the order it offers them (Tropical first)
	inline constexpr int Offered[] = { Tropical, FaganBradley, Lahiri, Raman, Krishnamurti, Yukteshwar, TrueChitra, DeLuce };
	// "Tropical", or the ayanamsa's name as the ephemeris has it ("Lahiri")
	std::string Name(int zodiac);
	bool IsValid(int zodiac);
}

class AstroCalculator {
public:
	AstroCalculator();
	// The zodiac every calculator follows unless it was given one of its own (SetZodiac): one for the whole program, so
	// that every chart, ephemeris and analysis is in the same zodiac. Tropical to begin with.
	static void SetDefaultZodiac(int zodiac) noexcept;
	static int DefaultZodiac() noexcept;
	// This calculator's own zodiac (FollowDefault: the program's), for work that must not change zodiac halfway (a worker thread).
	static constexpr int FollowDefault = -2;
	void SetZodiac(int zodiac) noexcept;
	int GetZodiac() const noexcept;		// the zodiac in use now (never FollowDefault)
	// the ayanamsa at a moment (degrees): what is taken off a tropical longitude; 0 in the tropical zodiac
	double Ayanamsa(double jd) const;
	// what to add to a longitude in zodiac 'from' to have it in zodiac 'to', at a moment
	static double ZodiacDifference(int from, int to, double jd);

	PlanetPosition CalcPlanet(Planet planet, DateTime const& dt, int harmonic = 1, bool withSpeed = true) const;
	HouseData CalcHouses(DateTime dt, double latitude, double longitude, HouseSystem system);
	// Houses for a sidereal time at the Midheaven (armc, degrees), a latitude and the obliquity of the ecliptic - for charts
	// whose angles aren't those of a moment (directions, composites). jd is the moment whose ayanamsa a sidereal zodiac takes off.
	HouseData CalcHousesFromArmc(double armc, double latitude, double obliquity, HouseSystem system, double jd) const;
	// the true obliquity of the ecliptic (degrees) at a Julian day
	double Obliquity(double jd) const;

	IngressData CalcPlanetIngress(Planet planet, DateTime start, bool reverse = false) const;
	StationData CalcPlanetStation(Planet planet, DateTime start) const;

	// The solar and lunar eclipses whose maximum falls in [from, to), in time order.
	std::vector<EclipseData> CalcEclipses(DateTime const& from, DateTime const& to) const;
	// The void of course periods of the Moon that overlap [from, to), in time order. The aspects looked for are to the
	// Sun and the planets up to Saturn, and to Uranus, Neptune and Pluto too when outerPlanets is set.
	std::vector<VoidOfCourseData> CalcVoidOfCourse(DateTime const& from, DateTime const& to, bool outerPlanets = true) const;
	// The exact major aspects (conjunction, sextile, square, trine, opposition) between every two of the bodies in [from, to),
	// in time order. The two nodes (mean and true) and the two Liliths (mean and osculating) are not paired with each other.
	std::vector<ExactAspectData> CalcExactAspects(std::vector<Planet> const& planets, DateTime const& from, DateTime const& to) const;

	bool Calculate(ChartData& data);

	double Epsilon{ .00001 };
	int MaxIterations{ 1000 };

private:
	double Longitude(int planet, double jd) const;
	// the time the Moon changes sign after (forward) or before jd
	double MoonSignChange(double jd, bool forward) const;
	VoidOfCourseData CalcVoid(double entered, double left, bool outerPlanets) const;
	// the ephemeris flags for a position in this calculator's zodiac (and the thread's ayanamsa set to it)
	int Flags() const;

	// (the library's state is per thread, so the ephemeris path is set once for each thread that calculates)
	static inline thread_local bool s_init{ false };
	static inline thread_local int s_sidMode{ -100 };	// the ayanamsa the thread's ephemeris was last set to
	static inline std::atomic<int> s_defaultZodiac{ Zodiac::Tropical };
	int m_SweFlags;
	int m_Zodiac{ FollowDefault };
};

