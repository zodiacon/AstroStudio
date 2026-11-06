#include "pch.h"
#include "Synastry.h"
#include "DerivedCharts.h"
#include <algorithm>
#include <cmath>

bool Synastry::HasHouses(HouseData const& houses) {
	for (int i = 0; i < 12; i++)
		if (houses.Cusps[i].Value != houses.Cusps[(i + 1) % 12].Value)
			return true;
	return false;
}

std::vector<SynastryAngleAspect> Synastry::ToAngles(std::vector<PlanetPosition> const& planets, HouseData const& houses, AspectCalculator const& calculator) {
	std::vector<SynastryAngleAspect> result;
	if (!HasHouses(houses))
		return result;
	for (int i = 0; i < static_cast<int>(planets.size()); i++) {
		for (bool mc : { false, true }) {
			auto diff = static_cast<float>(AstroPoint::Diff(planets[i].Longitude, mc ? houses.MC : houses.Asc));
			for (int type = 0; type < AspectSettings::AspectTypeCount; type++) {
				auto maxOrb = calculator.MaxOrbFor(static_cast<AspectType>(type), planets[i].Planet, std::nullopt);
				if (maxOrb < 0)
					continue;
				auto orb = std::fabs(diff - AspectCalculator::GetAspectAngle(static_cast<AspectType>(type)));
				if (orb <= maxOrb) {
					result.push_back({ i, mc, static_cast<AspectType>(type), orb, maxOrb });
					break;
				}
			}
		}
	}
	std::stable_sort(result.begin(), result.end(), [](auto const& x, auto const& y) { return x.Orb < y.Orb; });
	return result;
}

SynastryReport Synastry::Make(ChartData const& a, ChartData const& b, AspectSettings const& settings) {
	AspectCalculator calculator(settings);
	SynastryReport report;
	report.Aspects = calculator.CalcBetween(a.AllPlanets(), b.AllPlanets());
	std::stable_sort(report.Aspects.begin(), report.Aspects.end(), [](auto const& x, auto const& y) { return x.Orb < y.Orb; });
	report.AToB = ToAngles(a.AllPlanets(), b.Houses(), calculator);
	report.BToA = ToAngles(b.AllPlanets(), a.Houses(), calculator);
	auto overlay = [](ChartData const& planets, ChartData const& houses) {
		return HasHouses(houses.Houses()) ? DerivedCharts::HouseOverlay(houses.Houses(), planets.AllPlanets()) : std::vector<int>(planets.AllPlanets().size(), 0);
	};
	report.AInB = overlay(a, b);
	report.BInA = overlay(b, a);
	return report;
}
