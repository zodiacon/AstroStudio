#pragma once

#include "AstroCalculator.h"

enum class InfoType {
	Unknown,
	Male,
	Female,
	Event,
};

// How the wall-clock time of a chart relates to UT (ChartInfo::Time is always UT).
struct TimeZoneInfo {
	std::wstring Name;		// Windows time zone key, e.g. "Eastern Standard Time"; empty for a manual offset
	int OffsetUT{ 0 };		// minutes east of UT in effect at the chart's time (DST included); the offset itself when Name is empty
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
