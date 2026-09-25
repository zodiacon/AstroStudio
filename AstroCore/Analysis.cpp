#include "pch.h"
#include "Analysis.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <tuple>

namespace {
	double Wrap180(double angle) {
		angle = fmod(angle, 360);
		if (angle > 180)
			angle -= 360;
		else if (angle <= -180)
			angle += 360;
		return angle;
	}

	double Wrap360(double angle) {
		angle = fmod(angle, 360);
		return angle < 0 ? angle + 360 : angle;
	}

	DateTime FromJulian(double jd) {
		return DateTime(jd, DateTime::AfterPapalReform(jd));
	}

	// where a point is, and how fast it moves (degrees, and degrees per day of real time)
	struct Pos {
		double Lon;
		double Speed;
	};

	//
	// Where the planets are, at any time: the sky, the sky progressed, the birth chart moved by the solar arc, or the birth chart.
	//
	class Source {
	public:
		virtual ~Source() = default;
		virtual Pos At(Planet planet, double jd) const = 0;
		// moves so slowly that its values are worth keeping and interpolating
		virtual bool Slow() const {
			return false;
		}
	};

	class TransitSource : public Source {
	public:
		explicit TransitSource(AstroCalculator const& calc) : m_calc(calc) {
		}
		Pos At(Planet planet, double jd) const override {
			auto pp = m_calc.CalcPlanet(planet, FromJulian(jd), 1, true);
			return { pp.Longitude.Value, pp.Speed };
		}

	private:
		AstroCalculator const& m_calc;
	};

	class ProgressedSource : public Source {
	public:
		ProgressedSource(AstroCalculator const& calc, double natalJd) : m_calc(calc), m_natal(natalJd) {
		}
		Pos At(Planet planet, double jd) const override {
			// a day for a year: the sky of the progressed moment, and a year of real time is a day of its motion
			auto pp = m_calc.CalcPlanet(planet, FromJulian(m_natal + (jd - m_natal) / TropicalYear), 1, true);
			return { pp.Longitude.Value, pp.Speed / TropicalYear };
		}
		bool Slow() const override {
			return true;
		}

	private:
		AstroCalculator const& m_calc;
		double m_natal;
	};

	class NatalSource : public Source {
	public:
		explicit NatalSource(std::map<Planet, double> positions) : m_positions(std::move(positions)) {
		}
		Pos At(Planet planet, double) const override {
			return { m_positions.at(planet), 0 };
		}

	private:
		std::map<Planet, double> m_positions;
	};

	class SolarArcSource : public Source {
	public:
		SolarArcSource(AstroCalculator const& calc, std::map<Planet, double> natal, double natalJd, ArcKey key)
			: m_calc(calc), m_natal(std::move(natal)), m_natalJd(natalJd), m_key(key) {
		}
		Pos At(Planet planet, double jd) const override {
			auto [arc, rate] = Arc(jd);
			return { Wrap360(m_natal.at(planet) + arc), rate };
		}
		bool Slow() const override {
			return true;
		}

	private:
		// the arc at a moment and how fast it grows; the same moment is asked for once for every planet
		std::pair<double, double> Arc(double jd) const {
			if (jd == m_cachedJd)
				return m_cached;
			double years = (jd - m_natalJd) / TropicalYear, arc, rate;
			if (m_key == ArcKey::Naibod || m_key == ArcKey::Ptolemy) {
				double yearly = m_key == ArcKey::Naibod ? NaibodArc : PtolemyArc;
				arc = years * yearly;
				rate = yearly / TropicalYear;
			}
			else {
				// the distance the progressed Sun has gone since birth
				auto sun = m_calc.CalcPlanet(Planet::Sun, FromJulian(m_natalJd + years), 1, true);
				arc = fmod(sun.Longitude.Value - m_natal.at(Planet::Sun), 360);
				if (years >= 0 && arc < 0)
					arc += 360;
				else if (years < 0 && arc > 0)
					arc -= 360;
				rate = sun.Speed / TropicalYear;
			}
			m_cachedJd = jd;
			m_cached = { arc, rate };
			return m_cached;
		}

