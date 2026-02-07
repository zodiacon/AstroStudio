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
	std::wstring FirstName, MiddleName, LastName;
	InfoType Type{ InfoType::Unknown };
	double Longitude, Latitude, Elevation;
	std::wstring Country, State, City;
	DateTime Time;
	TimeZoneInfo TimeZone;
};
	
class ChartData {
public:
	ChartData& AddPlanets(std::initializer_list<PlanetPosition> const& planets);
	ChartData& AddPlanets(std::initializer_list<Planet> const& planets);
	ChartData& AddPlanets(std::vector<Planet> const& planets);
	ChartData& RemovePlanets(std::initializer_list<Planet> planets);
	ChartData& Clear() noexcept;
	int PlanetCount() const noexcept;
	PlanetPosition const& GetPlanet(int index) const noexcept;
	std::vector<PlanetPosition> const& AllPlanets() const noexcept;
	std::vector<PlanetPosition>& AllPlanets() noexcept;

	int Harmonic() const noexcept;
	int Harmonic(int harmonic) noexcept;

	void SetHouseSystem(HouseSystem system) noexcept;
	HouseSystem GetHouseSystem() const noexcept;
	HouseData const& Houses() const noexcept;
	HouseData& Houses() noexcept;
	ChartInfo& Info() noexcept;
	ChartInfo const& Info() const noexcept;

	void CalcHouses(AstroCalculator& calc) noexcept;
	void CalcPlanets(AstroCalculator& calc) noexcept;

private:
	HouseSystem m_HouseSystem{ HouseSystem::Koch };
	HouseData m_Houses{};
	std::vector<PlanetPosition> m_Planets;
	int m_Harmonic{ 1 };
	ChartInfo m_Info{};
};
