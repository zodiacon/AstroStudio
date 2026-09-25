#include "pch.h"
#include "Midpoints.h"
#include <algorithm>
#include <cmath>

std::vector<ChartPoint> Midpoints::Points(std::vector<PlanetPosition> const& planets) {
	std::vector<ChartPoint> points;
	points.reserve(planets.size());
	for (auto const& planet : planets)
		if (planet.Planet != Planet::PartOfFortune)		// (a midpoint of bodies, not of the Part of Fortune)
			points.push_back({ PointKind::Planet, planet.Planet, planet.Longitude, planet.Speed });
	return points;
}

std::vector<ChartPoint> Midpoints::Points(ChartData const& chart, MidpointOptions const& options) {
	std::vector<ChartPoint> points;
	for (auto const& planet : chart.AllPlanets()) {
		if (planet.Planet == Planet::PartOfFortune || (!options.Only.empty() && std::ranges::find(options.Only, planet.Planet) == options.Only.end()))
			continue;
		points.push_back({ PointKind::Planet, planet.Planet, planet.Longitude, planet.Speed });
	}
	if (options.Angles) {
		points.push_back({ PointKind::Ascendant, Planet::Sun, chart.Houses().Asc, 0 });
		points.push_back({ PointKind::Midheaven, Planet::Sun, chart.Houses().MC, 0 });
	}
	return points;
}

namespace {
	MidpointData Make(ChartPoint const& a, ChartPoint const& b) {
		MidpointData data;
		data.A = a;
		data.B = b;
		data.Longitude = AstroPoint::MidPoint(a.Longitude, b.Longitude);
		data.Speed = (a.Speed + b.Speed) / 2;
		data.Arc = AstroPoint::Diff(a.Longitude, b.Longitude);
		return data;
	}

	void SortByLongitude(std::vector<MidpointData>& midpoints) {
		std::ranges::stable_sort(midpoints, [](auto const& m1, auto const& m2) {
			return m1.Longitude.Value < m2.Longitude.Value;
		});
	}
}

std::vector<MidpointData> Midpoints::Calculate(std::vector<ChartPoint> const& points) {
	std::vector<MidpointData> midpoints;
	for (size_t i = 0; i < points.size(); i++)
		for (size_t j = i + 1; j < points.size(); j++)
			midpoints.push_back(Make(points[i], points[j]));
	SortByLongitude(midpoints);
	return midpoints;
}

std::vector<MidpointData> Midpoints::Calculate(ChartData const& chart, MidpointOptions const& options) {
	return Calculate(Points(chart, options));
}

std::vector<MidpointData> Midpoints::CalcBetween(std::vector<ChartPoint> const& a, std::vector<ChartPoint> const& b) {
	std::vector<MidpointData> midpoints;
	for (auto const& first : a)
		for (auto const& second : b)
			midpoints.push_back(Make(first, second));
	SortByLongitude(midpoints);
	return midpoints;
}

double Midpoints::OnDial(double longitude, int divisions) {
	if (divisions < 1)
		divisions = 1;
	double size = 360.0 / divisions;
	double value = std::fmod(longitude, size);
	if (value < 0)
		value += size;
	return value;
}

double Midpoints::ContactOrb(double point, double midpoint, ContactKind kind, int* angle) {
	// the angle between them, 0-180, and the nearest multiple of the step (180, or 45 on the dial)
	double distance = AstroPoint::Diff(AstroPoint(point).Normalize(), AstroPoint(midpoint).Normalize());
	double step = kind == ContactKind::Dial90 ? 45 : 180;
	double nearest = std::round(distance / step) * step;
	if (angle)
		*angle = (int)nearest;
	return std::fabs(distance - nearest);
}

std::vector<MidpointContact> Midpoints::Contacts(std::vector<MidpointData> const& midpoints, std::vector<ChartPoint> const& points,
	ContactOptions const& options) {
	std::vector<MidpointContact> contacts;
	for (size_t i = 0; i < midpoints.size(); i++) {
		auto const& midpoint = midpoints[i];
		for (auto const& point : points) {
			if (options.ExcludeMembers && (point.SameAs(midpoint.A) || point.SameAs(midpoint.B)))
				continue;
			int angle;
			double orb = ContactOrb(point.Longitude, midpoint.Longitude, options.Kind, &angle);
			if (orb <= options.Orb)
				contacts.push_back({ i, point, angle, orb });
		}
	}
	std::ranges::stable_sort(contacts, [](auto const& c1, auto const& c2) {
		return c1.Orb < c2.Orb;
	});
	return contacts;
}

std::vector<MidpointContact> Midpoints::Contacts(std::vector<MidpointData> const& midpoints, ChartPoint const& point,
	ContactOptions const& options) {
	return Contacts(midpoints, std::vector<ChartPoint>{ point }, options);
}