		AstroCalculator const& m_calc;
		std::map<Planet, double> m_natal;
		double m_natalJd;
		ArcKey m_key;
		mutable double m_cachedJd{ -1e300 };
		mutable std::pair<double, double> m_cached;
	};

	//
	// One planet of a source over the range. A slow source is worked out on a coarse grid and read between the points (cubic
	// Hermite: the values and speeds at the ends), which is far cheaper than asking the ephemeris at every step of a scan.
	// Fast() is for scanning; Exact() is for pinning an event down.
	//
	class Series {
	public:
		Series(Source const& source, Planet planet, double from, double to) : m_source(source), m_planet(planet) {
			if (source.Slow()) {
				m_t0 = from - Step;
				size_t count = static_cast<size_t>((to - from) / Step) + 4;
				m_points.resize(count);
				m_have.assign(count, false);
			}
		}

		Pos Fast(double jd) {
			if (m_points.empty())
				return m_source.At(m_planet, jd);

			double position = (jd - m_t0) / Step;
			size_t index = static_cast<size_t>(std::clamp(std::floor(position), 0.0, static_cast<double>(m_points.size() - 2)));
			double s = position - static_cast<double>(index);
			auto const& p0 = Point(index);
			auto const& p1 = Point(index + 1);
			double delta = Wrap180(p1.Lon - p0.Lon), v0 = p0.Speed * Step, v1 = p1.Speed * Step;
			double s2 = s * s, s3 = s2 * s;
			double value = (s3 - 2 * s2 + s) * v0 + (-2 * s3 + 3 * s2) * delta + (s3 - s2) * v1;
			double slope = ((3 * s2 - 4 * s + 1) * v0 + (-6 * s2 + 6 * s) * delta + (3 * s2 - 2 * s) * v1) / Step;
			return { Wrap360(p0.Lon + value), slope };
		}

		Pos Exact(double jd) const {
			return m_source.At(m_planet, jd);
		}

	private:
		static constexpr double Step = 10;		// days

		Pos const& Point(size_t index) {
			if (!m_have[index]) {
				m_points[index] = m_source.At(m_planet, m_t0 + index * Step);
				m_have[index] = true;
			}
			return m_points[index];
		}

		Source const& m_source;
		Planet m_planet;
		double m_t0{ 0 };
		std::vector<Pos> m_points;
		std::vector<bool> m_have;
	};

	// Illinois false position: a root of g between a and b, which g has opposite signs at (or zero).
	template<class F>
	double Root(F&& g, double a, double b, double ga, double gb, double tolerance) {
		if (ga == 0)
			return a;
		if (gb == 0)
			return b;
		int side = 0;
		for (int i = 0; i < 60; i++) {
			double c = (ga * b - gb * a) / (ga - gb);
			double gc = g(c);
			if (std::fabs(gc) < tolerance || std::fabs(b - a) < 1e-9)
				return c;
			if (gc * gb > 0) {
				b = c;
				gb = gc;
				if (side == -1)
					ga /= 2;
				side = -1;
			}
			else {
				a = c;
				ga = gc;
				if (side == 1)
					gb /= 2;
				side = 1;
			}
		}
		return (a + b) / 2;
	}

	constexpr double LongitudeTolerance = 1e-8;		// degrees

	// something a mover is compared with: a natal planet or angle (fixed), or a progressed planet (moving)
	struct Reference {
		AnalysisTarget Kind{ AnalysisTarget::Planet };
		Planet Body{ Planet::Sun };
		double Fixed{ 0 };
		std::unique_ptr<Series> Moving;

		double Lon(double jd) {
			return Moving ? Moving->Fast(jd).Lon : Fixed;
		}
		Pos Exact(double jd) const {
			return Moving ? Moving->Exact(jd) : Pos{ Fixed, 0 };
		}
	};

