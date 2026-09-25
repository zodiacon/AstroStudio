#include "pch.h"
#include "DerivedCharts.h"
#include <cmath>
#include <algorithm>

namespace {
	DateTime FromJulian(double jd) {
		return DateTime(jd, DateTime::AfterPapalReform(jd));
	}

	// an angle in (-180, 180]
	double Wrap180(double angle) {
		angle = fmod(angle, 360);
		if (angle > 180)
			angle -= 360;
		else if (angle <= -180)
			angle += 360;
		return angle;
	}

	double Radians(double degrees) {
		return degrees * 3.14159265358979323846 / 180;
	}

	double Degrees(double radians) {
		return radians * 180 / 3.14159265358979323846;
	}

	// the right ascension of the point of the ecliptic with this longitude, in degrees 0..360
	double RightAscension(double longitude, double obliquity) {
		double ra = Degrees(atan2(sin(Radians(longitude)) * cos(Radians(obliquity)), cos(Radians(longitude))));
		return ra < 0 ? ra + 360 : ra;
	}

	double Longitude(AstroCalculator const& calc, Planet planet, double jd) {
		return calc.CalcPlanet(planet, FromJulian(jd), 1, false).Longitude.Value;
	}

	// the geographic longitude (-180..180) halfway between two, on the shorter way round
	double MidLongitude(double a, double b) {
		double mid = AstroPoint::MidPoint(AstroPoint(a < 0 ? a + 360 : a), AstroPoint(b < 0 ? b + 360 : b)).Value;
		return mid > 180 ? mid - 360 : mid;
	}
}

double DerivedCharts::YearsBetween(DateTime const& from, DateTime const& to) {
	return (to.Julian() - from.Julian()) / TropicalYear;
}

DateTime DerivedCharts::ProgressedTime(DateTime const& natal, DateTime const& target) {
	return FromJulian(natal.Julian() + YearsBetween(natal, target));
}

double DerivedCharts::Arc(AstroCalculator const& calc, ChartData const& natal, DateTime const& target, ArcKey key) {
	double years = YearsBetween(natal.Info().Time, target);
	switch (key) {
		case ArcKey::Naibod:
			return years * NaibodArc;
		case ArcKey::Ptolemy:
			return years * PtolemyArc;
	}

	// the distance the Sun has gone in the progressed time
	double natalSun = calc.CalcPlanet(Planet::Sun, natal.Info().Time, 1, false).Longitude.Value;
	double progressedSun = calc.CalcPlanet(Planet::Sun, ProgressedTime(natal.Info().Time, target), 1, false).Longitude.Value;
	double arc = fmod(progressedSun - natalSun, 360);
	if (years >= 0 && arc < 0)
		arc += 360;
	else if (years < 0 && arc > 0)
		arc -= 360;
	return arc;
}

HouseData DerivedCharts::HousesFromMC(AstroCalculator const& calc, double midheaven, double latitude, DateTime const& time, HouseSystem system) {
	double obliquity = calc.Obliquity(time.Julian());
	return calc.CalcHousesFromArmc(RightAscension(midheaven, obliquity), latitude, obliquity, system);
}

