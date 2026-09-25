#pragma once

#include "ChartData.h"
#include "Aspects.h"
#include "DerivedCharts.h"
#include <string>
#include <vector>

// What a chart wheel can show around the chart itself.
enum class OverlayKind {
	Transit,		// the planets of a moment (at first now)
	Progressed,		// the chart moved on to a date
	Synastry,		// another chart
};

// A second set of planets shown in a band around the chart (a bi-wheel), with its aspects to the chart.
struct ChartOverlay {
	OverlayKind Kind{ OverlayKind::Transit };
	// what Progressed means: secondary progressions or solar arc directions
	ProgressionMethod Method{ ProgressionMethod::Secondary };
	// The moment (UT) the overlay is for, and how to show it as wall-clock time: transits are the sky then, progressions
	// are the chart progressed to then. Step, Auto and Live move it. A synastry overlay has none: it is another chart as it is.
	DateTime When;
	TimeZoneInfo Zone;
	bool FollowsTime() const {
		return Kind != OverlayKind::Synastry;
	}
	// the planets shown (for a synastry overlay, a copy of the other chart)
	ChartData Data;
	// the aspects between the overlay's planets (Planet1) and the chart's (Planet2)
	std::vector<AspectData> Aspects;
	// written in a corner of the wheel: what the overlay is and when
	std::wstring Caption;
	// how the overlay's planets are called in tips ("Transit Sun") and how the chart's are in an aspect ("... natal Mercury")
	std::wstring Label{ DefaultLabel(OverlayKind::Transit) };
	std::wstring BaseLabel{ L"natal" };

	static std::wstring DefaultLabel(OverlayKind kind) {
		switch (kind) {
			case OverlayKind::Progressed: return L"Progressed";
			case OverlayKind::Synastry: return L"Other";
			default: return L"Transit";
		}
	}
};