	// One thing to watch for between a mover and a reference: it happens when Wrap180(mover - reference - Delta) is zero.
	struct Spec {
		enum class Kind { Aspect, House, Sign } What{ Kind::Aspect };
		int Ref{ -1 };				// index into the references, or -1: a fixed longitude
		double Fixed{ 0 };
		double Delta{ 0 };
		double Orb{ 0 };			// aspects: the orb to enter and leave by
		AspectType Type{ AspectType::None };
		int Index{ 0 };				// the house (its cusp) or the sign (its start)
	};

	struct Sample {
		double T{ 0 };
		Pos Mover{ 0, 0 };
		std::vector<double> F;		// per spec
	};

	std::map<Planet, double> NatalLongitudes(AstroCalculator const& calc, ChartData const& natal, std::vector<Planet> const& wanted) {
		std::map<Planet, double> result;
		auto add = [&](Planet planet) {
			if (result.contains(planet))
				return;
			auto const& planets = natal.AllPlanets();
			if (auto it = std::find_if(planets.begin(), planets.end(), [&](auto const& p) { return p.Planet == planet; }); it != planets.end())
				result[planet] = it->Longitude.Value;
			else
				result[planet] = calc.CalcPlanet(planet, natal.Info().Time, 1, false).Longitude.Value;
		};
		for (auto planet : wanted)
			add(planet);
		add(Planet::Sun);		// the solar arc needs it
		return result;
	}

	class Scan {
	public:
		Scan(AstroCalculator const& calc, ChartData const& natal, AnalysisSettings const& settings, std::function<bool(double)> const& progress)
			: m_calc(calc), m_natal(natal), m_settings(settings), m_progress(progress), m_aspects(settings.Aspects),
			m_from(settings.From.Julian()), m_to(settings.To.Julian()) {
		}

		AnalysisResult Run();

	private:
		void BuildSources();
		void BuildReferences();
		std::vector<Spec> BuildSpecs(Planet mover) const;
		bool ScanMover(int index, int count, Planet mover);
		Sample Evaluate(Series& series, std::vector<Spec> const& specs, double t);
		double SpecValue(Spec const& spec, Pos mover, double refLon) const {
			return Wrap180(mover.Lon - refLon - spec.Delta);
		}
		// f of a spec at a time, worked out exactly
		double ExactF(Series const& mover, Spec const& spec, double t);
		void Advance(Series& mover, std::vector<Spec> const& specs, Planet planet, Sample const& a, Sample const& b);
		void Crossings(Series& mover, std::vector<Spec> const& specs, Planet planet, Sample const& a, Sample const& b);
		void Emit(AnalysisEventKind kind, Planet mover, Series const& series, Spec const* spec, double t);
		void Finish(AnalysisResult& result);

		AstroCalculator const& m_calc;
		ChartData const& m_natal;
		AnalysisSettings const& m_settings;
		std::function<bool(double)> const& m_progress;
		AspectCalculator m_aspects;
		double m_from, m_to;
		std::vector<Planet> m_movers;

		std::unique_ptr<Source> m_moverSource, m_targetSource;
		std::vector<Reference> m_references;
		std::map<Planet, std::unique_ptr<Series>> m_moverSeries;
		std::vector<AnalysisEvent> m_events;
		double m_stepK{ 1 };
		bool m_haveAspectSpecs{ false };
	};

	void Scan::BuildSources() {
		auto natalJd = m_natal.Info().Time.Julian();
		std::vector<Planet> wanted = m_movers;
		wanted.insert(wanted.end(), m_settings.Targets.begin(), m_settings.Targets.end());
		switch (m_settings.Type) {
			case AnalysisType::TransitsToNatal:
				m_moverSource = std::make_unique<TransitSource>(m_calc);
				break;
			case AnalysisType::TransitsToProgressed:
				m_moverSource = std::make_unique<TransitSource>(m_calc);
				m_targetSource = std::make_unique<ProgressedSource>(m_calc, natalJd);
				break;
			case AnalysisType::ProgressedToNatal:
			case AnalysisType::ProgressedToProgressed:
				m_moverSource = std::make_unique<ProgressedSource>(m_calc, natalJd);
				if (m_settings.Type == AnalysisType::ProgressedToProgressed)
					m_targetSource = std::make_unique<ProgressedSource>(m_calc, natalJd);
				break;
			case AnalysisType::SolarArcToNatal:
				m_moverSource = std::make_unique<SolarArcSource>(m_calc, NatalLongitudes(m_calc, m_natal, wanted), natalJd, m_settings.Key);
				break;
		}
	}

