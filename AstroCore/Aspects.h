#pragma once

#include "AstroCalculator.h"
#include <array>
#include <optional>

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
	// the widest orb the settings allowed for this pair of planets and this aspect: Orb / MaxOrb says how loose it is
	float MaxOrb{ 0 };

	bool IsMajor() const noexcept;
	bool IsSoft() const noexcept;
	bool IsHard() const noexcept;
};

struct AspectSettings {
	static constexpr int AspectTypeCount = 15;
	static constexpr int PlanetCount = static_cast<int>(Planet::NumPlanets);

	AspectSettings();

	float MajorAspectOrb{ 8 };
	float ConjunctionOrbAdd{ 0 };
	float MinorAspectOrb{ 2 };
	float SunPlanetOrbAdd{ 0 };
	float MoonPlanetOrbAdd{ 0 };
	float SunMoonOrbAdd{ 0 };
	bool MajorOnly{ false };

	// Each aspect can be switched off, or given an orb of its own (negative: the general orb of its kind, major or minor,
	// applies - with ConjunctionOrbAdd for the conjunction).
	std::array<bool, AspectTypeCount> AspectEnabled;
	std::array<float, AspectTypeCount> AspectOrb;
	// A planet can be left out of the aspects altogether, or have extra orb for the aspects it takes part in (the larger of
	// the two planets' extras is used; the Sun and Moon extras above come on top).
	std::array<bool, PlanetCount> PlanetEnabled;
	std::array<float, PlanetCount> PlanetOrbAdd;

	AspectSettings& CustomOrb(AspectType type, double orb);
	// the orb for an aspect of this type before any planet's extra
	float OrbFor(AspectType type) const;
	bool IsEnabled(Planet planet) const;
	float OrbAdd(Planet planet) const;
};

class AspectCalculator {
public:
	static float GetAspectAngle(AspectType type);
	AspectCalculator() = default;
	explicit AspectCalculator(AspectSettings const& settings);
	std::vector<AspectData> Calculate(std::vector<PlanetPosition> const& planets) const;
	// The aspects between two sets of planets (two charts, or a chart and its transits): each of a with each of b, with a's
	// planet as Planet1 and b's as Planet2. The same planet in both is a pair like any other.
	std::vector<AspectData> CalcBetween(std::vector<PlanetPosition> const& a, std::vector<PlanetPosition> const& b) const;
	AspectData CalcAspect(PlanetPosition p1, PlanetPosition p2) const;

	AspectCalculator& Settings(AspectSettings const& settings);
	AspectSettings const& Settings() const;

	AspectType GetAspectType(Planet p1, Planet p2, float diff, float& dist, float* maxOrb = nullptr) const;
	// The widest orb for an aspect of this type between p1 and p2 (only p1 when the other point is not a planet, like an
	// angle of a chart); negative if the aspect can't be made: it is switched off, or a planet is, or only major aspects count.
	float MaxOrbFor(AspectType type, Planet p1, std::optional<Planet> p2) const;
	static bool IsApplying(PlanetPosition p1, PlanetPosition p2, double angle, double exact) noexcept;

private:
	AspectSettings m_settings;
};

