#pragma once

#include "AstroCalculator.h"

//
// Copied unchanged from AstroStudio\PlanetSpacer.h - it is pure geometry with
// no ATL or GDI+ in it, so it is the one UI-layer file that ports verbatim.
// Nudges apart planets that would otherwise overlap on the wheel.
//
class PlanetSpacer final {
public:
	explicit PlanetSpacer(std::vector<PlanetPosition> const& positions);

	void Space(double minDegrees = 4.0);

	std::vector<PlanetPosition> const& NewPositions() const;

private:
	std::vector<PlanetPosition> m_positions;
	std::vector<PlanetPosition> m_newPositions;
};