ChartData DerivedCharts::Progress(AstroCalculator& calc, ChartData const& natal, DateTime const& target, ProgressionOptions const& options) {
	ChartData chart = natal;
	auto const& info = natal.Info();
	const auto system = natal.GetHouseSystem();

	switch (options.Method) {
		case ProgressionMethod::Secondary: {
			auto time = ProgressedTime(info.Time, target);
			chart.Info().Time = time;
			for (auto& planet : chart.AllPlanets())
				planet = calc.CalcPlanet(planet.Planet, time, natal.Harmonic());

			switch (options.Angles) {
				case ProgressedAngles::Calculated:
					chart.Houses() = calc.CalcHouses(time, info.Latitude, info.Longitude, system);
					break;
				case ProgressedAngles::SolarArc: {
					double arc = Arc(calc, natal, target, ArcKey::Actual);
					chart.Houses() = HousesFromMC(calc, natal.Houses().MC.Value + arc, info.Latitude, info.Time, system);
					break;
				}
				case ProgressedAngles::Natal:
					break;
			}
			break;
		}

		case ProgressionMethod::Primary: {
			// the arc of right ascension the sky has turned by; the Naibod arc if the key is the true solar arc
			double arc = Arc(calc, natal, target, options.Key == ArcKey::Actual ? ArcKey::Naibod : options.Key);
			double armc = AstroPoint(natal.Houses().Armc.Value + arc).Value;
			chart.Houses() = calc.CalcHousesFromArmc(armc, info.Latitude, calc.Obliquity(info.Time.Julian()), system);
			break;
		}

		case ProgressionMethod::SolarArc: {
			double arc = Arc(calc, natal, target, options.Key);
			for (auto& planet : chart.AllPlanets())
				planet.Longitude = AstroPoint(planet.Longitude.Value + arc * natal.Harmonic());
			auto& houses = chart.Houses();
			auto move = [&](AstroPoint& point) {
				point = AstroPoint(point.Value + arc);
			};
			move(houses.Asc);
			move(houses.MC);
			move(houses.Vertex);
			move(houses.EquAsc);
			move(houses.CoAsc1);
			move(houses.CoAsc2);
			move(houses.PolarAsc);
			for (auto& cusp : houses.Cusps)
				move(cusp);
			break;
		}
	}
	return chart;
}

std::optional<DateTime> DerivedCharts::FindLongitude(AstroCalculator const& calc, Planet planet, double longitude, DateTime const& from, ReturnSearch search) {
	if (search == ReturnSearch::Nearest) {
		auto next = FindLongitude(calc, planet, longitude, from, ReturnSearch::Next);
		auto previous = FindLongitude(calc, planet, longitude, from, ReturnSearch::Previous);
		if (next && previous)
			return next->Julian() - from.Julian() <= from.Julian() - previous->Julian() ? next : previous;
		return next ? next : previous;
	}

	const double direction = search == ReturnSearch::Next ? 1 : -1;
	auto offset = [&](double jd) {
		return Wrap180(Longitude(calc, planet, jd) - longitude);
	};

	// scan in steps that carry the planet a few degrees, and bisect the step in which it passes the longitude
	const double end = from.Julian() + direction * 400 * 365.25;
	double t0 = from.Julian() + direction * 1e-4;		// a moment off, so that being there now isn't finding it
	double g0 = offset(t0);
	for (int i = 0; i < 2000000 && direction * (end - t0) > 0; i++) {
		double speed = fabs(calc.CalcPlanet(planet, FromJulian(t0), 1, true).Speed);
		double step = speed > 0 ? std::clamp(10 / speed, 0.02, 30.0) : 30.0;
		double t1 = t0 + direction * step;
		double g1 = offset(t1);
		// a real crossing of zero, not the jump from 180 to -180
		if ((g0 <= 0 && g1 >= 0 || g0 >= 0 && g1 <= 0) && fabs(g0) < 90 && fabs(g1) < 90) {
			double a = t0, b = t1;
			double ga = g0;
			for (int j = 0; j < 60; j++) {
				double m = (a + b) / 2;
				double gm = offset(m);
				if ((ga <= 0) == (gm <= 0)) {
					a = m;
					ga = gm;
				}
				else
					b = m;
			}
			return FromJulian((a + b) / 2);
		}
		t0 = t1;
		g0 = g1;
	}
	return std::nullopt;
}

std::optional<DateTime> DerivedCharts::FindReturn(AstroCalculator const& calc, ChartData const& natal, Planet planet, DateTime const& from, ReturnSearch search) {
	double longitude;
	auto const& planets = natal.AllPlanets();
	if (auto it = std::find_if(planets.begin(), planets.end(), [&](auto const& p) { return p.Planet == planet; }); it != planets.end())
		longitude = it->Longitude.Value;
	else
		longitude = calc.CalcPlanet(planet, natal.Info().Time, 1, false).Longitude.Value;
	return FindLongitude(calc, planet, longitude, from, search);
}

