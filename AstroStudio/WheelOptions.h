#pragma once

#include "Aspects.h"
#include <bitset>
#include <string>

// What the chart wheel draws: which planets, which aspect lines, and whether the points past Pluto (Chiron, the nodes, Lilith,
// the asteroids...) get aspect lines. It is only about drawing: the aspect list and grid, the tooltips and the calculations
// still work with everything. It applies to the planets around the chart (transits...) as it does to the chart's own.
// Conjunctions have no line to draw (the planets stand together), so they are not an option.
//
// Kept in the app settings (AppSettings::WheelOptions) as text like  planets=10,13;aspects=9,10;beyond=1  - the numbers are
// the planets (Planet) and aspects (AspectType) left out, so that whatever is new is drawn.
struct WheelOptions {
	std::bitset<static_cast<size_t>(Planet::NumPlanets)> HiddenPlanets;
	std::bitset<AspectSettings::AspectTypeCount> HiddenAspects;
	bool BeyondPluto{ false };

	bool ShowsPlanet(Planet planet) const {
		auto index = static_cast<size_t>(planet);
		return index >= HiddenPlanets.size() || !HiddenPlanets[index];
	}
	bool ShowsAspect(AspectType type) const {
		auto index = static_cast<size_t>(type);
		return index >= HiddenAspects.size() || !HiddenAspects[index];
	}
	// Is this aspect's line drawn: is its kind on, and are both planets shown (and not past Pluto, unless that is on)?
	bool ShowsLine(AspectData const& aspect) const {
		return ShowsAspect(aspect.Type) && ShowsPlanet(aspect.Planet1.Planet) && ShowsPlanet(aspect.Planet2.Planet) &&
			(BeyondPluto || (aspect.Planet1.Planet <= Planet::Pluto && aspect.Planet2.Planet <= Planet::Pluto));
	}

	std::wstring ToText() const;
	// Reads what ToText wrote; anything it doesn't understand is left as it was.
	void FromText(std::wstring const& text);

	// the options in force, which every chart wheel draws by
	static WheelOptions& Current();
	static void LoadFromSettings();
	static void StoreInSettings();
};
