#include "TestCommon.h"
#include "Project.h"
#include <filesystem>
#include <fstream>
#include <random>

namespace fs = std::filesystem;

namespace {
	// a folder of its own for a test, removed afterwards
	struct TempFolder {
		fs::path Path;
		TempFolder() {
			std::random_device random;
			Path = fs::temp_directory_path() / (L"astro_project_test_" + std::to_wstring(random()) + L"_" + std::to_wstring(random()));
			fs::create_directories(Path);
		}
		~TempFolder() {
			std::error_code error;
			fs::remove_all(Path, error);
		}
		fs::path Make(std::wstring const& relative, std::string const& text = "; a file\n") const {
			auto path = Path / relative;
			fs::create_directories(path.parent_path());
			std::ofstream(path, std::ios::binary) << text;
			return path;
		}
		fs::path operator/(std::wstring const& relative) const {
			return Path / relative;
		}
	};

	std::string Narrow(std::wstring const& text) {
		return std::string(text.begin(), text.end());
	}

	std::string Read(fs::path const& path) {
		std::ifstream in(path, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	}
}

TEST_CASE("Files are typed by their extension", "[Project]") {
	CHECK(Project::TypeOfFile(L"a.chart") == ProjectItemType::Chart);
	CHECK(Project::TypeOfFile(L"C:\\x\\A.CHART") == ProjectItemType::Chart);
	CHECK(Project::TypeOfFile(L"a.analysis") == ProjectItemType::Analysis);
	CHECK(Project::TypeOfFile(L"a.aspects") == ProjectItemType::AspectSet);
	CHECK(Project::TypeOfFile(L"a.colors") == ProjectItemType::ColorSet);
	CHECK(Project::TypeOfFile(L"a.txt") == ProjectItemType::Other);
	CHECK(Project::TypeOfFile(L"chart") == ProjectItemType::Other);
}

TEST_CASE("Ids are unique and shaped like GUIDs", "[Project]") {
	auto a = Project::NewId(), b = Project::NewId();
	CHECK(a != b);
	CHECK(a.size() == 36);
	CHECK(a[8] == L'-');
	CHECK(a[13] == L'-');
	CHECK(a[14] == L'4');
	CHECK(a[18] == L'-');
	CHECK(a[23] == L'-');
}

TEST_CASE("Group paths are put in one form", "[Project]") {
	CHECK(Project::NormalizeGroup(L"Clients") == L"Clients");
	CHECK(Project::NormalizeGroup(L" Clients \\ Smith/") == L"Clients/Smith");
	CHECK(Project::NormalizeGroup(L"//A//B//") == L"A/B");
	CHECK(Project::NormalizeGroup(L"").empty());
	CHECK(Project::NormalizeGroup(L" / ").empty());
}

TEST_CASE("Linking files", "[Project]") {
	TempFolder folder;
	auto one = folder.Make(L"charts\\one.chart");
	auto two = folder.Make(L"charts\\two.chart");
	Project project;
	CHECK_FALSE(project.Dirty());

	auto first = project.Add(one);
	REQUIRE(first != nullptr);
	CHECK(first->Name == L"one");
	CHECK(first->Type == ProjectItemType::Chart);
	CHECK_FALSE(first->Missing);
	CHECK(first->Id.size() == 36);
	CHECK(project.Dirty());
	CHECK(project.Resolve(*first) == one.lexically_normal());

	// the same file again - however it is spelled - is the same item
	auto id = first->Id;
	CHECK(project.Add(one)->Id == id);
	CHECK(project.Add(folder / L"charts\\..\\charts\\ONE.CHART")->Id == id);
	CHECK(project.FindByPath(one)->Id == id);
	CHECK(project.Items().size() == 1);

	auto second = project.Add(two, L"Clients / Smith");
	CHECK(second->Group == L"Clients/Smith");
	CHECK(project.Items().size() == 2);
	// the group and its parent exist now
	CHECK(project.Groups() == std::vector<std::wstring>{ L"Clients", L"Clients/Smith" });

	// a file that isn't there yet can be linked (a chart about to be saved), and is missing
	auto later = project.Add(folder / L"charts\\later.chart");
	CHECK(later->Missing);
	CHECK(project.Refresh() == 1);
	folder.Make(L"charts\\later.chart");
	CHECK(project.Refresh() == 0);

	CHECK(project.Find(id) != nullptr);
	CHECK(project.Find(L"nothing") == nullptr);
	CHECK(project.FindByPath(folder / L"charts\\unknown.chart") == nullptr);
}

TEST_CASE("Taking a file out of a project leaves the file", "[Project]") {
	TempFolder folder;
	auto path = folder.Make(L"a.chart");
	Project project;
	auto id = project.Add(path)->Id;
	project.Session({ { id }, id });
	REQUIRE(project.Session().Open.size() == 1);

	CHECK(project.Remove(id));
	CHECK_FALSE(project.Remove(id));
	CHECK(project.Items().empty());
	CHECK(fs::exists(path));
	// and it is not remembered as open
	CHECK(project.Session().Open.empty());
	CHECK(project.Session().Active.empty());
}

TEST_CASE("What was open is saved with the project but does not make it dirty", "[Project]") {
	TempFolder folder;
	auto path = folder / L"p.astroproj";
	std::wstring error;
	Project project;
	auto id = project.Add(folder.Make(L"a.chart"))->Id;
	REQUIRE(project.SaveAs(path, error));
	CHECK_FALSE(project.Dirty());
	CHECK_FALSE(project.SessionDirty());
	CHECK_FALSE(project.NeedsSave());

	project.Session({ { id }, id });
	CHECK_FALSE(project.Dirty());
	CHECK(project.SessionDirty());
	CHECK(project.NeedsSave());
	project.Session({ { id }, id });		// (the same again)
	REQUIRE(project.Save(error));
	CHECK_FALSE(project.NeedsSave());

	Project loaded;
	REQUIRE(loaded.Load(path, error));
	CHECK(loaded.Session().Open == std::vector<std::wstring>{ id });
	CHECK_FALSE(loaded.NeedsSave());
	// taking the open item out is a change to what the project holds, and to what it remembers
	CHECK(loaded.Remove(id));
	CHECK(loaded.Dirty());
	CHECK(loaded.SessionDirty());
}

TEST_CASE("Names, tags, notes and order", "[Project]") {
	TempFolder folder;
	Project project;
	auto a = project.Add(folder.Make(L"a.chart"))->Id;
	auto b = project.Add(folder.Make(L"b.chart"))->Id;
	auto c = project.Add(folder.Make(L"c.chart"))->Id;

	CHECK(project.Rename(a, L"  Anna  "));
	CHECK(project.Find(a)->Name == L"Anna");
	CHECK(project.Rename(a, L"   "));		// blank: the file's own name again
	CHECK(project.Find(a)->Name == L"a");
	CHECK_FALSE(project.Rename(L"nope", L"x"));

	CHECK(project.SetTags(b, { L"family", L" born, in May ", L"Family", L"", L"work" }));
	CHECK(project.Find(b)->Tags == std::vector<std::wstring>{ L"family", L"born  in May", L"work" });
	CHECK(project.SetNotes(b, L"first line\nsecond \\ line"));
	CHECK(project.Find(b)->Notes == L"first line\nsecond \\ line");

	// c to the front, then a to a group at the end
	CHECK(project.Move(c, L"", a));
	REQUIRE(project.Items().size() == 3);
	CHECK(project.Items()[0].Id == c);
	CHECK(project.Items()[1].Id == a);
	CHECK(project.Items()[2].Id == b);
	CHECK(project.Move(a, L"Kids"));
	CHECK(project.Items()[2].Id == a);
	CHECK(project.Find(a)->Group == L"Kids");
	CHECK(project.Groups() == std::vector<std::wstring>{ L"Kids" });
	CHECK_FALSE(project.Move(a, L"", L"nothing"));
	CHECK_FALSE(project.Move(L"nope", L""));
	CHECK(project.Move(b, L"", b));		// (before itself: nothing to do)
}

TEST_CASE("Groups", "[Project]") {
	TempFolder folder;
	Project project;
	auto a = project.Add(folder.Make(L"a.chart"), L"Clients/Smith/Kids")->Id;
	auto b = project.Add(folder.Make(L"b.chart"), L"Clients")->Id;
	auto c = project.Add(folder.Make(L"c.chart"))->Id;
	CHECK(project.Groups() == std::vector<std::wstring>{ L"Clients", L"Clients/Smith", L"Clients/Smith/Kids" });

	CHECK(project.AddGroup(L"Events"));
	CHECK_FALSE(project.AddGroup(L"events"));		// (it is there)
	CHECK_FALSE(project.AddGroup(L"  "));

	SECTION("renaming takes everything under it along") {
		CHECK(project.RenameGroup(L"Clients", L"Customers"));
		CHECK(project.Find(a)->Group == L"Customers/Smith/Kids");
		CHECK(project.Find(b)->Group == L"Customers");
		CHECK(project.Find(c)->Group.empty());
		CHECK(project.Groups() == std::vector<std::wstring>{ L"Customers", L"Customers/Smith", L"Customers/Smith/Kids", L"Events" });
		// to a name that is taken, or that isn't a group
		CHECK_FALSE(project.RenameGroup(L"Customers", L"Events"));
		CHECK_FALSE(project.RenameGroup(L"Nothing", L"Other"));
		CHECK_FALSE(project.RenameGroup(L"Events", L""));
		// into a group of its own that is new: parents are made
		CHECK(project.RenameGroup(L"Events", L"Archive/Old"));
		CHECK(project.Groups().back() == L"Archive/Old");
		CHECK(std::ranges::count(project.Groups(), L"Archive") == 1);
	}
	SECTION("removing lifts what is in it up one level") {
		CHECK(project.RemoveGroup(L"Clients/Smith"));
		CHECK(project.Find(a)->Group == L"Clients/Kids");
		CHECK(project.Find(b)->Group == L"Clients");
		CHECK(project.Groups() == std::vector<std::wstring>{ L"Clients", L"Clients/Kids", L"Events" });
		CHECK(project.RemoveGroup(L"Clients"));
		CHECK(project.Find(a)->Group == L"Kids");
		CHECK(project.Find(b)->Group.empty());
		CHECK_FALSE(project.RemoveGroup(L"Clients"));
		CHECK_FALSE(project.RemoveGroup(L""));
	}
}

TEST_CASE("A project file round trip", "[Project]") {
	TempFolder folder;
	auto chart = folder.Make(L"charts\\anna.chart", "[Chart]\nVersion=1\nId=abc-123\n");
	auto analysis = folder.Make(L"analyses\\anna.analysis", "[Analysis]\nVersion=1\nId=analysis-9\n\n#EVENTS\n0,1,2\n");
	auto aspects = folder.Make(L"tight.aspects");
	auto projectFile = folder / L"work\\Smith.astroproj";
	fs::create_directories(projectFile.parent_path());

	std::wstring error;
	std::wstring chartId, analysisId;
	{
		Project project;
		project.Name(L"The Smiths");
		project.Description(L"two lines\nof \"description\"");
		chartId = project.Add(chart, L"People")->Id;
		analysisId = project.Add(analysis, L"People")->Id;
		auto setId = project.Add(aspects)->Id;
		CHECK(project.Find(chartId)->FileId == L"abc-123");
		CHECK(project.Find(analysisId)->FileId == L"analysis-9");
		CHECK(project.Find(setId)->FileId.empty());
		project.Rename(chartId, L"Anna, born 1980");
		project.SetTags(chartId, { L"family", L"client" });
		project.SetNotes(chartId, L"line 1\nline 2 with ; and = and \\");
		project.AddGroup(L"Empty group");
		project.Session({ { analysisId, chartId }, chartId });
		CHECK(project.Dirty());
		CHECK_FALSE(project.Save(error));		// (no file yet)
		INFO(Narrow(error));
		REQUIRE(project.SaveAs(projectFile, error));
		CHECK_FALSE(project.Dirty());
		CHECK(project.FilePath() == projectFile.lexically_normal());
	}

	// the paths in the file are relative to it
	auto text = Read(projectFile);
	CHECK(text.find("Path=..\\charts\\anna.chart") != std::string::npos);
	CHECK(text.find(folder.Path.string()) == std::string::npos);

	Project loaded;
	REQUIRE(loaded.Load(projectFile, error));
	INFO(Narrow(error));
	CHECK_FALSE(loaded.Dirty());
	CHECK(loaded.Name() == L"The Smiths");
	CHECK(loaded.Description() == L"two lines\nof \"description\"");
	REQUIRE(loaded.Items().size() == 3);
	auto const& first = loaded.Items()[0];
	CHECK(first.Id == chartId);
	CHECK(first.Name == L"Anna, born 1980");
	CHECK(first.Type == ProjectItemType::Chart);
	CHECK(first.Group == L"People");
	CHECK(first.FileId == L"abc-123");
	CHECK(first.Tags == std::vector<std::wstring>{ L"family", L"client" });
	CHECK(first.Notes == L"line 1\nline 2 with ; and = and \\");
	CHECK(loaded.Resolve(first) == chart.lexically_normal());
	CHECK_FALSE(first.Missing);
	CHECK(loaded.Items()[1].Type == ProjectItemType::Analysis);
	CHECK(loaded.Items()[2].Type == ProjectItemType::AspectSet);
	CHECK(loaded.Items()[2].Group.empty());
	CHECK(loaded.Groups() == std::vector<std::wstring>{ L"People", L"Empty group" });
	CHECK(loaded.Session().Open == std::vector<std::wstring>{ analysisId, chartId });
	CHECK(loaded.Session().Active == chartId);

	// nothing changes if it is saved again, and the second text is the same as the first
	REQUIRE(loaded.Save(error));
	CHECK(Read(projectFile) == text);
}

TEST_CASE("The project and its files move together", "[Project]") {
	TempFolder folder;
	auto chart = folder.Make(L"all\\charts\\a.chart");
	auto projectFile = folder / L"all\\p.astroproj";
	std::wstring error;
	Project project;
	auto id = project.Add(chart)->Id;
	REQUIRE(project.SaveAs(projectFile, error));
	CHECK(project.Find(id)->Path == L"charts\\a.chart");

	// the whole folder renamed: the relative path still finds the file
	auto moved = folder / L"moved";
	fs::rename(folder / L"all", moved);
	Project loaded;
	REQUIRE(loaded.Load(moved / L"p.astroproj", error));
	CHECK(loaded.Items()[0].Missing == false);
	CHECK(loaded.Resolve(loaded.Items()[0]) == (moved / L"charts\\a.chart").lexically_normal());

	// Save As elsewhere: the link now goes up and over, and still leads to the same file
	auto elsewhere = folder / L"elsewhere\\deep\\q.astroproj";
	fs::create_directories(elsewhere.parent_path());
	REQUIRE(loaded.SaveAs(elsewhere, error));
	CHECK(loaded.Items()[0].Path == L"..\\..\\moved\\charts\\a.chart");
	CHECK(loaded.Resolve(loaded.Items()[0]) == (moved / L"charts\\a.chart").lexically_normal());
	Project again;
	REQUIRE(again.Load(elsewhere, error));
	CHECK_FALSE(again.Items()[0].Missing);

	// a project written by hand: a full path, no Name, no Id
	std::ofstream(elsewhere, std::ios::binary) << "[Project]\nVersion=1\n[Item.1]\nPath=" << chart.string() << "\n";
	Project absolute;
	// (that chart no longer exists at its old place)
	REQUIRE(absolute.Load(elsewhere, error));
	CHECK(absolute.Items()[0].Missing);
	CHECK(absolute.Items()[0].Name == L"a");		// no Name: the file's
	CHECK(absolute.Items()[0].Type == ProjectItemType::Chart);
	CHECK(absolute.Items()[0].Id.size() == 36);		// no Id: made
}

TEST_CASE("Finding a file that has gone", "[Project]") {
	TempFolder folder;
	auto chart = folder.Make(L"old\\anna.chart", "[Chart]\nVersion=1\nId=anna-1\n");
	auto other = folder.Make(L"old\\bob.chart", "[Chart]\nVersion=1\nId=bob-1\n");
	auto plain = folder.Make(L"old\\plain.chart", "[Chart]\nVersion=1\n");
	Project project;
	auto anna = project.Add(chart)->Id;
	auto bob = project.Add(other)->Id;
	auto notes = project.Add(plain)->Id;
	CHECK(project.Refresh() == 0);

	fs::create_directories(folder / L"new");
	fs::rename(chart, folder / L"new\\anna.chart");
	fs::rename(other, folder / L"new\\robert.chart");		// renamed as well
	fs::rename(plain, folder / L"new\\plain.chart");
	CHECK(project.Refresh() == 3);
	CHECK(project.Find(anna)->Missing);

	// not there: folders that don't have it
	CHECK_FALSE(project.Locate(anna, { folder / L"nowhere", folder / L"old" }));
	CHECK(project.Find(anna)->Missing);
	// by name
	CHECK(project.Locate(anna, { folder / L"old", folder / L"new" }));
	CHECK_FALSE(project.Find(anna)->Missing);
	CHECK(project.Resolve(*project.Find(anna)) == (folder / L"new\\anna.chart").lexically_normal());
	// by the id in the file, under another name
	CHECK(project.Locate(bob, { folder / L"new" }));
	CHECK(project.Resolve(*project.Find(bob)) == (folder / L"new\\robert.chart").lexically_normal());
	CHECK(project.Find(bob)->Name == L"bob");		// (the name shown stays)
	// a file with no id is found by name only
	CHECK(project.Locate(notes, { folder / L"new" }));
	CHECK(project.Refresh() == 0);
	CHECK_FALSE(project.Locate(L"nope", { folder / L"new" }));

	// a file of that name that is another chart (another id) is not taken for it
	auto impostor = folder.Make(L"third\\anna.chart", "[Chart]\nVersion=1\nId=someone-else\n");
	fs::remove(folder / L"new\\anna.chart");
	CHECK(project.Refresh() == 1);
	CHECK_FALSE(project.Locate(anna, { folder / L"third" }));
	CHECK(project.Find(anna)->Missing);

	// or pointed at a file by hand
	CHECK(project.Relink(anna, impostor));
	CHECK_FALSE(project.Find(anna)->Missing);
	CHECK(project.Find(anna)->FileId == L"someone-else");
	CHECK_FALSE(project.Relink(L"nope", impostor));
}

TEST_CASE("File ids: charts, derived charts and analyses carry them", "[Project]") {
	TempFolder folder;
	CHECK(Project::ReadFileId(folder.Make(L"a.chart", "[Chart]\nVersion=1\nId=  abc \n")) == L"abc");
	CHECK(Project::ReadFileId(folder.Make(L"b.chart", "[Derived]\nVersion=1\nId=derived-1\n")) == L"derived-1");
	CHECK(Project::ReadFileId(folder.Make(L"c.analysis", "[Analysis]\nVersion=1\nId=an-1\n\n#EVENTS\n0,1,2,3\n")) == L"an-1");
	CHECK(Project::ReadFileId(folder.Make(L"d.chart", "[Chart]\nVersion=1\n")).empty());
	CHECK(Project::ReadFileId(folder.Make(L"e.chart", "this is not INI\n")).empty());
	CHECK(Project::ReadFileId(folder / L"missing.chart").empty());
	CHECK(Project::ReadFileId(folder.Make(L"f.aspects", "[Chart]\nId=x\n")).empty());		// (not a kind of file that has one)
}

TEST_CASE("A project file that is wrong is refused, and what is not understood is kept", "[Project]") {
	TempFolder folder;
	auto path = folder / L"p.astroproj";
	std::wstring error;
	auto write = [&](std::string const& text) {
		std::ofstream(path, std::ios::binary) << text;
	};

	Project project;
	project.Name(L"Before");
	auto before = project.Add(folder.Make(L"x.chart"))->Id;
	auto refuse = [&](std::string const& text, std::wstring const& mentions) {
		write(text);
		CHECK_FALSE(project.Load(path, error));
		CHECK(error.find(mentions) != std::wstring::npos);
		// and the project is as it was
		CHECK(project.Name() == L"Before");
		CHECK(project.Find(before) != nullptr);
	};
	refuse("[Chart]\nVersion=1\n", L"not a project");
	refuse("[Project]\nName=x\n", L"not a project");
	refuse("[Project]\nVersion=99\n", L"newer");
	refuse("[Project]\nVersion=1\n[Item.1]\nName=no path\n", L"[Item.1] Path");
	refuse("[Project]\nVersion=1\n[Item.1]\nId=same\nPath=a.chart\n[Item.2]\nId=same\nPath=b.chart\n", L"same id");
	CHECK_FALSE(project.Load(folder / L"absent.astroproj", error));
	CHECK_FALSE(error.empty());

	// an item that is in a group nobody listed makes the group; a session entry for an item that isn't there is dropped
	write("[Project]\nVersion=1\nName=Hand made\n[Item.1]\nId=one\nPath=a.chart\nGroup=Somewhere/Deep\n[Session]\nOpen=one, ghost\nActive=ghost\n"
		"[Notes For Me]\nkeep=this\n[Item.2]\nId=two\nPath=b.chart\nType=whatever\n");
	REQUIRE(project.Load(path, error));
	CHECK(project.Name() == L"Hand made");
	CHECK(project.Groups() == std::vector<std::wstring>{ L"Somewhere", L"Somewhere/Deep" });
	CHECK(project.Session().Open == std::vector<std::wstring>{ L"one" });
	CHECK(project.Session().Active.empty());
	CHECK(project.Items()[1].Type == ProjectItemType::Other);		// (a Type it doesn't know)
	REQUIRE(project.Save(error));
	auto saved = Read(path);
	CHECK(saved.find("[Notes For Me]") != std::string::npos);
	CHECK(saved.find("keep=this") != std::string::npos);
}
