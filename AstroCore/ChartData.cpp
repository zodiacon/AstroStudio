#include "pch.h"
#include "ChartData.h"
#include "ArabicParts.h"
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
        if (auto it = std::find_if(m_Planets.begin(), m_Planets.end(), [&](auto& pp) { return pp.Planet == planet; }); it != m_Planets.end())
            m_Planets.erase(it);
    return *this;
}

HouseData const& ChartData::Houses() const noexcept {
    return m_Houses;
}

HouseData& ChartData::Houses() noexcept {
    return m_Houses;
}

ChartInfo& ChartData::Info() noexcept {
    return m_Info;
}

ChartInfo const& ChartData::Info() const noexcept {
    return m_Info;
}

ChartData& ChartData::Clear() noexcept {
    m_Planets.clear();
    return *this;
}

int ChartData::PlanetCount() const noexcept {
    return (int)m_Planets.size();
}

PlanetPosition const& ChartData::GetPlanet(int index) const noexcept {
    assert(index >= 0 && index < m_Planets.size());
    return m_Planets[index];
}

std::vector<PlanetPosition> const& ChartData::AllPlanets() const noexcept {
    return m_Planets;
}

std::vector<PlanetPosition>& ChartData::AllPlanets() noexcept {
    return m_Planets;
}

HouseSystem ChartData::GetHouseSystem() const noexcept {
    return m_HouseSystem;
}

void ChartData::SetHouseSystem(HouseSystem system) noexcept {
    m_HouseSystem = system;
}

int ChartData::Harmonic() const noexcept {
    return m_Harmonic;
}

int ChartData::Harmonic(int harmonic) noexcept {
    if (harmonic > 0 && harmonic < 10000)
        m_Harmonic = harmonic;
    return m_Harmonic;
}

void ChartData::CalcHouses(AstroCalculator& calc) noexcept {
    m_Houses = calc.CalcHouses(m_Info.Time, m_Info.Latitude, m_Info.Longitude, m_HouseSystem);
    UpdatePartOfFortune(&calc);
}

void ChartData::CalcPlanets(AstroCalculator& calc) noexcept {
    for (auto& p : m_Planets)
        p = calc.CalcPlanet(p.Planet, m_Info.Time, m_Harmonic);
    UpdatePartOfFortune(&calc);
}

void ChartData::UpdatePartOfFortune(AstroCalculator const* calc) noexcept {
    auto find = [&](Planet planet) {
        return std::find_if(m_Planets.begin(), m_Planets.end(), [&](auto const& p) { return p.Planet == planet; });
    };
    auto fortune = find(Planet::PartOfFortune), sun = find(Planet::Sun), moon = find(Planet::Moon);
    if (fortune == m_Planets.end() || sun == m_Planets.end() || moon == m_Planets.end())
        return;

    // the sect by where the Sun really is: a harmonic chart has its planets multiplied
    AstroPoint realSun = sun->Longitude;
    if (m_Harmonic > 1 && calc)
        realSun = calc->CalcPlanet(Planet::Sun, m_Info.Time, 1, false).Longitude;
    bool day = ArabicParts::IsDay(realSun, m_Houses.Asc);
    double asc = m_Houses.Asc.Value * m_Harmonic;
    double value = day ? asc + moon->Longitude.Value - sun->Longitude.Value : asc + sun->Longitude.Value - moon->Longitude.Value;
    *fortune = PlanetPosition{ .Longitude = AstroPoint(value), .Planet = Planet::PartOfFortune };
}
