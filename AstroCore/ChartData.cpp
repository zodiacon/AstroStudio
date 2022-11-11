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

void ChartData::Houses(HouseData const& houses) {
    m_Houses = houses;
}

HouseData const& ChartData::Houses() const {
    return m_Houses;
}

ChartInfo& ChartData::Info() {
    return m_Info;
}

ChartInfo const& ChartData::Info() const {
    return m_Info;
}

ChartData& ChartData::Clear() {
    m_Planets.clear();
    return *this;
}

int ChartData::PlanetCount() const {
    return (int)m_Planets.size();
}

PlanetPosition const& ChartData::Planet(int index) const {
    assert(index >= 0 && index < m_Planets.size());
    return m_Planets[index];
}

std::vector<PlanetPosition> const& ChartData::AllPlanets() const {
    return m_Planets;
}

std::vector<PlanetPosition>& ChartData::AllPlanets() {
    return m_Planets;
}

HouseSystem ChartData::GetHouseSystem() const {
    return m_HouseSystem;
}

void ChartData::SetHouseSystem(HouseSystem system) {
    m_HouseSystem = system;
}
