#pragma once

#include "AstroCalculator.h"

// all aspect angles are multiplied by 100 to keep them as simple integers

enum class AspectType {
	Conjunction,
	Sextile,
	Square,
	Trine,
	Opposition,

	SemiSextile,
	SemiSquare,
	Quintile,
	BiQuintile,
	Septile,
	BiSeptile,
	Quincunx,
	SesquiQuadrate,
	Novile,
	BiNovile,

	None = -1,
};

struct AspectData {
	AspectType Type;
	float Angle;
	float Orb;
	PlanetPosition Planet1;
	PlanetPosition Planet2;
	bool Applying;

	bool IsMajor() const noexcept;
	bool IsSoft() const noexcept;
	bool IsHard() const noexcept;
};

struct AspectSettings {
	float MajorAspectOrb{ 8 };
	float ConjunctionOrbAdd{ 0 };
	float MinorAspectOrb{ 2 };
	float SunPlanetOrbAdd{ 0 };
	float MoonPlanetOrbAdd{ 0 };
	float SunMoonOrbAdd{ 0 };
	bool MajorOnly{ false };

	AspectSettings& CustomOrb(AspectType type, double orb);
};

class AspectCalculator {
public:
	static float GetAspectAngle(AspectType type);
	AspectCalculator() = default;
	explicit AspectCalculator(AspectSettings const& settings);
	std::vector<AspectData> Calculate(std::vector<PlanetPosition> const& planets) const;
	AspectData CalcAspect(PlanetPosition p1, PlanetPosition p2) const;

	AspectCalculator& Settings(AspectSettings const& settings);
	AspectSettings const& Settings() const;

	AspectType GetAspectType(Planet p1, Planet p2, float diff, float& dist) const;
	static bool IsApplying(PlanetPosition p1, PlanetPosition p2, double angle, double exact) noexcept;

private:
	AspectSettings m_settings;
};

