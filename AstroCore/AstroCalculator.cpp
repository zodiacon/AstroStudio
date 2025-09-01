#include "pch.h"
#include "AstroCalculator.h"
#include "DateTime.h"
#include "ChartData.h"
#include <assert.h>
#include <utility>

static char s_error[256];

const std::unordered_map<Planet, double> MonthlyCycle{
	{ Planet::Moon, 1.8},
	{ Planet::Sun, 27 },
	{ Planet::Mercury, 20 },
	{ Planet::Venus, 25 },
	{ Planet::Mars, 40 },
	{ Planet::Jupiter, 250 },
	{ Planet::Saturn, 500 },
	{ Planet::Uranus, 6 * 360 },
	{ Planet::Neptune, 13 * 360 },
	{ Planet::Pluto, 16 * 360 },
	{ Planet::MeanNode, 500 },
	{ Planet::TrueNode, 500 },
	{ Planet::Chiron, 4 * 360},
	{ Planet::OscuApog, 6 },
	{ Planet::Lilith, 200 },
};

AstroCalculator::AstroCalculator() : m_SweFlags(SEFLG_MOSEPH) {
	if (!s_init) {
		s_init = true;
		char path[MAX_PATH];
		GetModuleFileNameA(nullptr, path, std::size(path));
		*strrchr(path, '\\') = 0;
		swe_set_ephe_path(path);
	}
}

PlanetPosition AstroCalculator::CalcPlanet(Planet planet, DateTime const& dt, int harmonic, bool withSpeed) const {
	double xx[6];
	swe_calc_ut(dt, (int)planet, m_SweFlags | (withSpeed ? SEFLG_SPEED : 0), xx, s_error);
	PlanetPosition pp;
	pp.Planet = planet;
	pp.Longitude = xx[0] * harmonic;
	//pp.Longitude.Normalize();
	pp.Latitude = xx[1];
	if (withSpeed) {
		pp.Speed = xx[3];
		pp.LatitudeSpeed = xx[4];
		if (pp.Speed < 0)
			pp.Longitude.Flags |= AstroPointFlags::Retro;
	}
	return pp;
}

IngressData AstroCalculator::CalcPlanetIngress(Planet planet, DateTime start, bool reverse) const {
	const double eps = Epsilon;
	auto data = CalcPlanet(planet, start);
	auto sign = data.Longitude.Sign();
	const AstroPoint targetNext = data.Longitude.NextSign();
	const AstroPoint targetPrev = data.Longitude.ZeroSign();
	double avg = std::min(MonthlyCycle.at(planet), fabs(MonthlyCycle.at(planet)) /
		std::max(AstroPoint::Diff(data.Longitude, targetNext), AstroPoint::Diff(data.Longitude, targetPrev)));
	double dir = reverse ? -1 : 1;
	DateTime run = start;
	int iter = 0;
	do {
		start = start.AddDays(avg * dir);
		if (start < run && !reverse || start > run && reverse) start = run;
		auto next = CalcPlanet(planet, start);
		AstroPoint targetNext2 = next.Longitude.NextSign();
		if (next.Longitude.Sign() != sign) {
			// switch direction
			avg /= 2;
			dir = -dir;
			sign = next.Longitude.Sign();
		}
		data = next;
	} while (++iter < MaxIterations && AstroPoint::Diff(data.Longitude, targetNext) > eps && AstroPoint::Diff(data.Longitude, targetPrev) > eps);
	auto nextSign = AstroPoint(data.Speed >= 0 && !reverse /*|| data.LongitudeSpeed < 0 && reverse*/ ?
		(double)data.Longitude + 1 : (double)data.Longitude - 1).Sign();

	return IngressData{ planet, start, nextSign, data.Speed < 0 };
}

StationData AstroCalculator::CalcPlanetStation(Planet planet, DateTime start) const {
	double eps = Epsilon;
	assert(planet != Planet::Sun && planet != Planet::Moon);
	if (planet == Planet::Sun || planet == Planet::Moon)
		return StationData{};

	auto data = CalcPlanet(planet, start);
	int iter = 0;
	do {
		if (fabs(data.Speed) < eps)
			break;
		start = start.AddDays(fabs(data.Speed) / 2 * MonthlyCycle.at(planet) / 2);
		data = CalcPlanet(planet, start);
	} while (++iter < MaxIterations / 4);

	return StationData{ planet, start, data.Longitude, data.Speed > 0 };
}

bool AstroCalculator::Calculate(ChartData& data) {
	auto const& info = data.Info();
	data.Houses() = CalcHouses(info.Time, info.Latitude, info.Longitude, data.GetHouseSystem());
	for (auto& p : data.AllPlanets()) {
		p = CalcPlanet(p.Planet, info.Time, data.Harmonic());
	}

	return true;
}

HouseData AstroCalculator::CalcHouses(DateTime dt, double latitude, double longitude, HouseSystem system) {
	HouseData houses;
	double ascmc[10];
	double cusps[13];
	swe_houses(dt, latitude, longitude, (int)system, cusps, ascmc);
	for (int i = 0; i < 12; i++)
		houses.Cusps[i] = cusps[i + 1];

	houses.Asc = ascmc[0];
	houses.MC = ascmc[1];
	houses.Armc = ascmc[2];
	houses.Vertex = ascmc[3];
	houses.EquAsc = ascmc[4];
	houses.CoAsc1 = ascmc[5];
	houses.CoAsc2 = ascmc[6];
	houses.PolarAsc = ascmc[7];

	return houses;
}
