#include "pch.h"
#include "Aspects.h"

float aspectAngles[] = {
    0, 60, 90, 120, 180,
    30, 45, 72, 144,
    360 / 7.0f, 720 / 7.0f, 150, 135,
    40, 80,
};

AspectSettings::AspectSettings() {
	AspectEnabled.fill(true);
	AspectOrb.fill(-1);
	PlanetEnabled.fill(true);
	PlanetEnabled[static_cast<int>(Planet::PartOfFortune)] = false;		// (a point, not a body: it makes no aspects unless it is asked to)
	PlanetOrbAdd.fill(0);
}

AspectSettings& AspectSettings::CustomOrb(AspectType type, double orb) {
	if (int i = static_cast<int>(type); i >= 0 && i < AspectTypeCount)
		AspectOrb[i] = static_cast<float>(orb);
	return *this;
}

float AspectSettings::OrbFor(AspectType type) const {
	int i = static_cast<int>(type);
	if (i < 0 || i >= AspectTypeCount)
		return 0;
	if (AspectOrb[i] >= 0)
		return AspectOrb[i];
	bool major = type <= AspectType::Opposition;
	float orb = major ? MajorAspectOrb : MinorAspectOrb;
	return type == AspectType::Conjunction ? orb + ConjunctionOrbAdd : orb;
}

bool AspectSettings::IsEnabled(Planet planet) const {
	int i = static_cast<int>(planet);
	return i < 0 || i >= PlanetCount || PlanetEnabled[i];
}

float AspectSettings::OrbAdd(Planet planet) const {
	int i = static_cast<int>(planet);
	return i < 0 || i >= PlanetCount ? 0 : PlanetOrbAdd[i];
}

AspectCalculator::AspectCalculator(AspectSettings const& settings) : m_settings(settings) {
}

std::vector<AspectData> AspectCalculator::Calculate(std::vector<PlanetPosition> const& planets) const {
    std::vector<AspectData> aspects;
    aspects.reserve(32);

    for(int i = 0; i < (int)planets.size(); i++)
        for(int j = 0; j < i; j++)
            if (i != j && m_settings.IsEnabled(planets[i].Planet) && m_settings.IsEnabled(planets[j].Planet)) {
                auto data = CalcAspect(planets[j], planets[i]);
                if (data.Type != AspectType::None)
                    aspects.push_back(std::move(data));
            }
    return aspects;
}

std::vector<AspectData> AspectCalculator::CalcBetween(std::vector<PlanetPosition> const& a, std::vector<PlanetPosition> const& b) const {
    std::vector<AspectData> aspects;
    for (auto const& first : a)
        for (auto const& second : b)
            if (auto data = CalcAspect(first, second); data.Type != AspectType::None)
                aspects.push_back(std::move(data));
    return aspects;
}

float AspectCalculator::GetAspectAngle(AspectType type) {
    return aspectAngles[(int)type];
}

AspectType AspectCalculator::GetAspectType(Planet p1, Planet p2, float diff, float& dist, float* maxOrb) const {
    int count = m_settings.MajorOnly ? 5 : _countof(aspectAngles);
    dist = -1;
    if (!m_settings.IsEnabled(p1) || !m_settings.IsEnabled(p2))
        return AspectType::None;
    float extra = std::max(m_settings.OrbAdd(p1), m_settings.OrbAdd(p2));
    if (p1 == Planet::Sun && p2 == Planet::Moon)
        extra += m_settings.SunMoonOrbAdd;
    else if (p1 == Planet::Sun)
        extra += m_settings.SunPlanetOrbAdd;
    else if (p1 == Planet::Moon)
        extra += m_settings.MoonPlanetOrbAdd;
    
    for (int i = 0; i < count; i++) {
        auto type = (AspectType)i;
        if (!m_settings.AspectEnabled[i])
            continue;
        auto orb = m_settings.OrbFor(type) + extra;

        dist = fabs(diff - aspectAngles[i]);
        if (dist <= orb) {
            if (maxOrb)
                *maxOrb = orb;
            //
            // aspect found!
            //
            return type;
        }
    }
    return AspectType::None;
}

float AspectCalculator::MaxOrbFor(AspectType type, Planet p1, std::optional<Planet> p2) const {
    int i = static_cast<int>(type);
    if (i < 0 || i >= AspectSettings::AspectTypeCount || (m_settings.MajorOnly && i >= 5) || !m_settings.AspectEnabled[i])
        return -1;
    if (!m_settings.IsEnabled(p1) || (p2 && !m_settings.IsEnabled(*p2)))
        return -1;
    float extra = m_settings.OrbAdd(p1);
    if (p2) {
        extra = std::max(extra, m_settings.OrbAdd(*p2));
        if (p1 == Planet::Sun && *p2 == Planet::Moon)
            extra += m_settings.SunMoonOrbAdd;
        else if (p1 == Planet::Sun)
            extra += m_settings.SunPlanetOrbAdd;
        else if (p1 == Planet::Moon)
            extra += m_settings.MoonPlanetOrbAdd;
    }
    return m_settings.OrbFor(type) + extra;
}

AspectData AspectCalculator::CalcAspect(PlanetPosition p1, PlanetPosition p2) const {
    auto diff = fabs(p1.Longitude - p2.Longitude);
    if (diff > 180)
        diff = 360 - diff;
    AspectData data;
    data.Planet1 = p1;
    data.Planet2 = p2;
    data.Angle = (float)diff;
    data.Type = GetAspectType(p1.Planet, p2.Planet, (float)diff, data.Orb, &data.MaxOrb);
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
