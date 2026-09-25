#pragma once

#include "AstroPoint.h"
#include "DateTime.h"

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

class AstroCalculator {
public:
	AstroCalculator();
	PlanetPosition CalcPlanet(Planet planet, DateTime const& dt, int harmonic = 1, bool withSpeed = true) const;
	HouseData CalcHouses(DateTime dt, double latitude, double longitude, HouseSystem system);
	// Houses for a sidereal time at the Midheaven (armc, degrees), a latitude and the obliquity of the ecliptic - for charts
	// whose angles aren't those of a moment (directions, composites).
	HouseData CalcHousesFromArmc(double armc, double latitude, double obliquity, HouseSystem system) const;
	// the true obliquity of the ecliptic (degrees) at a Julian day
	double Obliquity(double jd) const;

	IngressData CalcPlanetIngress(Planet planet, DateTime start, bool reverse = false) const;
	StationData CalcPlanetStation(Planet planet, DateTime start) const;

	// The solar and lunar eclipses whose maximum falls in [from, to), in time order.
	std::vector<EclipseData> CalcEclipses(DateTime const& from, DateTime const& to) const;
	// The void of course periods of the Moon that overlap [from, to), in time order. The aspects looked for are to the
	// Sun and the planets up to Saturn, and to Uranus, Neptune and Pluto too when outerPlanets is set.
	std::vector<VoidOfCourseData> CalcVoidOfCourse(DateTime const& from, DateTime const& to, bool outerPlanets = true) const;

	bool Calculate(ChartData& data);

	double Epsilon{ .00001 };
	int MaxIterations{ 1000 };

private:
	double Longitude(int planet, double jd) const;
	// the time the Moon changes sign after (forward) or before jd
	double MoonSignChange(double jd, bool forward) const;
	VoidOfCourseData CalcVoid(double entered, double left, bool outerPlanets) const;

	static inline bool s_init{ false };
	int m_SweFlags;
};

