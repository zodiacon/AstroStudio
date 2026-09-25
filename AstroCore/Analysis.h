#pragma once

#include "ChartData.h"
#include "Aspects.h"
#include "DerivedCharts.h"
#include <functional>
#include <memory>
#include <vector>

// What is compared with what.
enum class AnalysisType {
	TransitsToNatal,			// the sky at each date against the birth chart
	ProgressedToNatal,			// secondary progressions at each date against the birth chart
	SolarArcToNatal,			// the birth chart moved by the solar arc at each date, against the birth chart
	TransitsToProgressed,		// the sky at each date against the chart progressed to that same date
	ProgressedToProgressed,		// the progressed planets against each other
};

enum class AnalysisEventKind {
	EnterOrb,			// an aspect comes within its orb
	Exact,				// an aspect is exact
	LeaveOrb,			// and goes out of it again
	InOrbAtStart,		// the aspect was already within its orb when the range began
	InOrbAtEnd,			// and still is when it ends
	HouseIngress,		// a planet crosses a cusp of the birth chart into a house (Index: 1 to 12)
	SignIngress,		// a planet changes sign (Index: 0 for Aries to 11)
	StationRetrograde,	// a planet turns retrograde
	StationDirect,		// and direct
};

// the point an aspect is made to: a planet, or one of the angles of the birth chart
enum class AnalysisTarget : unsigned char {
	Planet, Ascendant, Midheaven,
};

// The whole of an aspect's stay within its orb, as far as the range shows it.
struct AnalysisWindow {
	bool HasEnter{ false };			// false if it was already within its orb when the range began
	DateTime Enter;
	std::vector<DateTime> Exacts;	// usually one; a planet that turns inside the orb can be exact twice or more
	bool HasLeave{ false };			// false if it is still within its orb when the range ends
	DateTime Leave;
};

struct AnalysisEvent {
	AnalysisType Type{ AnalysisType::TransitsToNatal };		// which analysis found it (they can be run together)
	DateTime Time;								// UT
	AnalysisEventKind Kind{ AnalysisEventKind::Exact };
	Planet Mover{ Planet::Sun };				// the planet that moves (transiting, progressed or directed)
	AnalysisTarget TargetKind{ AnalysisTarget::Planet };
	Planet Target{ Planet::Sun };				// aspects: the planet it is made to (unused for an angle)
	AspectType Aspect{ AspectType::None };		// aspects
	double Orb{ 0 };							// aspects: the orb the aspect is judged by (the widest allowed)
	int Pass{ 0 };								// aspects: 1, 2 or 3 - a retrograde planet can make the same aspect up to three times
	bool Retrograde{ false };					// the mover's motion at that moment
	int Index{ 0 };								// ingresses: the house or the sign
	double Longitude{ 0 };						// where the mover is
	// The row that ends a stay within the orb - the event that leaves it, or the one that says it still is at the end of the
	// range - has the whole stay: when it entered, was exact and left. Null for every other event.
	std::shared_ptr<AnalysisWindow const> Window;
};

struct AnalysisSettings {
	AnalysisType Type{ AnalysisType::TransitsToNatal };
	// Several analyses can be run together (RunAll): these, when there are any (Type is then the first), otherwise just Type.
	std::vector<AnalysisType> Types;
	std::vector<AnalysisType> TypeList() const {
		return Types.empty() ? std::vector<AnalysisType>{ Type } : Types;
	}
	DateTime From, To;							// UT
	std::vector<Planet> Movers;
	std::vector<Planet> Targets;				// planets the aspects are made to (the natal ones, or the progressed ones)
	bool NatalAngles{ true };					// the Ascendant and Midheaven too, when the targets are natal
	AspectSettings Aspects;						// which aspects, and their orbs (a planet switched off there is left out)
	bool AspectEvents{ true };
	// Events that need no target. House ingresses are through the houses of the birth chart, so only the types that compare
	// with it (natal targets) have them; the other kinds work for every type.
	bool HouseIngresses{ false };
	bool SignIngresses{ false };
	bool Stations{ false };
	ArcKey Key{ ArcKey::Actual };				// for solar arc

	// Transits of the Moon (about 13 degrees a day) would bury everything else in a long range: beyond this it is dropped.
	static constexpr double MoonTransitLimitDays = 62;

	bool TransitMovers() const {
		return Type == AnalysisType::TransitsToNatal || Type == AnalysisType::TransitsToProgressed;
	}
	bool NatalTargets() const {
		return Type == AnalysisType::TransitsToNatal || Type == AnalysisType::ProgressedToNatal || Type == AnalysisType::SolarArcToNatal;
	}
	// is the Moon taken out of the movers for this range
	bool MoonDropped() const {
		return TransitMovers() && To.Julian() - From.Julian() > MoonTransitLimitDays;
	}
	// the movers that are used
	std::vector<Planet> EffectiveMovers() const;
};

struct AnalysisResult {
	std::vector<AnalysisEvent> Events;		// in time order
	bool Cancelled{ false };
};

// Scans a range of dates for the moments things happen between the birth chart and the sky (or its progressions).
struct Analysis abstract final {
	// The events of the range. progress, if given, is called now and then with the fraction done (0 to 1) and returns false to
	// cancel the run.
	static AnalysisResult Run(AstroCalculator const& calc, ChartData const& natal, AnalysisSettings const& settings,
		std::function<bool(double)> const& progress = {});
	// One analysis for each of the settings' types (TypeList), their events together in time order, each with its Type. Sign
	// ingresses and stations are found once for each kind of mover (the sky, progressions, solar arc directions), by the first
	// analysis that has it, so that two analyses of transits don't list every one of them twice.
	static AnalysisResult RunAll(AstroCalculator const& calc, ChartData const& natal, AnalysisSettings const& settings,
		std::function<bool(double)> const& progress = {});
};
