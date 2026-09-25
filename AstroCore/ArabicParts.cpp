#include "pch.h"
#include "ArabicParts.h"
#include "DerivedCharts.h"
#include <algorithm>
#include <cmath>

namespace {
	const wchar_t* const PlanetNames[] = {
		L"Sun", L"Moon", L"Mercury", L"Venus", L"Mars", L"Jupiter", L"Saturn", L"Uranus", L"Neptune", L"Pluto",
		L"Mean Node", L"True Node", L"Lilith", L"Osculating Apogee", L"Earth", L"Chiron", L"Pholus", L"Ceres", L"Pallas", L"Juno", L"Vesta",
	};
	static_assert(_countof(PlanetNames) == static_cast<int>(Planet::NumPlanets));

	// "from X to Y": Asc + Y - X, reversed at night
	PartDefinition FromTo(PCWSTR name, PartPoint from, PartPoint to) {
		return { name, PartPoint::Asc(), to, from, PartReversal::AtNight };
	}
}

std::vector<PartDefinition> const& ArabicParts::Standard() {
	static const std::vector<PartDefinition> parts = {
		FromTo(L"Fortune", PartPoint::Of(Planet::Sun), PartPoint::Of(Planet::Moon)),
		FromTo(L"Spirit", PartPoint::Of(Planet::Moon), PartPoint::Of(Planet::Sun)),
		FromTo(L"Eros", PartPoint::OtherPart(L"Spirit"), PartPoint::Of(Planet::Venus)),
		FromTo(L"Courage", PartPoint::OtherPart(L"Fortune"), PartPoint::Of(Planet::Mars)),
		FromTo(L"Victory", PartPoint::OtherPart(L"Spirit"), PartPoint::Of(Planet::Jupiter)),
		FromTo(L"Nemesis", PartPoint::OtherPart(L"Fortune"), PartPoint::Of(Planet::Saturn)),
		FromTo(L"Father", PartPoint::Of(Planet::Sun), PartPoint::Of(Planet::Saturn)),
		FromTo(L"Mother", PartPoint::Of(Planet::Moon), PartPoint::Of(Planet::Venus)),
		{ L"Marriage", PartPoint::Asc(), PartPoint::Cusp(7), PartPoint::Of(Planet::Venus), PartReversal::Never },
		{ L"Death", PartPoint::Asc(), PartPoint::Cusp(8), PartPoint::Of(Planet::Moon), PartReversal::Never },
	};
	return parts;
}

bool ArabicParts::IsDay(AstroPoint const& sun, AstroPoint const& ascendant) {
	// the houses run on from the Ascendant, 1 to 6 below the horizon: the Sun is up in the half that ends at the Ascendant
	return sun.IsBetween(ascendant.Opposite(), ascendant);
}

std::optional<bool> ArabicParts::IsDayChart(ChartData const& chart) {
	for (auto const& planet : chart.AllPlanets())
		if (planet.Planet == Planet::Sun)
			return IsDay(planet.Longitude, chart.Houses().Asc);
	return std::nullopt;
}

Planet ArabicParts::Ruler(ZodiacSign sign, bool modern) {
	static const Planet traditional[] = {
		Planet::Mars, Planet::Venus, Planet::Mercury, Planet::Moon, Planet::Sun, Planet::Mercury,
		Planet::Venus, Planet::Mars, Planet::Jupiter, Planet::Saturn, Planet::Saturn, Planet::Jupiter,
	};
	if (modern) {
		switch (sign) {
			case ZodiacSign::Scorpio: return Planet::Pluto;
			case ZodiacSign::Aquarius: return Planet::Uranus;
			case ZodiacSign::Pisces: return Planet::Neptune;
			default: break;
		}
	}
	return traditional[static_cast<int>(sign)];
}

