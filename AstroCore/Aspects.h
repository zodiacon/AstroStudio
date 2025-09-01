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

	None = -1,
};

struct AspectData {
	AspectType Type;
	double Angle;
	double Orb;
	PlanetPosition Planet1;
	PlanetPosition Planet2;
	bool Applying;

	bool IsMajor() const;
	bool IsSoft() const;
	bool IsHard() const;
};

struct AspectSettings {
	double MajorAspectOrb{ 8 };
	double ConjuntionOrbAdd{ 0 };
	double MinorAspectOrb{ 2 };
	double SunPlanetOrbAdd{ 0 };
	double MoonPlanetOrbAdd{ 0 };
	double SunMoonOrbAdd{ 0 };
	bool MajorOnly{ false };

	AspectSettings& CustomOrb(AspectType type, double orb);
};

class AspectCalculator {
public:
	static double GetAspectAngle(AspectType type);
	AspectCalculator() = default;
	explicit AspectCalculator(AspectSettings const& settings);
	std::vector<AspectData> Calculate(std::vector<PlanetPosition> const& planets) const;
	AspectData CalcAspect(PlanetPosition p1, PlanetPosition p2) const;

	AspectCalculator& Settings(AspectSettings const& settings);
	AspectSettings const& Settings() const;

	AspectType GetAspectType(Planet p1, Planet p2, double diff, double& dist) const;
	bool IsApplying(PlanetPosition p1, PlanetPosition p2, double angle, double exact) const;

private:
	AspectSettings m_settings;
};

