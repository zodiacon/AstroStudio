#include "pch.h"
#include "Aspects.h"

float aspectAngles[] = {
    0, 60, 90, 120, 180,
    30, 45, 72, 144,
    360 / 7.0f, 720 / 7.0f, 150, 135,
    40, 80,
};

AspectCalculator::AspectCalculator(AspectSettings const& settings) : m_settings(settings) {
}

std::vector<AspectData> AspectCalculator::Calculate(std::vector<PlanetPosition> const& planets) const {
    std::vector<AspectData> aspects;
    aspects.reserve(32);

    for(int i = 0; i < (int)planets.size(); i++)
        for(int j = 0; j < i; j++)
            if (i != j) {
                auto data = CalcAspect(planets[j], planets[i]);
                if (data.Type != AspectType::None)
                    aspects.push_back(std::move(data));
            }
    return aspects;
}

float AspectCalculator::GetAspectAngle(AspectType type) {
    return aspectAngles[(int)type];
}

AspectType AspectCalculator::GetAspectType(Planet p1, Planet p2, float diff, float& dist) const {
    int count = m_settings.MajorOnly ? 5 : _countof(aspectAngles);
    dist = -1;
    float extra = 0;
    if (p1 == Planet::Sun && p2 == Planet::Moon)
        extra += m_settings.SunMoonOrbAdd;
    else if (p1 == Planet::Sun)
        extra += m_settings.SunPlanetOrbAdd;
    else if (p1 == Planet::Moon)
        extra += m_settings.MoonPlanetOrbAdd;
    
    for (int i = 0; i < count; i++) {
        auto type = (AspectType)i;
        bool major = i <= (int)AspectType::Opposition;       
        auto orb = major ? m_settings.MajorAspectOrb : m_settings.MinorAspectOrb;
        if (type == AspectType::Conjunction)
            orb += m_settings.ConjunctionOrbAdd;
        orb += extra;

        dist = fabs(diff - aspectAngles[i]);
        if (dist <= orb) {
            //
            // aspect found!
            //
            return type;
        }
    }
    return AspectType::None;
}

AspectData AspectCalculator::CalcAspect(PlanetPosition p1, PlanetPosition p2) const {
    auto diff = fabs(p1.Longitude - p2.Longitude);
    if (diff > 180)
        diff = 360 - diff;
    AspectData data;
    data.Planet1 = p1;
    data.Planet2 = p2;
    data.Angle = (float)diff;
    data.Type = GetAspectType(p1.Planet, p2.Planet, (float)diff, data.Orb);
    if (data.Type != AspectType::None) {
        data.Applying = IsApplying(p1, p2, GetAspectAngle(data.Type), GetAspectAngle(data.Type));
    }
    return data;
}


bool AspectCalculator::IsApplying(PlanetPosition p1, PlanetPosition p2, double angle, double exact) noexcept {
    auto delta = .1;
    return fabs(AstroPoint::Diff(p1.Longitude + delta * p1.Speed, p2.Longitude + delta * p2.Speed) - exact)
        < fabs(AstroPoint::Diff(p1.Longitude, p2.Longitude) - angle);
}

bool AspectData::IsMajor() const noexcept {
    return Type <= AspectType::Opposition;
}

bool AspectData::IsSoft() const noexcept {
    return Type == AspectType::Sextile || Type == AspectType::Trine;
}

bool AspectData::IsHard() const noexcept {
    return Type == AspectType::Square || Type == AspectType::Opposition || Type == AspectType::SemiSquare;
}
