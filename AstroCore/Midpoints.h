#pragma once

#include "ChartData.h"
#include <vector>

// Midpoints: the point half way between two others. Everything here is about longitudes on the ecliptic (no UI).

// What a ChartPoint is: a planet (or another body of the chart), or one of the two angles.
enum class PointKind {
	Planet,
	Ascendant,
	Midheaven,
};

// A point of a chart that can be one end of a midpoint: a planet with its position, or the Ascendant or Midheaven.
struct ChartPoint {
	PointKind Kind{ PointKind::Planet };
	Planet Body{ Planet::Sun };		// (only for a planet)
	AstroPoint Longitude;
	double Speed{ 0 };				// degrees per day; angles have none
	// Which chart the point is of, when there are more than one (a chart and the planets around it: 0 and 1). The Sun of one is not
	// the Sun of the other, so the ends of a midpoint between two charts are not mistaken for points standing on it.
	int Set{ 0 };

	bool SameAs(ChartPoint const& other) const noexcept {
		return Set == other.Set && Kind == other.Kind && (Kind != PointKind::Planet || Body == other.Body);
	}
};

// The midpoint of two points, on the shorter arc between them (350 and 10 have their midpoint at 0, not at 180).
struct MidpointData {
	ChartPoint A, B;
	AstroPoint Longitude;		// the midpoint
	double Speed{ 0 };			// the mean of the two speeds
	double Arc{ 0 };			// the distance between the two points, 0-180 (the midpoint is half of it from each)

	// the midpoint on the other side of the zodiac: it is on the same axis, and matters just as much
	AstroPoint Opposite() const {
		return Longitude.Opposite();
	}
};

struct MidpointOptions {
	// the Ascendant and Midheaven take part (when the chart has houses) ...
	bool Angles{ false };
	// ... and which of the two, when they do
	bool Ascendant{ true };
	bool Midheaven{ true };
	// the bodies that take part; empty means all the chart's
	std::vector<Planet> Only;
	// bodies that are left out (of those that take part)
	std::vector<Planet> Except;
};

// What counts as a planet standing on a midpoint.
enum class ContactKind {
	// on the midpoint or on the point opposite it (the axis of the midpoint: 0 and 180 degrees)
	Axis,
	// as Ebertin's 90 degree dial: also at 45, 90 and 135 degrees from the midpoint
	Dial90,
	// the same points (0, 45, 90, 135 and 180 degrees), but a tree of them is read on the 45 degree dial: the dial of the eighth harmonic,
	// where the conjunction, semi-square, square, sesquiquadrate and opposition of a place all fall on the same spot
	Dial45,
};

struct ContactOptions {
	ContactKind Kind{ ContactKind::Axis };
	double Orb{ 1.5 };			// degrees from exact
	// a point never counts for a midpoint it is one end of (at 45 degrees it always would be, half the arc being 45 when the
	// arc is 90); switch off when the midpoints and the points belong to different charts
	bool ExcludeMembers{ true };
};

// A point on a midpoint (or on an angle to it).
struct MidpointContact {
	size_t Midpoint;			// the index of the midpoint in the list that was searched
	ChartPoint Point;
	int Angle;					// 0, 45, 90, 135 or 180: the angle between the point and the midpoint
	double Orb;					// how far from exact, in degrees
};

// One branch of a midpoint tree: a point and the midpoints that stand on it (or at an angle to it), the tightest first.
struct MidpointBranch {
	ChartPoint Point;
	double Dial{ 0 };								// where the point is on the dial (the 90 degree one, or the 45 degree one for ContactKind::Dial45)
	std::vector<MidpointContact> Contacts;			// (Midpoint is an index into the midpoints the tree was made from)
};

class Midpoints final {
public:
	// The points of a chart that the options let take part: its planets in the order they are kept in, then the Ascendant and
	// the Midheaven if wanted.
	// (`set` is the number the points get for ChartPoint::Set)
	static std::vector<ChartPoint> Points(ChartData const& chart, MidpointOptions const& options = {}, int set = 0);
	static std::vector<ChartPoint> Points(std::vector<PlanetPosition> const& planets, int set = 0);

	// The midpoint of every pair of the points (none with itself), in the order of their longitudes (the order of the points
	// for equal ones).
	static std::vector<MidpointData> Calculate(std::vector<ChartPoint> const& points);
	static std::vector<MidpointData> Calculate(ChartData const& chart, MidpointOptions const& options = {});
	// The midpoint of each point of a with each of b (two charts: A is from a and B from b), in the order of their longitudes.
	static std::vector<MidpointData> CalcBetween(std::vector<ChartPoint> const& a, std::vector<ChartPoint> const& b);

	// The points that stand on a midpoint, with the tightest first (a point on several midpoints is listed for each).
	static std::vector<MidpointContact> Contacts(std::vector<MidpointData> const& midpoints, std::vector<ChartPoint> const& points,
		ContactOptions const& options = {});
	// The contacts of one point only: its midpoint tree.
	static std::vector<MidpointContact> Contacts(std::vector<MidpointData> const& midpoints, ChartPoint const& point,
		ContactOptions const& options = {});

	// The midpoint tree of a set of points: for each of them that has a midpoint on it, the midpoints there (as Contacts finds them,
	// so with the same options), the points in the order of their places on the 90 degree dial - the order the tree is read in, the
	// contacts of the whole chart side by side - and the midpoints of each branch tightest first. Points with none are left out.
	static std::vector<MidpointBranch> Tree(std::vector<MidpointData> const& midpoints, std::vector<ChartPoint> const& points,
		ContactOptions const& options = {});

	// the position of a longitude on a dial of 360/n degrees: on the 90 degree dial (n = 4) the conjunction, square and
	// opposition all fall together, and the midpoints of a tree stand side by side
	static double OnDial(double longitude, int divisions = 4);
	// the number of divisions of the circle that a tree of this kind of contact is read on: 8 (the 45 degree dial) for Dial45, otherwise 4
	static int DialDivisions(ContactKind kind) noexcept {
		return kind == ContactKind::Dial45 ? 8 : 4;
	}
	// how far a longitude is from the nearest of the angles that the kind of contact counts, and which one that was
	static double ContactOrb(double point, double midpoint, ContactKind kind, int* angle = nullptr);
};