	void Scan::BuildReferences() {
		if (m_settings.NatalTargets()) {
			auto natal = NatalLongitudes(m_calc, m_natal, m_settings.Targets);
			for (auto planet : m_settings.Targets) {
				Reference ref;
				ref.Body = planet;
				ref.Fixed = natal.at(planet);
				m_references.push_back(std::move(ref));
			}
			if (m_settings.NatalAngles) {
				Reference asc, mc;
				asc.Kind = AnalysisTarget::Ascendant;
				asc.Fixed = m_natal.Houses().Asc.Value;
				mc.Kind = AnalysisTarget::Midheaven;
				mc.Fixed = m_natal.Houses().MC.Value;
				m_references.push_back(std::move(asc));
				m_references.push_back(std::move(mc));
			}
			return;
		}
		for (auto planet : m_settings.Targets) {
			Reference ref;
			ref.Body = planet;
			ref.Moving = std::make_unique<Series>(*m_targetSource, planet, m_from, m_to);
			m_references.push_back(std::move(ref));
		}
	}

	std::vector<Spec> Scan::BuildSpecs(Planet mover) const {
		std::vector<Spec> specs;
		if (m_settings.AspectEvents) {
			for (size_t r = 0; r < m_references.size(); r++) {
				auto const& ref = m_references[r];
				if (ref.Kind == AnalysisTarget::Planet && m_settings.Type == AnalysisType::ProgressedToProgressed) {
					// a pair is looked at once, whichever of the two is called the mover
					if (ref.Body == mover)
						continue;
					if (ref.Body < mover && std::find(m_movers.begin(), m_movers.end(), ref.Body) != m_movers.end())
						continue;
				}
				for (int i = 0; i < AspectSettings::AspectTypeCount; i++) {
					auto type = static_cast<AspectType>(i);
					auto orb = m_aspects.MaxOrbFor(type, mover, ref.Kind == AnalysisTarget::Planet ? std::optional<Planet>(ref.Body) : std::nullopt);
					if (orb <= 0)
						continue;
					double angle = AspectCalculator::GetAspectAngle(type);
					// the two sides of the target; the conjunction and the opposition have only one
					std::vector<double> deltas{ angle };
					if (angle != 0 && angle != 180)
						deltas.push_back(-angle);
					for (double delta : deltas) {
						Spec spec;
						spec.Ref = static_cast<int>(r);
						spec.Delta = delta;
						spec.Orb = orb;
						spec.Type = type;
						specs.push_back(spec);
					}
				}
			}
		}
		if (m_settings.HouseIngresses && m_settings.NatalTargets()) {
			for (int i = 0; i < 12; i++) {
				Spec spec;
				spec.What = Spec::Kind::House;
				spec.Fixed = m_natal.Houses().Cusps[i].Value;
				spec.Index = i;
				specs.push_back(spec);
			}
		}
		if (m_settings.SignIngresses) {
			for (int i = 0; i < 12; i++) {
				Spec spec;
				spec.What = Spec::Kind::Sign;
				spec.Fixed = 30.0 * i;
				spec.Index = i;
				specs.push_back(spec);
			}
		}
		return specs;
	}

	Sample Scan::Evaluate(Series& series, std::vector<Spec> const& specs, double t) {
		Sample sample;
		sample.T = t;
		sample.Mover = series.Fast(t);
		sample.F.resize(specs.size());
		for (size_t i = 0; i < specs.size(); i++) {
			auto const& spec = specs[i];
			double refLon = spec.Ref >= 0 ? m_references[spec.Ref].Lon(t) : spec.Fixed;
			sample.F[i] = SpecValue(spec, sample.Mover, refLon);
		}
		return sample;
	}

