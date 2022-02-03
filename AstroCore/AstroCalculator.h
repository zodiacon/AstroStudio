#pragma once

#include "AstroPoint.h"
#include "DateTime.h"

struct PlanetPosition {
	AstroPoint Longitude;
	double Speed;
	double Latitude;
	double LatitudeSpeed;
};

struct PlanetPhenom {
	PlanetType Planet;
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
	Placidus = 'P', Koch = 'K', Porphyrius = 'O', Regiomontanus = 'R', Campanus = 'C',
	Equal = 'E', Morinus = 'M', Topocentric = 'T', Alcabitus = 'B', Horizontal = 'H'
};

struct HouseData {
	AstroPoint Asc;
	AstroPoint MC;
	AstroPoint Cusps[13];
	AstroPoint Vertex;
	AstroPoint EquAsc;
	AstroPoint Armc;
	AstroPoint CoAsc1;
	AstroPoint CoAsc2;
	AstroPoint PolarAsc;
};

class AstroCalculator {
public:
	AstroCalculator();
	PlanetPosition CalcPlanet(PlanetType type, DateTime const& dt, bool withSpeed = true) const;
	static HouseData CalcHouses(DateTime dt, double latitude, double longitude, HouseSystem system);

	IngressData CalcPlanetIngress(PlanetType planet, DateTime start, bool reverse = false) const;
	StationData CalcPlanetStation(PlanetType planet, DateTime start) const;

	double Epsilon{ .00001 };
	int MaxIterations{ 1000 };

private:
	static inline bool s_init{ false };
	int m_SweFlags;
};

