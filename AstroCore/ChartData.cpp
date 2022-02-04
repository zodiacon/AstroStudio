#include "pch.h"
#include "ChartData.h"
#include <assert.h>

ChartData& ChartData::AddPlanets(std::initializer_list<PlanetPosition> planets) {
    m_Planets.insert(m_Planets.end(), planets.begin(), planets.end());
    return *this;
}

ChartData& ChartData::RemovePlanets(std::initializer_list<PlanetType> planets) {
    for (auto planet : planets)
        m_Planets.erase(std::find_if(m_Planets.begin(), m_Planets.end(), [&](auto& pp) { return pp.Planet == planet; }));
    return *this;
}

HouseData& ChartData::Houses() {
    return m_Houses;
}

HouseData const& ChartData::Houses() const {
    return m_Houses;
}

ChartData& ChartData::Clear() {
    m_Planets.clear();
    return *this;
}

int ChartData::PlanetsCount() const {
    return (int)m_Planets.size();
}

PlanetPosition const& ChartData::Planet(int index) const {
    assert(index >= 0 && index < m_Planets.size());
    return m_Planets[index];
}
