#pragma once

#include "ChartData.h"
#include "Aspects.h"
#include <vector>

// A planet of one chart in aspect to the Ascendant or the Midheaven of the other.
struct SynastryAngleAspect {
	int Planet{ 0 };				// index into the planets of the planet's chart
	bool Midheaven{ false };		// the other chart's Midheaven; otherwise its Ascendant
	AspectType Type{ AspectType::None };
	float Orb{ 0 };
	float MaxOrb{ 0 };				// the widest orb allowed (the aspect's own plus the planet's extra)
};

// Two charts side by side (synastry): the aspects between their planets, the planets of each to the angles of the other, and
// where each one's planets fall in the other's houses.
struct SynastryReport {
	std::vector<AspectData> Aspects;			// a's planet as Planet1, b's as Planet2; tightest first
	std::vector<SynastryAngleAspect> AToB;		// a's planets to b's angles, tightest first
	std::vector<SynastryAngleAspect> BToA;		// b's planets to a's angles, tightest first
	std::vector<int> AInB;		// for each of a's planets (in a's order), the house of b's it is in: 1-12, 0 if b has no houses
	std::vector<int> BInA;		// for each of b's planets, the house of a's
};

struct Synastry abstract final {
	// The charts' planets and houses as they are (nothing is calculated). An aspect is the first type, in AspectType order, whose
	// orb takes the pair (as AspectCalculator::CalcBetween has it); an angle has no orb of its own, so only the planet's extra
	// counts there (AspectCalculator::MaxOrbFor). A chart without houses has no angles and no house overlay.
	static SynastryReport Make(ChartData const& a, ChartData const& b, AspectSettings const& settings);
	// whether a chart has houses to speak of (a chart of an unknown time may have none worked out)
	static bool HasHouses(HouseData const& houses);
	// the planets of one chart to the angles of another (empty if it has no houses)
	static std::vector<SynastryAngleAspect> ToAngles(std::vector<PlanetPosition> const& planets, HouseData const& houses, AspectCalculator const& calculator);
};