	double Scan::ExactF(Series const& mover, Spec const& spec, double t) {
		double refLon = spec.Ref >= 0 ? m_references[spec.Ref].Exact(t).Lon : spec.Fixed;
		return SpecValue(spec, mover.Exact(t), refLon);
	}

	void Scan::Emit(AnalysisEventKind kind, Planet mover, Series const& series, Spec const* spec, double t) {
		AnalysisEvent event;
		event.Type = m_settings.Type;
		event.Time = FromJulian(t);
		event.Kind = kind;
		event.Mover = mover;
		auto pos = series.Exact(t);
		event.Retrograde = pos.Speed < 0;
		event.Longitude = pos.Lon;
		if (spec) {
			if (spec->What == Spec::Kind::Aspect) {
				auto const& ref = m_references[spec->Ref];
				event.TargetKind = ref.Kind;
				event.Target = ref.Body;
				event.Aspect = spec->Type;
				event.Orb = spec->Orb;
			}
			else {
				// forward over the start of a house or sign enters it; backward enters the one before
				event.Index = event.Retrograde ? (spec->Index + 11) % 12 : spec->Index;
				if (spec->What == Spec::Kind::House)
					event.Index++;
			}
		}
		m_events.push_back(event);
	}

	void Scan::Crossings(Series& mover, std::vector<Spec> const& specs, Planet planet, Sample const& a, Sample const& b) {
		for (size_t i = 0; i < specs.size(); i++) {
			auto const& spec = specs[i];
			double fa = a.F[i], fb = b.F[i];
			auto f = [&](double t) { return ExactF(mover, spec, t); };

			// exact (or, for an ingress, the crossing): the sign of the difference changes, near zero rather than at the wrap
			bool crossed = std::fabs(fa) < 90 && std::fabs(fb) < 90 && ((fa < 0 && fb >= 0) || (fa > 0 && fb <= 0));
			double exact = 0;
			if (crossed)
				exact = Root(f, a.T, b.T, fa, fb, LongitudeTolerance);

			if (spec.What != Spec::Kind::Aspect) {
				if (crossed)
					Emit(spec.What == Spec::Kind::House ? AnalysisEventKind::HouseIngress : AnalysisEventKind::SignIngress, planet, mover, &spec, exact);
				continue;
			}

			if (crossed)
				Emit(AnalysisEventKind::Exact, planet, mover, &spec, exact);

			// entering and leaving the orb, on the way to and from the exact moment if there was one in between
			auto g = [&](double t) { return std::fabs(f(t)) - spec.Orb; };
			double ga = std::fabs(fa) - spec.Orb, gb = std::fabs(fb) - spec.Orb;
			if (crossed) {
				if (ga > 0)
					Emit(AnalysisEventKind::EnterOrb, planet, mover, &spec, Root(g, a.T, exact, ga, -spec.Orb, LongitudeTolerance));
				if (gb > 0)
					Emit(AnalysisEventKind::LeaveOrb, planet, mover, &spec, Root(g, exact, b.T, -spec.Orb, gb, LongitudeTolerance));
			}
			else if (ga > 0 && gb <= 0)
				Emit(AnalysisEventKind::EnterOrb, planet, mover, &spec, Root(g, a.T, b.T, ga, gb, LongitudeTolerance));
			else if (ga <= 0 && gb > 0)
				Emit(AnalysisEventKind::LeaveOrb, planet, mover, &spec, Root(g, a.T, b.T, ga, gb, LongitudeTolerance));
		}
	}

