#pragma once

#include "AstroPoint.h"
#include "DateTime.h"

struct PlanetPosition {
	AstroPoint Longitude;
	double Speed;
	double Latitude;
	double LatitudeSpeed;
	PlanetType Planet;
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
	PlanetPosition CalcPlanet(PlanetType planet, DateTime const& dt, bool withSpeed = true) const;
	HouseData CalcHouses(DateTime dt, double latitude, double longitude, HouseSystem system);

	IngressData CalcPlanetIngress(PlanetType planet, DateTime start, bool reverse = false) const;
	StationData CalcPlanetStation(PlanetType planet, DateTime start) const;

	int Harmonic() const;
	int Harmonic(int harmonic);

	bool Calculate(ChartData& data);

	double Epsilon{ .00001 };
	int MaxIterations{ 1000 };

private:
	static inline bool s_init{ false };
	int m_SweFlags;
	int m_Harmonic{ 1 };
};