std::optional<AstroPoint> ArabicParts::Position(PartPoint const& point, ChartData const& chart, PartOptions const& options,
	std::vector<PartData> const& parts) {
	auto& houses = chart.Houses();
	auto planetAt = [&](Planet body) -> std::optional<AstroPoint> {
		for (auto const& planet : chart.AllPlanets())
			if (planet.Planet == body)
				return planet.Longitude;
		return std::nullopt;
	};
	switch (point.Kind) {
		case PartPointKind::Planet:
			return planetAt(point.Body);
		case PartPointKind::Ascendant:
			return houses.Asc;
		case PartPointKind::Midheaven:
			return houses.MC;
		case PartPointKind::Descendant:
			return houses.Asc.Opposite();
		case PartPointKind::Imum:
			return houses.MC.Opposite();
		case PartPointKind::Cusp:
			if (point.Number < 1 || point.Number > 12)
				return std::nullopt;
			return houses.Cusps[point.Number - 1];
		case PartPointKind::RulerOfCusp:
			if (point.Number < 1 || point.Number > 12)
				return std::nullopt;
			return planetAt(Ruler(houses.Cusps[point.Number - 1].Sign(), options.ModernRulers));
		case PartPointKind::Part: {
			auto it = std::ranges::find(parts, point.Part, &PartData::Name);
			if (it == parts.end())
				return std::nullopt;
			return it->Longitude;
		}
	}
	return std::nullopt;
}

std::optional<PartData> ArabicParts::Calculate(ChartData const& chart, PartDefinition const& definition, PartOptions const& options,
	std::vector<PartData> const& parts) {
	auto base = Position(definition.Base, chart, options, parts);
	auto plus = Position(definition.Plus, chart, options, parts);
	auto minus = Position(definition.Minus, chart, options, parts);
	if (!base || !plus || !minus)
		return std::nullopt;

	PartData data;
	data.Name = definition.Name;
	switch (options.Sect) {
		case SectMode::Day: data.Night = false; break;
		case SectMode::Night: data.Night = true; break;
		default: data.Night = !IsDayChart(chart).value_or(true); break;
	}
	data.Reversed = data.Night && options.ReverseAtNight && definition.Reversal == PartReversal::AtNight;
	if (data.Reversed)
		std::swap(plus, minus);
	data.Longitude = AstroPoint(base->Value + plus->Value - minus->Value).Normalize();
	data.House = DerivedCharts::HouseOf(chart.Houses(), data.Longitude);
	data.Formula = Describe(definition, data.Reversed);
	return data;
}

std::vector<PartData> ArabicParts::Calculate(ChartData const& chart, std::vector<PartDefinition> const& definitions, PartOptions const& options) {
	std::vector<PartData> parts;
	for (auto const& definition : definitions)
		if (auto part = Calculate(chart, definition, options, parts))
			parts.push_back(std::move(*part));
	return parts;
}

std::vector<PartData> ArabicParts::Calculate(ChartData const& chart, PartOptions const& options) {
	return Calculate(chart, Standard(), options);
}

std::wstring ArabicParts::Describe(PartPoint const& point) {
	switch (point.Kind) {
		case PartPointKind::Planet: return PlanetNames[static_cast<int>(point.Body)];
		case PartPointKind::Ascendant: return L"Asc";
		case PartPointKind::Midheaven: return L"MC";
		case PartPointKind::Descendant: return L"Desc";
		case PartPointKind::Imum: return L"IC";
		case PartPointKind::Cusp: return L"cusp " + std::to_wstring(point.Number);
		case PartPointKind::RulerOfCusp: return L"ruler of " + std::to_wstring(point.Number);
		case PartPointKind::Part: return point.Part;
	}
	return L"";
}

std::wstring ArabicParts::Describe(PartDefinition const& definition, bool reversed) {
	auto const& plus = reversed ? definition.Minus : definition.Plus;
	auto const& minus = reversed ? definition.Plus : definition.Minus;
	return Describe(definition.Base) + L" + " + Describe(plus) + L" - " + Describe(minus);
}

std::vector<PartAspect> ArabicParts::Aspects(std::vector<PartData> const& parts, std::vector<PlanetPosition> const& planets,
	AspectCalculator const& calculator) {
	std::vector<PartAspect> aspects;
	for (size_t i = 0; i < parts.size(); i++) {
		for (auto const& planet : planets) {
			auto diff = static_cast<float>(AstroPoint::Diff(parts[i].Longitude, planet.Longitude));
			for (int type = 0; type < AspectSettings::AspectTypeCount; type++) {
				auto maxOrb = calculator.MaxOrbFor(static_cast<AspectType>(type), planet.Planet, std::nullopt);
				if (maxOrb < 0)
					continue;
				auto orb = std::fabs(diff - AspectCalculator::GetAspectAngle(static_cast<AspectType>(type)));
				if (orb <= maxOrb) {
					aspects.push_back({ i, planet, static_cast<AspectType>(type), diff, orb, maxOrb });
					break;
				}
			}
		}
	}
	return aspects;
}