	void Scan::Advance(Series& mover, std::vector<Spec> const& specs, Planet planet, Sample const& a, Sample const& b) {
		// A station in between splits the step: a planet can touch an orb and turn back without ever being exact, and the
		// difference is not monotonic across it.
		if (a.Mover.Speed * b.Mover.Speed < 0) {
			auto speed = [&](double t) { return mover.Exact(t).Speed; };
			double station = Root(speed, a.T, b.T, a.Mover.Speed, b.Mover.Speed, 1e-9);
			if (m_settings.Stations) {
				bool turnsRetrograde = a.Mover.Speed > 0;
				Emit(turnsRetrograde ? AnalysisEventKind::StationRetrograde : AnalysisEventKind::StationDirect, planet, mover, nullptr, station);
				m_events.back().Retrograde = turnsRetrograde;
			}
			if (station > a.T && station < b.T) {
				auto middle = Evaluate(mover, specs, station);
				Crossings(mover, specs, planet, a, middle);
				Crossings(mover, specs, planet, middle, b);
				return;
			}
		}
		Crossings(mover, specs, planet, a, b);
	}

	bool Scan::ScanMover(int index, int count, Planet planet) {
		auto& series = *m_moverSeries.at(planet);
		auto specs = BuildSpecs(planet);
		if (specs.empty() && !m_settings.Stations)
			return true;

		auto reportProgress = [&](double t) {
			return !m_progress || m_progress((index + (t - m_from) / (m_to - m_from)) / count);
		};

		double t = m_from;
		auto previous = Evaluate(series, specs, t);
		for (size_t i = 0; i < specs.size(); i++)
			if (specs[i].What == Spec::Kind::Aspect && std::fabs(previous.F[i]) <= specs[i].Orb)
				Emit(AnalysisEventKind::InOrbAtStart, planet, series, &specs[i], t);

		int steps = 0;
		while (t < m_to) {
			// how far the mover and the fastest reference move relative to each other in a step: a fraction of the smallest orb
			double fastestReference = 0;
			for (auto& ref : m_references)
				if (ref.Moving)
					fastestReference = std::max(fastestReference, std::fabs(ref.Moving->Fast(t).Speed));
			double relative = std::fabs(previous.Mover.Speed) + fastestReference;
			double step = std::clamp(m_stepK / std::max(relative, 1e-6), 0.001, 20.0);
			double next = std::min(m_to, t + step);

			auto current = Evaluate(series, specs, next);
			Advance(series, specs, planet, previous, current);
			previous = std::move(current);
			t = next;
			if ((++steps & 127) == 0 && !reportProgress(t))
				return false;
		}
		for (size_t i = 0; i < specs.size(); i++)
			if (specs[i].What == Spec::Kind::Aspect && std::fabs(previous.F[i]) <= specs[i].Orb)
				Emit(AnalysisEventKind::InOrbAtEnd, planet, series, &specs[i], t);
		return reportProgress(m_to);
	}

	void Scan::Finish(AnalysisResult& result) {
		std::stable_sort(m_events.begin(), m_events.end(), [](AnalysisEvent const& a, AnalysisEvent const& b) {
			return a.Time.Julian() < b.Time.Julian();
		});

		// which pass of a retrograde planet each aspect event belongs to, for each mover, target and aspect
		using Key = std::tuple<Planet, AnalysisTarget, Planet, AspectType>;
		std::map<Key, int> passes;
		for (auto& event : m_events) {
			if (event.Aspect == AspectType::None)
				continue;
			int& pass = passes[{ event.Mover, event.TargetKind, event.Target, event.Aspect }];
			if (event.Kind == AnalysisEventKind::EnterOrb || event.Kind == AnalysisEventKind::InOrbAtStart || pass == 0)
				pass++;
			event.Pass = pass;
		}
		// the stays within an orb, gathered as their events go by and given to the event that ends each
		struct Stay {
			std::shared_ptr<AnalysisWindow> Window;
		};
		std::map<Key, Stay> stays;
		for (auto& event : m_events) {
			if (event.Aspect == AspectType::None)
				continue;
			auto& stay = stays[{ event.Mover, event.TargetKind, event.Target, event.Aspect }];
			switch (event.Kind) {
				case AnalysisEventKind::EnterOrb:
					stay.Window = std::make_shared<AnalysisWindow>();
					stay.Window->HasEnter = true;
					stay.Window->Enter = event.Time;
					break;
				case AnalysisEventKind::InOrbAtStart:
					stay.Window = std::make_shared<AnalysisWindow>();
					break;
				case AnalysisEventKind::Exact:
					if (stay.Window)
						stay.Window->Exacts.push_back(event.Time);
					break;
				case AnalysisEventKind::LeaveOrb:
				case AnalysisEventKind::InOrbAtEnd:
					if (stay.Window) {
						stay.Window->HasLeave = event.Kind == AnalysisEventKind::LeaveOrb;
						stay.Window->Leave = event.Time;
						event.Window = std::move(stay.Window);
					}
					break;
			}
		}
		result.Events = std::move(m_events);
	}

