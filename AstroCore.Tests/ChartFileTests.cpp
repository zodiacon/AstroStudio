// ChartFile (AstroStudio/ChartFile.cpp, built into the tests with the few app files it needs): chart, derived chart and analysis
// files written and read back, and the links of a derived chart to the files of the charts it was made from.
#include "TestCommon.h"
#include "ChartFile.h"
#include "Project.h"
#include <filesystem>
#include <fstream>
#include <random>

namespace fs = std::filesystem;

namespace {
	std::string Narrow(std::wstring const& text) {
		std::string result;
		for (auto ch : text)
			result += ch < 128 ? (char)ch : '?';
		return result;
	}

	struct TempFolder {
		fs::path Path;
		TempFolder() {
			std::random_device random;
			// a space in the name on purpose: paths must be kept as they are
			Path = fs::temp_directory_path() / (L"astro chartfile test " + std::to_wstring(random()) + L"_" + std::to_wstring(random()));
			fs::create_directories(Path);
		}
		~TempFolder() {
			std::error_code error;
			fs::remove_all(Path, error);
		}
		fs::path operator/(std::wstring const& relative) const {
			return Path / relative;
		}
	};

	ChartData Person(std::wstring first, int year, double latitude, int offsetMinutes = 120) {
		ChartData chart;
		chart.AddPlanets(std::vector<Planet>{ Planet::Sun, Planet::Moon, Planet::Mercury, Planet::Venus, Planet::Mars, Planet::Jupiter,
			Planet::Saturn, Planet::Uranus, Planet::Neptune, Planet::Pluto });
		chart.SetHouseSystem(HouseSystem::Placidus);
		auto& info = chart.Info();
		info.FirstName = std::move(first);
		info.LastName = L"Tester";
		info.Type = InfoType::Female;
		info.Time = DateTime(year, 6, 15, 14, 30, 0);
		info.TimeZone.Name.clear();			// a fixed offset: the same on every computer
		info.TimeZone.OffsetUT = offsetMinutes;
		info.Latitude = latitude;
		info.Longitude = 34.78;
		info.Elevation = 0;
		info.City = L"Tel Aviv";
		info.Country = L"Israel";
		return chart;
	}

	void Save(ChartData const& chart, fs::path const& path) {
		std::wstring error;
		bool ok = ChartFile::Save(chart, path.c_str(), error);
		INFO(Narrow(error));
		REQUIRE(ok);
	}

	ChartData LoadChart(fs::path const& path) {
		ChartData chart;
		std::wstring error;
		bool ok = ChartFile::Load(path.c_str(), chart, error);
		INFO(Narrow(error));
		REQUIRE(ok);
		return chart;
	}

	ChartFile::Loaded LoadAny(fs::path const& path) {
		ChartFile::Loaded loaded;
		std::wstring error;
		bool ok = ChartFile::LoadFile(path.c_str(), loaded, error);
		INFO(Narrow(error));
		REQUIRE(ok);
		return loaded;
	}

