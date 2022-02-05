#include "pch.h"
#include "Aspects.h"

double aspectAngles[] = {
    0, 60, 90, 120, 180,
    30, 45, 72, 144,
    360 / 7.0, 720 / 7.0, 150, 135
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

double AspectCalculator::GetAspectAngle(AspectType type) {
    return aspectAngles[(int)type];
}

AspectType AspectCalculator::GetAspectType(PlanetType p1, PlanetType p2, double diff, double& dist) const {
    int count = m_settings.MajorOnly ? 5 : _countof(aspectAngles);
    dist = -1;
    double extra = 0;
    if (p1 == PlanetType::Sun && p2 == PlanetType::Moon)
        extra += m_settings.SunMoonOrbAdd;
    else if (p1 == PlanetType::Sun)
        extra += m_settings.SunPlanetOrbAdd;
    else if (p1 == PlanetType::Moon)
        extra += m_settings.MoonPlanetOrbAdd;
    
    for (int i = 0; i < count; i++) {
        auto type = (AspectType)i;
        bool major = i <= (int)AspectType::Opposition;       
        auto orb = major ? m_settings.MajorAspectOrb : m_settings.MinorAspectOrb;
        if (type == AspectType::Conjunction)
            orb += m_settings.ConjuntionOrbAdd;
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
    data.Angle = diff;
    data.Type = GetAspectType(p1.Planet, p2.Planet, diff, data.Orb);
    if (data.Type != AspectType::None) {
        data.Applying = IsApplying(p1, p2, GetAspectAngle(data.Type), GetAspectAngle(data.Type));
    }
    return data;
}


bool AspectCalculator::IsApplying(PlanetPosition p1, PlanetPosition p2, double angle, double exact) const {
    auto delta = .1;
    return fabs(AstroPoint::Diff(p1.Longitude + delta * p1.Speed, p2.Longitude + delta * p2.Speed) - exact)
        < fabs(AstroPoint::Diff(p1.Longitude, p2.Longitude) - angle);
}

bool AspectData::IsMajor() const {
    return Type <= AspectType::Opposition;
}

bool AspectData::IsSoft() const {
    return Type == AspectType::Sextile || Type == AspectType::Trine || Type == AspectType::SemiSextile;
}

bool AspectData::IsHard() const {
    return Type == AspectType::Square || Type == AspectType::Opposition || Type == AspectType::SemiSquare;
}
