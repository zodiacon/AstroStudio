#include "pch.h"
#include "ChartData.h"
#include <assert.h>

ChartData& ChartData::AddPlanets(std::initializer_list<PlanetPosition> const& planets) {
    m_Planets.insert(m_Planets.end(), planets.begin(), planets.end());
    return *this;
}

ChartData& ChartData::AddPlanets(std::vector<Planet> const& planets) {
    for (auto& p : planets)
        m_Planets.push_back(PlanetPosition{ .Planet = p });
    return *this;
}

ChartData& ChartData::AddPlanets(std::initializer_list<Planet> const& planets) {
    for (auto& p : planets)
        m_Planets.push_back(PlanetPosition{ .Planet = p });
    return *this;
}

ChartData& ChartData::RemovePlanets(std::initializer_list<Planet> planets) {
    for (auto planet : planets)
        m_Planets.erase(std::find_if(m_Planets.begin(), m_Planets.end(), [&](auto& pp) { return pp.Planet == planet; }));
    return *this;
}

HouseData const& ChartData::Houses() const {
    return m_Houses;
}

HouseData& ChartData::Houses() {
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

PlanetPosition const& ChartData::GetPlanet(int index) const {
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

int ChartData::Harmonic() const {
    return m_Harmonic;
}

int ChartData::Harmonic(int harmonic) {
    if (harmonic > 0 && harmonic < 10000)
        m_Harmonic = harmonic;
    return m_Harmonic;
}

void ChartData::CalcHouses(AstroCalculator& calc) {
    m_Houses = calc.CalcHouses(m_Info.Time, m_Info.Latitude, m_Info.Longitude, m_HouseSystem);
}

void ChartData::CalcPlanets(AstroCalculator& calc) {
    for (auto& p : m_Planets)
        p = calc.CalcPlanet(p.Planet, m_Info.Time, m_Harmonic);
}
