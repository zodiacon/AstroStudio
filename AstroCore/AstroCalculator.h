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

	IngressData CalcPlanetIngress(Planet planet, DateTime start, bool reverse = false) const;
	StationData CalcPlanetStation(Planet planet, DateTime start) const;

	bool Calculate(ChartData& data);

	double Epsilon{ .00001 };
	int MaxIterations{ 1000 };

private:
	static inline bool s_init{ false };
	int m_SweFlags;
};