	std::string Read(fs::path const& path) {
		std::ifstream in(path, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	}

	void Write(fs::path const& path, std::string const& text) {
		std::ofstream(path, std::ios::binary) << text;
	}

	bool Same(fs::path const& a, std::wstring const& b) {
		return _wcsicmp(fs::path(a).lexically_normal().c_str(), fs::path(b).lexically_normal().c_str()) == 0;
	}

	// a composite of two saved charts, saved as a derived chart file
	DerivedRecipe Composite(ChartData const& a, fs::path const& aPath, ChartData const& b, fs::path const& bPath) {
		DerivedRecipe recipe;
		recipe.Kind = DerivedKind::Composite;
		recipe.A = a;
		recipe.B = b;
		recipe.APath = aPath.wstring();
		recipe.BPath = bPath.wstring();
		recipe.Id = Project::NewId();
		return recipe;
	}

	void SaveDerived(DerivedRecipe const& recipe, fs::path const& path) {
		std::wstring error;
		bool ok = ChartFile::SaveDerived(recipe, path.c_str(), error);
		INFO(Narrow(error));
		REQUIRE(ok);
	}
}

TEST_CASE("A chart file reads back as it was written", "[ChartFile]") {
	TempFolder folder;
	auto chart = Person(L"Anna", 1980, 32.08, 180);
	chart.Harmonic(5);
	chart.SetFileId(Project::NewId());
	auto path = folder / L"Anna.chart";
	Save(chart, path);

	auto loaded = LoadChart(path);
	auto const& a = chart.Info();
	auto const& b = loaded.Info();
	CHECK(b.FirstName == a.FirstName);
	CHECK(b.LastName == a.LastName);
	CHECK(b.Type == a.Type);
	CHECK(b.City == a.City);
	CHECK(b.Country == a.Country);
	CHECK(b.Latitude == Approx(a.Latitude).margin(1e-6));
	CHECK(b.Longitude == Approx(a.Longitude).margin(1e-6));
	CHECK(b.Time.Julian() == Approx(a.Time.Julian()).margin(1.0 / 86400));
	CHECK(b.TimeZone.Name.empty());
	CHECK(b.TimeZone.OffsetUT == 180);
	CHECK(loaded.GetHouseSystem() == HouseSystem::Placidus);
	CHECK(loaded.Harmonic() == 5);
	CHECK(loaded.PlanetCount() == chart.PlanetCount());
	CHECK(_wcsicmp(loaded.FileId().c_str(), chart.FileId().c_str()) == 0);
	// positions are never stored
	CHECK(Read(path).find("Longitude") != std::string::npos);
	CHECK(Read(path).find("Sun=") == std::string::npos);
}

TEST_CASE("A chart file without an id loads with none", "[ChartFile]") {
	TempFolder folder;
	auto path = folder / L"NoId.chart";
	Save(Person(L"Ben", 1990, 40.7), path);
	CHECK(Read(path).find("Id=") == std::string::npos);
	CHECK(LoadChart(path).FileId().empty());
}

TEST_CASE("A chart file that is wrong is refused with the place", "[ChartFile]") {
	TempFolder folder;
	auto path = folder / L"Bad.chart";
	Save(Person(L"Cleo", 1975, 51.5), path);
	auto text = Read(path);
	auto at = text.find("Latitude=");
	REQUIRE(at != std::string::npos);
	text.insert(at + 9, "north ");
	Write(path, text);

	ChartData chart;
	std::wstring error;
	CHECK_FALSE(ChartFile::Load(path.c_str(), chart, error));
	CHECK(error.find(L"Latitude") != std::wstring::npos);
}

TEST_CASE("A derived chart file is not a plain chart", "[ChartFile]") {
	TempFolder folder;
	auto a = Person(L"Dan", 1970, 32.0), b = Person(L"Eve", 1972, 31.0);
	auto path = folder / L"Composite.chart";
	DerivedRecipe recipe;
	recipe.A = a;
	recipe.B = b;
	SaveDerived(recipe, path);

	ChartData chart;
	std::wstring error;
	CHECK_FALSE(ChartFile::Load(path.c_str(), chart, error));
	auto loaded = LoadAny(path);
	CHECK(loaded.Derived);
	CHECK(loaded.Recipe.Kind == DerivedKind::Composite);
	CHECK(loaded.Recipe.A.Info().FirstName == L"Dan");
	CHECK(loaded.Recipe.B.Info().FirstName == L"Eve");
	// charts that had no file: the copies are all there is, and that is nothing to report
	CHECK(loaded.Unlinked.empty());
	CHECK(loaded.Recipe.APath.empty());
}

TEST_CASE("Every kind of derived chart reads back", "[ChartFile]") {
	TempFolder folder;
	auto natal = Person(L"Fay", 1985, 48.85);
	auto check = [&](DerivedRecipe recipe) {
		recipe.Id = Project::NewId();
		auto path = folder / L"Derived.chart";
		SaveDerived(recipe, path);
		auto loaded = LoadAny(path);
		REQUIRE(loaded.Derived);
		auto const& r = loaded.Recipe;
		CHECK(r.Kind == recipe.Kind);
		// each kind keeps only what it uses
		if (recipe.Kind == DerivedKind::Composite)
			CHECK(r.Houses == recipe.Houses);
		if (recipe.Kind == DerivedKind::SolarArc || recipe.Kind == DerivedKind::Primary)
			CHECK(r.Key == recipe.Key);
		if (recipe.Kind == DerivedKind::Progressed)
			CHECK(r.Angles == recipe.Angles);
		CHECK(_wcsicmp(r.Id.c_str(), recipe.Id.c_str()) == 0);
		if (recipe.HasTarget()) {
			CHECK(r.Target.Julian() == Approx(recipe.Target.Julian()).margin(1.0 / 86400));
			CHECK(r.Zone.OffsetUT == recipe.Zone.OffsetUT);
		}
		if (recipe.Kind == DerivedKind::Relocated) {
			CHECK(r.Place.Latitude == Approx(recipe.Place.Latitude));
			CHECK(r.Place.Longitude == Approx(recipe.Place.Longitude));
			CHECK(r.Place.Elevation == Approx(recipe.Place.Elevation));
			CHECK(r.Place.City == recipe.Place.City);
			CHECK(r.Place.Country == recipe.Place.Country);
			CHECK(r.Zone.Name == recipe.Zone.Name);
			if (recipe.Zone.Name.empty())
				CHECK(r.Zone.OffsetUT == recipe.Zone.OffsetUT);
			// the birth stays the source's
			CHECK(r.A.Info().Time.Julian() == Approx(recipe.A.Info().Time.Julian()).margin(1.0 / 86400));
			CHECK(r.A.Info().Latitude == Approx(recipe.A.Info().Latitude));
		}
	};
	DerivedRecipe recipe;
	recipe.A = natal;
	recipe.B = Person(L"Gil", 1983, 52.5);
	recipe.Kind = DerivedKind::Composite;
	recipe.Houses = CompositeHouses::MidpointARMC;
	check(recipe);
	recipe.Kind = DerivedKind::Davison;
	check(recipe);

	recipe.B = {};
	recipe.Target = DateTime(2030, 3, 1, 10, 0, 0);
	recipe.Zone.Name.clear();
	recipe.Zone.OffsetUT = 60;
	recipe.Kind = DerivedKind::SolarArc;
	recipe.Key = ArcKey::Naibod;
	check(recipe);
	recipe.Kind = DerivedKind::Primary;
	recipe.Key = ArcKey::Ptolemy;
	check(recipe);
	recipe.Kind = DerivedKind::Progressed;
	recipe.Angles = ProgressedAngles::SolarArc;
	check(recipe);

	recipe.Kind = DerivedKind::Relocated;
	recipe.Place.Latitude = -33.8688;
	recipe.Place.Longitude = 151.2093;
	recipe.Place.Elevation = 58;
	recipe.Place.City = L"Sydney";
	recipe.Place.Country = L"Australia";
	recipe.Zone = {};
	recipe.Zone.OffsetUT = 600;		// a fixed offset
	check(recipe);
	recipe.Zone.Name = L"Tokyo Standard Time";		// a zone of its own
	recipe.Zone.OffsetUT = 540;
	check(recipe);
}

TEST_CASE("A relocated chart's file needs its place", "[ChartFile]") {
	TempFolder folder;
	DerivedRecipe recipe;
	recipe.Kind = DerivedKind::Relocated;
	recipe.A = Person(L"Jo", 1990, 40.0);
	recipe.Place.Latitude = 10;
	recipe.Place.Longitude = 20;
	recipe.Zone.OffsetUT = 60;
	auto path = folder / L"Relocated.chart";
	SaveDerived(recipe, path);
	auto text = Read(path);
	auto at = text.find("Latitude=");		// (the first is the new place's: [Derived.Location] comes before the source chart)
	REQUIRE(at != std::string::npos);
	REQUIRE(text.rfind("[Derived.Location]", at) != std::string::npos);
	text.insert(at + 9, "north ");
	Write(path, text);
	ChartFile::Loaded loaded;
	std::wstring error;
	CHECK_FALSE(ChartFile::LoadFile(path.c_str(), loaded, error));
	CHECK(error.find(L"[Derived.Location] Latitude") != std::wstring::npos);
}

TEST_CASE("A derived chart reads its sources from their files", "[ChartFile]") {
	TempFolder folder;
	auto a = Person(L"Hana", 1980, 32.0), b = Person(L"Ido", 1978, 31.5);
	a.SetFileId(Project::NewId());
	b.SetFileId(Project::NewId());
	fs::create_directories(folder / L"My Charts");
	auto aPath = folder / L"My Charts/Hana.chart", bPath = folder / L"My Charts/Ido.chart";
	Save(a, aPath);
	Save(b, bPath);
	auto path = folder / L"Hana and Ido.chart";
	SaveDerived(Composite(a, aPath, b, bPath), path);

	SECTION("the paths are kept relative to the derived file") {
		auto text = Read(path);
		CHECK(text.find("ASource=My Charts\\Hana.chart") != std::string::npos);
		CHECK(text.find("BSource=My Charts\\Ido.chart") != std::string::npos);
		auto loaded = LoadAny(path);
		CHECK(loaded.Unlinked.empty());
		CHECK(Same(aPath, loaded.Recipe.APath));
		CHECK(Same(bPath, loaded.Recipe.BPath));
	}
	SECTION("a source edited since is read as it is now") {
		a.Info().FirstName = L"Hannah";
		a.Info().Latitude = 33.0;
		Save(a, aPath);
		auto loaded = LoadAny(path);
		CHECK(loaded.Unlinked.empty());
		CHECK(loaded.Recipe.A.Info().FirstName == L"Hannah");
		CHECK(loaded.Recipe.A.Info().Latitude == Approx(33.0));
		CHECK(loaded.Recipe.B.Info().FirstName == L"Ido");
	}
	SECTION("a source moved within its folder is found by its id") {
		auto moved = folder / L"My Charts/Hana (renamed).chart";
		fs::rename(aPath, moved);
		auto loaded = LoadAny(path);
		CHECK(loaded.Unlinked.empty());
		CHECK(Same(moved, loaded.Recipe.APath));
	}
	SECTION("a source moved next to the derived file is found by its id") {
		auto moved = folder / L"Hana.chart";
		fs::rename(aPath, moved);
		auto loaded = LoadAny(path);
		CHECK(loaded.Unlinked.empty());
		CHECK(Same(moved, loaded.Recipe.APath));
	}
	SECTION("the derived file and its sources moved together still link") {
		auto other = folder / L"elsewhere";
		fs::create_directories(other);
		fs::rename(folder / L"My Charts", other / L"My Charts");
		fs::rename(path, other / L"Hana and Ido.chart");
		auto loaded = LoadAny(other / L"Hana and Ido.chart");
		CHECK(loaded.Unlinked.empty());
		CHECK(Same(other / L"My Charts/Hana.chart", loaded.Recipe.APath));
	}
	SECTION("another chart under the source's name is not taken for it") {
		auto stranger = Person(L"Stranger", 1999, 10.0);
		stranger.SetFileId(Project::NewId());
		fs::remove(aPath);
		Save(stranger, aPath);
		auto loaded = LoadAny(path);
		REQUIRE(loaded.Unlinked.size() == 1);
		CHECK(loaded.Recipe.A.Info().FirstName == L"Hana");		// the copy in the derived file
		CHECK(loaded.Recipe.B.Info().FirstName == L"Ido");
	}
	SECTION("a source that is gone falls back to the copy and is reported") {
		fs::remove(bPath);
		auto loaded = LoadAny(path);
		REQUIRE(loaded.Unlinked.size() == 1);
		CHECK(loaded.Unlinked[0].find(L"Ido.chart") != std::wstring::npos);
		CHECK(loaded.Recipe.B.Info().FirstName == L"Ido");
		CHECK(loaded.Recipe.A.Info().FirstName == L"Hana");
	}
}

TEST_CASE("A source on another drive keeps its full path", "[ChartFile]") {
	TempFolder folder;
	auto a = Person(L"Jon", 1960, 0.0), b = Person(L"Kim", 1961, 1.0);
	auto path = folder / L"Pair.chart";
	auto recipe = Composite(a, L"Q:\\Nowhere\\Jon.chart", b, L"Q:\\Nowhere\\Kim.chart");
	SaveDerived(recipe, path);
	CHECK(Read(path).find("ASource=Q:\\Nowhere\\Jon.chart") != std::string::npos);
	auto loaded = LoadAny(path);
	CHECK(loaded.Unlinked.size() == 2);
	CHECK(loaded.Recipe.APath == L"Q:\\Nowhere\\Jon.chart");
}

TEST_CASE("An analysis file reads back", "[ChartFile]") {
	TempFolder folder;
	AnalysisDocument document;
	document.Id = Project::NewId();
	document.ChartName = L"Lea Tester";
	document.Chart = Person(L"Lea", 1990, 45.0);
	document.Settings.Type = AnalysisType::TransitsToNatal;
	document.Settings.From = DateTime(2026, 1, 1, 0, 0, 0);
	document.Settings.To = DateTime(2027, 1, 1, 0, 0, 0);
	AnalysisEvent event;
	event.Time = DateTime(2026, 1, 2, 3, 4, 5);
	event.Kind = AnalysisEventKind::Exact;
	event.Mover = Planet::Saturn;
	event.Aspect = AspectType::Square;
	document.Events.push_back(event);
	auto path = folder / L"Lea.analysis";
	std::wstring error;
	REQUIRE(ChartFile::SaveAnalysis(document, path.c_str(), error));

	AnalysisDocument loaded;
	bool ok = ChartFile::LoadAnalysis(path.c_str(), loaded, error);
	INFO(Narrow(error));
	REQUIRE(ok);
	CHECK(_wcsicmp(loaded.Id.c_str(), document.Id.c_str()) == 0);
	CHECK(loaded.ChartName == L"Lea Tester");
	CHECK(loaded.Chart.Info().FirstName == L"Lea");
	REQUIRE(loaded.Events.size() == 1);
	CHECK(loaded.Events[0].Mover == Planet::Saturn);
	CHECK(loaded.Events[0].Aspect == AspectType::Square);
	CHECK(loaded.Events[0].Time.Julian() == Approx(event.Time.Julian()).margin(1e-6));
	CHECK(_wcsicmp(Project::ReadFileId(path).c_str(), document.Id.c_str()) == 0);
}
