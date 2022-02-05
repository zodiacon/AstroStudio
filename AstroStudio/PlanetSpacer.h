#pragma once

#include "AstroCalculator.h"

class PlanetSpacer final {
public:
    explicit PlanetSpacer(std::vector<PlanetPosition> const& positions);

    void Space(double minDegrees = 4.0);

    std::vector<PlanetPosition> const& NewPositions() const;

private:
    std::vector<PlanetPosition> m_positions;
    std::vector<PlanetPosition> m_newPositions;

};

