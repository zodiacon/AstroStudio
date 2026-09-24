#include "pch.h"
#include "AstroCalculator.h"
#include "DateTime.h"
#include "ChartData.h"
#include <assert.h>
#include <utility>
#include <optional>
#include <algorithm>

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
	{ Planet::Pholus, 6 * 360 },
	{ Planet::Ceres, 120 },
	{ Planet::Pallas, 120 },
	{ Planet::Juno, 120 },
	{ Planet::Vesta, 100 },
};

// About how many days a body needs to cross a sign, which the ingress and station searches size their steps by. A body
// that isn't in the table (a new one, or Earth) gets a slow year: the searches then take more steps but never fail.
static double CycleOf(Planet planet) {
	auto it = MonthlyCycle.find(planet);
	return it != MonthlyCycle.end() ? it->second : 360;
}

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
	double xx[6]{};		// stays zero if the ephemeris can't give the body (its file is missing, or the year is outside it)
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
	double avg = std::min(CycleOf(planet), fabs(CycleOf(planet)) /
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
		start = start.AddDays(fabs(data.Speed) / 2 * CycleOf(planet) / 2);
		data = CalcPlanet(planet, start);
	} while (++iter < MaxIterations / 4);

	return StationData{ planet, start, data.Longitude, data.Speed > 0 };
}

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
}

std::vector<EclipseData> AstroCalculator::CalcEclipses(DateTime const& from, DateTime const& to) const {
	std::vector<EclipseData> eclipses;
	const double end = to.Julian();
	for (bool solar : { true, false }) {
		double search = from.Julian();
		// eclipses of a kind are at least several months apart; the limit is a safety net
		for (int i = 0; i < 100000 && search < end; i++) {
			double tret[10]{};
			char error[256]{};
			int flags = solar ? swe_sol_eclipse_when_glob(search, m_SweFlags, 0, tret, 0, error) : swe_lun_eclipse_when(search, m_SweFlags, 0, tret, 0, error);
			if (flags == ERR || tret[0] >= end)
				break;

			EclipseKind kind = EclipseKind::Partial;
			if (flags & SE_ECL_TOTAL)
				kind = EclipseKind::Total;
			else if (flags & SE_ECL_ANNULAR_TOTAL)
				kind = EclipseKind::Hybrid;
			else if (flags & SE_ECL_ANNULAR)
				kind = EclipseKind::Annular;
			else if (flags & SE_ECL_PENUMBRAL)
				kind = EclipseKind::Penumbral;
			eclipses.push_back({ FromJulian(tret[0]), solar, kind });
			search = tret[0] + 1;
		}
	}
	std::sort(eclipses.begin(), eclipses.end(), [](auto const& a, auto const& b) { return a.Maximum.Julian() < b.Maximum.Julian(); });
	return eclipses;
}

double AstroCalculator::Longitude(int planet, double jd) const {
	double xx[6]{};
	char error[256];
	swe_calc_ut(jd, planet, m_SweFlags, xx, error);
	return xx[0];
}

double AstroCalculator::MoonSignChange(double jd, bool forward) const {
	auto sign = [&](double t) { return (int)(Longitude(SE_MOON, t) / 30); };
	const int start = sign(jd);
	const double step = (forward ? 1 : -1) / 12.0;		// two hours; the Moon takes over two days in a sign
	double inside = jd, outside = jd;
	for (int i = 0; i < 80; i++) {
		outside = inside + step;
		if (sign(outside) != start)
			break;
		inside = outside;
	}
	for (int i = 0; i < 40; i++) {
		double middle = (inside + outside) / 2;
		(sign(middle) == start ? inside : outside) = middle;
	}
	return (inside + outside) / 2;
}

VoidOfCourseData AstroCalculator::CalcVoid(double entered, double left, bool outerPlanets) const {
	static constexpr int bodies[] = { SE_SUN, SE_MERCURY, SE_VENUS, SE_MARS, SE_JUPITER, SE_SATURN, SE_URANUS, SE_NEPTUNE, SE_PLUTO };
	const int count = outerPlanets ? 9 : 6;
	// the Moon runs ahead of everything, so the difference between it and a planet, less an aspect's angle, only increases;
	// the aspect is exact when that difference passes through zero
	static constexpr double angles[] = { 0, 60, -60, 90, -90, 120, -120, 180 };

	// look for the last crossing, in steps of two hours (the Moon covers one degree in that time)
	const double step = 1 / 12.0;
	struct Crossing {
		double From, To;
		int Body;
		double Angle;
	};
	std::optional<Crossing> last;

	double t = entered;
	double moon = Longitude(SE_MOON, t);
	double planets[9];
	for (int j = 0; j < count; j++)
		planets[j] = Longitude(bodies[j], t);
	while (t < left) {
		double next = std::min(t + step, left);
		double nextMoon = Longitude(SE_MOON, next);
		double nextPlanets[9];
		for (int j = 0; j < count; j++) {
			nextPlanets[j] = Longitude(bodies[j], next);
			for (double angle : angles) {
				double before = Wrap180(moon - planets[j] - angle), after = Wrap180(nextMoon - nextPlanets[j] - angle);
				if (before < 0 && after >= 0 && after - before < 90 && (!last || next >= last->To))
					last = Crossing{ t, next, j, angle };
			}
		}
		t = next;
		moon = nextMoon;
		std::copy(std::begin(nextPlanets), std::end(nextPlanets), std::begin(planets));
	}

	VoidOfCourseData data{};
	data.End = FromJulian(left);
	data.Sign = static_cast<ZodiacSign>((int)(Longitude(SE_MOON, entered + 0.01) / 30));
	if (!last) {
		data.Start = FromJulian(entered);
		data.WholeSign = true;
		return data;
	}

	// pin the last aspect down
	double from = last->From, to = last->To;
	for (int i = 0; i < 40; i++) {
		double middle = (from + to) / 2;
		double difference = Wrap180(Longitude(SE_MOON, middle) - Longitude(bodies[last->Body], middle) - last->Angle);
		(difference < 0 ? from : to) = middle;
	}
	data.Start = FromJulian((from + to) / 2);
	data.WholeSign = false;
	data.LastPlanet = static_cast<Planet>(bodies[last->Body]);
	data.LastAngle = fabs(last->Angle);
	return data;
}

std::vector<VoidOfCourseData> AstroCalculator::CalcVoidOfCourse(DateTime const& from, DateTime const& to, bool outerPlanets) const {
	std::vector<VoidOfCourseData> periods;
	const double first = from.Julian(), end = to.Julian();

	double entered = MoonSignChange(first, false);
	double left = MoonSignChange(first, true);
	for (int i = 0; i < 100000; i++) {
		periods.push_back(CalcVoid(entered, left, outerPlanets));
		if (left >= end)
			break;
		entered = left;
		left = MoonSignChange(left + 0.02, true);
	}
	return periods;
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