	AnalysisResult Scan::Run() {
		AnalysisResult result;
		if (!(m_to > m_from))
			return result;
		m_movers = m_settings.EffectiveMovers();
		BuildSources();
		BuildReferences();

		// the smallest orb sets how finely to look, so that nothing narrower than a step is missed
		double smallest = 1000;
		for (auto planet : m_movers)
			for (auto const& ref : m_references)
				for (int i = 0; i < AspectSettings::AspectTypeCount; i++)
					if (auto orb = m_aspects.MaxOrbFor(static_cast<AspectType>(i), planet, ref.Kind == AnalysisTarget::Planet ? std::optional<Planet>(ref.Body) : std::nullopt); orb > 0)
						smallest = std::min<double>(smallest, orb * 2);
		m_stepK = std::clamp(smallest / 4, 0.02, 1.0);

		for (auto planet : m_movers)
			m_moverSeries[planet] = std::make_unique<Series>(*m_moverSource, planet, m_from, m_to);
		int count = static_cast<int>(m_movers.size());
		for (int i = 0; i < count; i++)
			if (!ScanMover(i, count, m_movers[i])) {
				result.Cancelled = true;
				return result;
			}
		Finish(result);
		return result;
	}
}

std::vector<Planet> AnalysisSettings::EffectiveMovers() const {
	std::vector<Planet> movers;
	bool dropMoon = MoonDropped();
	for (auto planet : Movers)
		if (!(dropMoon && planet == Planet::Moon) && std::find(movers.begin(), movers.end(), planet) == movers.end())
			movers.push_back(planet);
	return movers;
}

AnalysisResult Analysis::Run(AstroCalculator const& calc, ChartData const& natal, AnalysisSettings const& settings,
	std::function<bool(double)> const& progress) {
	Scan scan(calc, natal, settings, progress);
	return scan.Run();
}

AnalysisResult Analysis::RunAll(AstroCalculator const& calc, ChartData const& natal, AnalysisSettings const& settings,
	std::function<bool(double)> const& progress) {
	auto types = settings.TypeList();
	AnalysisResult all;
	bool moverSeen[3]{};
	for (size_t i = 0; i < types.size(); i++) {
		AnalysisSettings one = settings;
		one.Type = types[i];
		one.Types.clear();
		int kind = types[i] == AnalysisType::TransitsToNatal || types[i] == AnalysisType::TransitsToProgressed ? 0 :
			types[i] == AnalysisType::SolarArcToNatal ? 2 : 1;
		if (moverSeen[kind]) {
			one.SignIngresses = false;
			one.Stations = false;
		}
		moverSeen[kind] = true;

		std::function<bool(double)> part;
		if (progress)
			part = [&](double fraction) { return progress((static_cast<double>(i) + fraction) / static_cast<double>(types.size())); };
		auto result = Run(calc, natal, one, part);
		if (result.Cancelled) {
			all.Events.clear();
			all.Cancelled = true;
			return all;
		}
		all.Events.insert(all.Events.end(), result.Events.begin(), result.Events.end());
	}
	std::stable_sort(all.Events.begin(), all.Events.end(), [](AnalysisEvent const& a, AnalysisEvent const& b) {
		return a.Time.Julian() < b.Time.Julian();
	});
	return all;
}

