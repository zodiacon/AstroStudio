#pragma once

#include "Midpoints.h"
#include "Aspects.h"
#include <bitset>
#include <string>

// What the midpoint list and the midpoint tree work with (Options > Midpoints): which points take part as the ends of a midpoint (the
// bodies, the Ascendant, the Midheaven) and, for the list and for the tree separately, how near a point must be to a midpoint to
// stand on it (the orb) and which angles count (ContactKind: the 90 degree dial or just the axis). The tree's orb and kind are also
// the two boxes at the top of its tab, and are the same setting.
//
// Kept in the app settings (AppSettings::MidpointSettingsText) as text like
//   except=10,13;asc=1;mc=1;listorb=150;listkind=axis;treeorb=150;treekind=dial
// (the kinds are axis, dial - the 90 degree dial - and dial45; the list counts the same contacts on either dial, and the tree reads its branches on it)
// where except lists the bodies (Planet numbers) that are left out, so that a new body takes part, and the orbs are in hundredths
// of a degree.
struct MidpointSettings {
	std::bitset<static_cast<size_t>(Planet::NumPlanets)> HiddenPlanets;
	bool Ascendant{ true };
	bool Midheaven{ true };
	int ListOrb{ 150 };		// hundredths of a degree
	int TreeOrb{ 150 };
	ContactKind ListKind{ ContactKind::Axis };
	ContactKind TreeKind{ ContactKind::Dial90 };

	// the orbs to choose from, in hundredths of a degree
	static constexpr int Orbs[] = { 25, 50, 100, 150, 200, 300, 400, 500 };
	// the index of the one nearest to an orb (which is why an orb that isn't in the list, from a hand-edited setting, still works)
	static int NearestOrb(int hundredths);

	bool TakesPart(Planet planet) const {
		auto index = static_cast<size_t>(planet);
		return index >= HiddenPlanets.size() || !HiddenPlanets[index];
	}
	// The points that take part, for MidpointOptions: `angles` is whether the chart has any to offer (a transit's sky has none), and
	// the settings then say which.
	MidpointOptions Points(bool angles = true) const;
	ContactOptions ListContacts() const;
	ContactOptions TreeContacts() const;

	// the point in the glyph font: the planet's glyph, and Z and X for the Ascendant and the Midheaven
	static CString PointGlyph(ChartPoint const& point);
	// the aspect that an angle between a point and a midpoint (0, 45, 90, 135, 180) is, for its glyph
	static AspectType AspectOfAngle(int angle);

	// the angle between a point and the midpoint it stands on in words: "on", "semi-square", "square", "sesquiquadrate", "opposite"
	static PCWSTR AngleWords(int angle);
	// ... and short, for a cell of the list ("" for on the midpoint)
	static PCWSTR ShortAngleWords(int angle);

	std::wstring ToText() const;
	// Reads what ToText wrote; anything it doesn't understand is left as it was.
	void FromText(std::wstring const& text);

	// the settings in force
	static MidpointSettings& Current();
	static void LoadFromSettings();
	static void StoreInSettings();
};
