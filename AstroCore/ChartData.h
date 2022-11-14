#pragma once

#include "AstroCalculator.h"

enum class InfoType {
	Unknown,
	Male,
	Female,
	Event,
};

struct TimeZoneInfo {
	std::wstring Name;
	int OffsetUT;	// minutes
};

struct ChartInfo {
	std::wstring FirstName, LastName;
	InfoType Type{ InfoType::Unknown };
	double Longitude, Latitude, Elevation;
	std::wstring Country, State, City;
	DateTime Time;
	TimeZoneInfo TimeZone;
};
	
class ChartData {
public:
	ChartData& AddPlanets(std::initializer_list<PlanetPosition> const& planets);
	ChartData& AddPlanets(std::initializer_list<PlanetType> const& planets);
	ChartData& AddPlanets(std::vector<PlanetType> const& planets);
	ChartData& RemovePlanets(std::initializer_list<PlanetType> planets);
	ChartData& Clear();
	int PlanetCount() const;
	PlanetPosition const& Planet(int index) const;
	std::vector<PlanetPosition> const& AllPlanets() const;
	std::vector<PlanetPosition>& AllPlanets();

	int Harmonic() const;
	int Harmonic(int harmonic);

	void SetHouseSystem(HouseSystem system);
	HouseSystem GetHouseSystem() const;
	HouseData const& Houses() const;
	HouseData& Houses();
	ChartInfo& Info();
	ChartInfo const& Info() const;

	void CalcHouses(AstroCalculator& calc);
	void CalcPlanets(AstroCalculator& calc);

private:
	HouseSystem m_HouseSystem{ HouseSystem::Koch };
	HouseData m_Houses{};
	std::vector<PlanetPosition> m_Planets;
	int m_Harmonic{ 1 };
	ChartInfo m_Info{};
};
