#pragma once

#include "AstroCalculator.h"
#include <queue>

class ChartData {
public:
	ChartData& AddPlanets(std::initializer_list<PlanetPosition> planets);
	ChartData& RemovePlanets(std::initializer_list<PlanetType> planets);
	ChartData& Clear();
	int PlanetsCount() const;
	PlanetPosition const& Planet(int index) const;

	HouseData& Houses();
	HouseData const& Houses() const;

private:
	HouseData m_Houses{};
	std::vector<PlanetPosition> m_Planets;
};