namespace {
	template<class T>
	std::wstring NumberList(std::vector<T> const& values) {
		std::wstring text;
		for (auto value : values)
			text += (text.empty() ? L"" : L",") + std::to_wstring(static_cast<int>(value));
		return text;
	}

	// the numbers in a comma separated list that are in 0 .. below
	std::vector<int> ReadNumbers(std::wstring const& text, int below) {
		std::vector<int> numbers;
		size_t start = 0;
		while (start <= text.size()) {
			size_t end = text.find(L',', start);
			if (end == std::wstring::npos)
				end = text.size();
			auto item = text.substr(start, end - start);
			wchar_t* stop = nullptr;
			long value = wcstol(item.c_str(), &stop, 10);
			if (!item.empty() && *stop == 0 && value >= 0 && value < below)
				numbers.push_back(static_cast<int>(value));
			start = end + 1;
		}
		return numbers;
	}
}

std::wstring AnalysisSettings::ToText() const {
	std::vector<int> aspects;
	for (int i = 0; i < AspectSettings::AspectTypeCount; i++)
		if (Aspects.AspectEnabled[i] && !(Aspects.MajorOnly && i > static_cast<int>(AspectType::Opposition)))
			aspects.push_back(i);
	int days = static_cast<int>(std::lround(To.Julian() - From.Julian()));
	return L"types=" + NumberList(TypeList()) + L";movers=" + NumberList(Movers) + L";targets=" + NumberList(Targets) +
		L";angles=" + std::to_wstring(NatalAngles ? 1 : 0) + L";aspects=" + std::to_wstring(AspectEvents ? 1 : 0) + L";asp=" + NumberList(aspects) +
		L";houses=" + std::to_wstring(HouseIngresses ? 1 : 0) + L";signs=" + std::to_wstring(SignIngresses ? 1 : 0) +
		L";stations=" + std::to_wstring(Stations ? 1 : 0) + L";days=" + std::to_wstring(std::max(days, 1));
}

void AnalysisSettings::FromText(std::wstring const& text) {
	size_t start = 0;
	while (start < text.size()) {
		size_t end = text.find(L';', start);
		if (end == std::wstring::npos)
			end = text.size();
		auto part = text.substr(start, end - start);
		start = end + 1;
		auto equals = part.find(L'=');
		if (equals == std::wstring::npos)
			continue;
		auto key = part.substr(0, equals), value = part.substr(equals + 1);
		auto flag = [&] { return value == L"1"; };
		if (key == L"types") {
			Types.clear();
			for (int i : ReadNumbers(value, 5))
				Types.push_back(static_cast<AnalysisType>(i));
			if (!Types.empty())
				Type = Types[0];
		}
		else if (key == L"movers") {
			Movers.clear();
			for (int i : ReadNumbers(value, static_cast<int>(Planet::NumPlanets)))
				Movers.push_back(static_cast<Planet>(i));
		}
		else if (key == L"targets") {
			Targets.clear();
			for (int i : ReadNumbers(value, static_cast<int>(Planet::NumPlanets)))
				Targets.push_back(static_cast<Planet>(i));
		}
		else if (key == L"angles")
			NatalAngles = flag();
		else if (key == L"aspects")
			AspectEvents = flag();
		else if (key == L"asp") {
			Aspects.AspectEnabled.fill(false);
			Aspects.MajorOnly = false;
			for (int i : ReadNumbers(value, AspectSettings::AspectTypeCount))
				Aspects.AspectEnabled[i] = true;
		}
		else if (key == L"houses")
			HouseIngresses = flag();
		else if (key == L"signs")
			SignIngresses = flag();
		else if (key == L"stations")
			Stations = flag();
		else if (key == L"days") {
			wchar_t* stop = nullptr;
			long days = wcstol(value.c_str(), &stop, 10);
			if (*stop == 0 && days >= 1 && days <= 40000)
				To = From.AddDays(static_cast<double>(days));
		}
	}
}