ChartInfo DerivedCharts::MidpointInfo(ChartInfo const& a, ChartInfo const& b) {
	ChartInfo info = a;
	info.FirstName.clear();
	info.MiddleName.clear();
	info.LastName.clear();
	info.City.clear();
	info.State.clear();
	info.Country.clear();
	info.Type = InfoType::Event;
	info.Time = FromJulian((a.Time.Julian() + b.Time.Julian()) / 2);
	info.Latitude = (a.Latitude + b.Latitude) / 2;
	info.Longitude = MidLongitude(a.Longitude, b.Longitude);
	info.Elevation = (a.Elevation + b.Elevation) / 2;
	info.TimeZone = TimeZoneInfo{};		// a chart of the two has no time zone of its own
	return info;
}

ChartData DerivedCharts::Davison(AstroCalculator& calc, ChartData const& a, ChartData const& b) {
	ChartData chart;
	chart.SetHouseSystem(a.GetHouseSystem());
	chart.Harmonic(a.Harmonic());
	std::vector<Planet> planets;
	for (auto const& p : a.AllPlanets())
		planets.push_back(p.Planet);
	chart.AddPlanets(planets);
	chart.Info() = MidpointInfo(a.Info(), b.Info());
	calc.Calculate(chart);
	return chart;
}

ChartData DerivedCharts::Composite(AstroCalculator& calc, ChartData const& a, ChartData const& b, CompositeHouses method) {
	ChartData chart;
	chart.SetHouseSystem(a.GetHouseSystem());
	chart.Harmonic(a.Harmonic());

	for (auto const& pa : a.AllPlanets()) {
		auto const& others = b.AllPlanets();
		auto it = std::find_if(others.begin(), others.end(), [&](auto const& p) { return p.Planet == pa.Planet; });
		if (it == others.end())
			continue;

		PlanetPosition mid{};
		mid.Planet = pa.Planet;
		mid.Longitude = AstroPoint::MidPoint(pa.Longitude, it->Longitude);
		mid.Speed = (pa.Speed + it->Speed) / 2;
		mid.Latitude = (pa.Latitude + it->Latitude) / 2;
		mid.LatitudeSpeed = (pa.LatitudeSpeed + it->LatitudeSpeed) / 2;
		if (mid.Speed < 0)
			mid.Longitude.Flags |= AstroPointFlags::Retro;
		chart.AllPlanets().push_back(mid);
	}

	// the midpoint of the two births, in time and in place
	auto& info = chart.Info();
	info = MidpointInfo(a.Info(), b.Info());

	if (method == CompositeHouses::MidpointMC) {
		double mc = AstroPoint::MidPoint(a.Houses().MC, b.Houses().MC).Value;
		chart.Houses() = HousesFromMC(calc, mc, info.Latitude, info.Time, chart.GetHouseSystem());
	}
	else {
		double armc = AstroPoint::MidPoint(a.Houses().Armc, b.Houses().Armc).Value;
		chart.Houses() = calc.CalcHousesFromArmc(armc, info.Latitude, calc.Obliquity(info.Time.Julian()), chart.GetHouseSystem());
	}
	return chart;
}

int DerivedCharts::HouseOf(HouseData const& houses, AstroPoint const& longitude) {
	// a house runs from its cusp up to, but not including, the next one: a planet on a cusp is in the house that begins there
	auto span = [](double from, double to) {
		double d = fmod(to - from, 360);
		return d < 0 ? d + 360 : d;
	};
	for (int i = 0; i < 12; i++) {
		double from = houses.Cusps[i].Value, width = span(from, houses.Cusps[(i + 1) % 12].Value);
		if (width > 0 && span(from, longitude.Value) < width)
			return i + 1;
	}
	return 0;
}

std::vector<int> DerivedCharts::HouseOverlay(HouseData const& houses, std::vector<PlanetPosition> const& planets) {
	std::vector<int> result;
	result.reserve(planets.size());
	for (auto const& planet : planets)
		result.push_back(HouseOf(houses, planet.Longitude));
	return result;
}
