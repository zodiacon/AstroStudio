// (no precompiled header: this file is also built into the unit tests, which have none)
#define NOMINMAX
#include <Windows.h>
#include "Project.h"
#include <IniDocument.h>
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <random>

namespace fs = std::filesystem;

namespace {
	using Names = std::vector<std::wstring>;

	bool SameText(std::wstring_view a, std::wstring_view b) {
		if (a.size() != b.size())
			return false;
		return a.empty() || ::CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
	}

	std::wstring Trim(std::wstring_view text) {
		while (!text.empty() && iswspace(text.front()))
			text.remove_prefix(1);
		while (!text.empty() && iswspace(text.back()))
			text.remove_suffix(1);
		return std::wstring(text);
	}

	// a value on one line: a backslash and a line break are written as \\ and \n
	std::wstring Escape(std::wstring_view text) {
		std::wstring result;
		for (wchar_t ch : text) {
			if (ch == L'\\')
				result += L"\\\\";
			else if (ch == L'\n')
				result += L"\\n";
			else if (ch != L'\r')
				result += ch;
		}
		return result;
	}

	std::wstring Unescape(std::wstring_view text) {
		std::wstring result;
		for (size_t i = 0; i < text.size(); i++) {
			if (text[i] == L'\\' && i + 1 < text.size()) {
				result += text[i + 1] == L'n' ? L'\n' : text[i + 1];
				i++;
			}
			else
				result += text[i];
		}
		return result;
	}

	std::wstring TypeName(ProjectItemType type) {
		switch (type) {
			case ProjectItemType::Chart: return L"Chart";
			case ProjectItemType::Analysis: return L"Analysis";
			case ProjectItemType::AspectSet: return L"AspectSet";
			case ProjectItemType::ColorSet: return L"ColorSet";
			default: return L"Other";
		}
	}

	ProjectItemType TypeFromName(std::wstring_view name) {
		for (auto type : { ProjectItemType::Chart, ProjectItemType::Analysis, ProjectItemType::AspectSet, ProjectItemType::ColorSet })
			if (SameText(TypeName(type), name))
				return type;
		return ProjectItemType::Other;
	}

	bool InGroup(std::wstring_view group, std::wstring_view parent) {
		// is `group` the same as `parent`, or below it
		if (group.size() < parent.size() || !SameText(group.substr(0, parent.size()), parent))
			return false;
		return group.size() == parent.size() || group[parent.size()] == L'/';
	}

	std::vector<std::wstring> Split(std::wstring_view text, wchar_t separator) {
		std::vector<std::wstring> parts;
		size_t start = 0;
		while (start <= text.size()) {
			auto end = text.find(separator, start);
			if (end == std::wstring_view::npos)
				end = text.size();
			parts.push_back(Trim(text.substr(start, end - start)));
			start = end + 1;
		}
		return parts;
	}

	std::wstring SanitizeTag(std::wstring_view tag) {
		std::wstring result;
		for (wchar_t ch : tag)
			result += (ch == L',' || ch == L'\n' || ch == L'\r') ? L' ' : ch;
		return Trim(result);
	}

	const wchar_t* const ItemPrefix = L"Item.";
}

//
// statics
//

ProjectItemType Project::TypeOfFile(fs::path const& path) {
	auto extension = path.extension().wstring();
	if (SameText(extension, L".chart"))
		return ProjectItemType::Chart;
	if (SameText(extension, L".analysis"))
		return ProjectItemType::Analysis;
	if (SameText(extension, L".aspects"))
		return ProjectItemType::AspectSet;
	if (SameText(extension, L".colors"))
		return ProjectItemType::ColorSet;
	return ProjectItemType::Other;
}

std::wstring Project::ReadFileId(fs::path const& path) {
	auto type = TypeOfFile(path);
	if (type != ProjectItemType::Chart && type != ProjectItemType::Analysis)
		return {};
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return {};
	std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	// an analysis file has its events after a line #EVENTS, which are not INI
	if (auto events = text.find("\n#EVENTS\n"); events != std::string::npos)
		text.resize(events + 1);
	IniDocument ini;
	if (!ini.Parse(text))
		return {};
	for (auto section : { L"Chart", L"Derived", L"Analysis" })
		if (auto id = ini.Get(section, L"Id"))
			return Trim(*id);
	return {};
}

std::wstring Project::NewId() {
	static std::mt19937_64 random{ std::random_device{}() };
	auto a = random(), b = random();
	// (version 4, variant 1: the shape of a GUID made of random numbers)
	a = (a & 0xFFFFFFFFFFFF0FFFull) | 0x0000000000004000ull;
	b = (b & 0x3FFFFFFFFFFFFFFFull) | 0x8000000000000000ull;
	wchar_t text[40];
	swprintf_s(text, L"%08x-%04x-%04x-%04x-%012llx", static_cast<unsigned>(a >> 32), static_cast<unsigned>((a >> 16) & 0xFFFF), static_cast<unsigned>(a & 0xFFFF),
		static_cast<unsigned>(b >> 48), b & 0xFFFFFFFFFFFFull);
	return text;
}

std::wstring Project::NormalizeGroup(std::wstring_view group) {
	std::wstring result;
	std::wstring text(group);
	std::ranges::replace(text, L'\\', L'/');
	for (auto const& part : Split(text, L'/')) {
		if (part.empty())
			continue;
		result += (result.empty() ? L"" : L"/") + part;
	}
	return result;
}

//
// the project
//

void Project::Name(std::wstring name) {
	if (m_Name != name) {
		m_Name = std::move(name);
		m_Dirty = true;
	}
}

void Project::Description(std::wstring description) {
	if (m_Description != description) {
		m_Description = std::move(description);
		m_Dirty = true;
	}
}

ProjectItem* Project::FindItem(std::wstring_view id) {
	auto it = std::ranges::find_if(m_Items, [&](auto const& item) { return item.Id == id; });
	return it == m_Items.end() ? nullptr : &*it;
}

ProjectItem const* Project::Find(std::wstring_view id) const {
	return const_cast<Project*>(this)->FindItem(id);
}

fs::path Project::Resolve(ProjectItem const& item) const {
	fs::path path(item.Path);
	if (path.is_relative()) {
		auto base = m_FilePath.empty() ? fs::current_path() : m_FilePath.parent_path();
		path = base / path;
	}
	return path.lexically_normal();
}

ProjectItem const* Project::FindByPath(fs::path const& path) const {
	std::error_code error;
	auto wanted = fs::absolute(path, error).lexically_normal().wstring();
	for (auto const& item : m_Items)
		if (SameText(Resolve(item).wstring(), wanted))
			return &item;
	return nullptr;
}

std::wstring Project::StoredPath(fs::path const& absolute) const {
	if (!m_FilePath.empty()) {
		// relative to the project file's folder - unless they are not on the same drive, when there is no way from one to the other
		auto relative = absolute.lexically_relative(m_FilePath.parent_path());
		if (!relative.empty())
			return relative.wstring();
	}
	return absolute.wstring();
}

void Project::EnsureGroup(std::wstring_view group) {
	// the group and each of its parents
	auto normal = NormalizeGroup(group);
	std::wstring path;
	for (auto const& part : Split(normal, L'/')) {
		if (part.empty())
			continue;
		path += (path.empty() ? L"" : L"/") + part;
		if (std::ranges::none_of(m_Groups, [&](auto const& g) { return SameText(g, path); })) {
			m_Groups.push_back(path);
			m_Dirty = true;
		}
	}
}

ProjectItem const* Project::Add(fs::path const& path, std::wstring_view group) {
	std::error_code error;
	auto absolute = fs::absolute(path, error).lexically_normal();
	if (auto existing = FindByPath(absolute))
		return existing;

	ProjectItem item;
	item.Id = NewId();
	item.Type = TypeOfFile(absolute);
	item.Name = absolute.stem().wstring();
	item.Path = StoredPath(absolute);
	item.Group = NormalizeGroup(group);
	if (fs::exists(absolute, error))
		item.FileId = ReadFileId(absolute);
	else
		item.Missing = true;
	EnsureGroup(item.Group);
	m_Items.push_back(std::move(item));
	m_Dirty = true;
	return &m_Items.back();
}

bool Project::Remove(std::wstring_view id) {
	auto it = std::ranges::find_if(m_Items, [&](auto const& item) { return item.Id == id; });
	if (it == m_Items.end())
		return false;
	m_Items.erase(it);
	std::erase(m_Session.Open, std::wstring(id));
	if (m_Session.Active == id)
		m_Session.Active.clear();
	m_Dirty = true;
	return true;
}

bool Project::Rename(std::wstring_view id, std::wstring name) {
	auto item = FindItem(id);
	if (!item)
		return false;
	name = Trim(name);
	if (name.empty())
		name = Resolve(*item).stem().wstring();
	if (item->Name != name) {
		item->Name = std::move(name);
		m_Dirty = true;
	}
	return true;
}

bool Project::SetTags(std::wstring_view id, std::vector<std::wstring> tags) {
	auto item = FindItem(id);
	if (!item)
		return false;
	std::vector<std::wstring> clean;
	for (auto const& tag : tags) {
		auto text = SanitizeTag(tag);
		if (!text.empty() && std::ranges::none_of(clean, [&](auto const& t) { return SameText(t, text); }))
			clean.push_back(text);
	}
	if (item->Tags != clean) {
		item->Tags = std::move(clean);
		m_Dirty = true;
	}
	return true;
}

bool Project::SetNotes(std::wstring_view id, std::wstring notes) {
	auto item = FindItem(id);
	if (!item)
		return false;
	if (item->Notes != notes) {
		item->Notes = std::move(notes);
		m_Dirty = true;
	}
	return true;
}

bool Project::Move(std::wstring_view id, std::wstring_view group, std::wstring_view beforeId) {
	auto from = std::ranges::find_if(m_Items, [&](auto const& item) { return item.Id == id; });
	if (from == m_Items.end())
		return false;
	if (id == beforeId)
		return true;		// (before itself: where it is)
	if (!beforeId.empty() && !Find(beforeId))
		return false;
	auto item = std::move(*from);
	m_Items.erase(from);
	item.Group = NormalizeGroup(group);
	EnsureGroup(item.Group);
	auto to = m_Items.end();
	if (!beforeId.empty())
		to = std::ranges::find_if(m_Items, [&](auto const& other) { return other.Id == beforeId; });
	m_Items.insert(to, std::move(item));
	m_Dirty = true;
	return true;
}

//
// groups
//

bool Project::AddGroup(std::wstring_view group) {
	auto normal = NormalizeGroup(group);
	if (normal.empty())
		return false;
	auto before = m_Groups.size();
	EnsureGroup(normal);
	return m_Groups.size() != before;
}

bool Project::RenameGroup(std::wstring_view from, std::wstring_view to) {
	auto oldName = NormalizeGroup(from), newName = NormalizeGroup(to);
	if (oldName.empty() || newName.empty())
		return false;
	if (std::ranges::none_of(m_Groups, [&](auto const& g) { return SameText(g, oldName); }))
		return false;
	if (SameText(oldName, newName))
		return true;
	// the new name must be free: neither it nor anything below it exists (unless it is inside the group being renamed)
	if (std::ranges::any_of(m_Groups, [&](auto const& g) { return InGroup(g, newName) && !InGroup(g, oldName); }))
		return false;

	auto rename = [&](std::wstring& group) {
		if (InGroup(group, oldName))
			group = newName + group.substr(oldName.size());
	};
	for (auto& group : m_Groups)
		rename(group);
	for (auto& item : m_Items)
		rename(item.Group);
	// its parents, if they are new, go in front of it
	auto at = std::ranges::find_if(m_Groups, [&](auto const& g) { return SameText(g, newName); }) - m_Groups.begin();
	std::wstring parent;
	auto parts = Split(newName, L'/');
	for (size_t i = 0; i + 1 < parts.size(); i++) {
		parent += (parent.empty() ? L"" : L"/") + parts[i];
		if (std::ranges::none_of(m_Groups, [&](auto const& g) { return SameText(g, parent); }))
			m_Groups.insert(m_Groups.begin() + at++, parent);
	}
	m_Dirty = true;
	return true;
}

bool Project::RemoveGroup(std::wstring_view group) {
	auto name = NormalizeGroup(group);
	auto it = std::ranges::find_if(m_Groups, [&](auto const& g) { return SameText(g, name); });
	if (name.empty() || it == m_Groups.end())
		return false;
	auto slash = name.rfind(L'/');
	auto parent = slash == std::wstring::npos ? std::wstring() : name.substr(0, slash);
	// what is below it goes up one level: "A/B/C" in "A/B" becomes "A/C"
	auto lift = [&](std::wstring& g) {
		if (SameText(g, name))
			g = parent;
		else if (InGroup(g, name))
			g = (parent.empty() ? L"" : parent + L"/") + g.substr(name.size() + 1);
	};
	for (auto& item : m_Items)
		lift(item.Group);
	std::vector<std::wstring> groups;
	for (auto g : m_Groups) {
		if (SameText(g, name))
			continue;
		lift(g);
		groups.push_back(g);
	}
	m_Groups = std::move(groups);
	m_Dirty = true;
	return true;
}

//
// files that have gone
//

int Project::Refresh() {
	int missing = 0;
	for (auto& item : m_Items) {
		std::error_code error;
		item.Missing = !fs::exists(Resolve(item), error);
		missing += item.Missing ? 1 : 0;
	}
	return missing;
}

bool Project::Relink(std::wstring_view id, fs::path const& path) {
	auto item = FindItem(id);
	if (!item)
		return false;
	std::error_code error;
	auto absolute = fs::absolute(path, error).lexically_normal();
	item->Path = StoredPath(absolute);
	item->Type = TypeOfFile(absolute);
	item->Missing = !fs::exists(absolute, error);
	if (!item->Missing)
		if (auto fileId = ReadFileId(absolute); !fileId.empty())
			item->FileId = fileId;
	m_Dirty = true;
	return true;
}

bool Project::Locate(std::wstring_view id, std::vector<fs::path> const& folders) {
	auto item = FindItem(id);
	if (!item)
		return false;
	auto name = Resolve(*item).filename();
	std::error_code error;

	// the same name, unless the file there is another one (it has a different id in it)
	for (auto const& folder : folders) {
		auto candidate = folder / name;
		if (!fs::is_regular_file(candidate, error))
			continue;
		auto found = ReadFileId(candidate);
		if (item->FileId.empty() || found.empty() || found == item->FileId)
			return Relink(id, candidate);
	}
	// a file that is the same one under another name
	if (!item->FileId.empty()) {
		for (auto const& folder : folders) {
			for (fs::directory_iterator it(folder, error), end; !error && it != end; it.increment(error)) {
				if (!it->is_regular_file(error) || TypeOfFile(it->path()) != item->Type)
					continue;
				if (ReadFileId(it->path()) == item->FileId)
					return Relink(id, it->path());
			}
		}
	}
	return false;
}

void Project::Session(ProjectSession session) {
	// only items that are there
	std::erase_if(session.Open, [&](auto const& id) { return !Find(id); });
	if (!Find(session.Active))
		session.Active.clear();
	if (session.Open != m_Session.Open || session.Active != m_Session.Active) {
		m_Session = std::move(session);
		m_Dirty = true;
	}
}

//
// the file
//

bool Project::Load(fs::path const& path, std::wstring& error) {
	IniDocument ini;
	if (!ini.Load(path.c_str())) {
		error = ini.Error();
		return false;
	}
	auto version = ini.GetInt(L"Project", L"Version");
	if (!version) {
		error = L"this is not a project file (there is no [Project] Version)";
		return false;
	}
	if (*version > CurrentVersion) {
		error = L"this project was saved by a newer version of the program (file version " + std::to_wstring(*version) + L")";
		return false;
	}

	Project loaded;
	loaded.m_FilePath = fs::absolute(path).lexically_normal();
	loaded.m_Name = ini.GetString(L"Project", L"Name", loaded.m_FilePath.stem().wstring());
	loaded.m_Description = Unescape(ini.GetString(L"Project", L"Description"));

	for (auto const& key : ini.Keys(L"Groups"))
		loaded.EnsureGroup(ini.GetString(L"Groups", key));

	std::wstring prefix = ItemPrefix;
	for (auto const& section : ini.Sections()) {
		if (section.size() <= prefix.size() || !SameText(std::wstring_view(section).substr(0, prefix.size()), prefix))
			continue;
		ProjectItem item;
		item.Path = ini.GetString(section, L"Path");
		if (item.Path.empty()) {
			error = L"[" + section + L"] Path is missing";
			return false;
		}
		item.Id = ini.GetString(section, L"Id");
		if (item.Id.empty())
			item.Id = NewId();		// (an item added by hand)
		else if (std::ranges::any_of(loaded.m_Items, [&](auto const& other) { return other.Id == item.Id; })) {
			error = L"[" + section + L"] Id (line " + std::to_wstring(ini.LineOf(section, L"Id")) + L"): another item has the same id";
			return false;
		}
		item.Type = ini.Has(section, L"Type") ? TypeFromName(ini.GetString(section, L"Type")) : TypeOfFile(item.Path);
		item.Name = ini.GetString(section, L"Name");
		if (item.Name.empty())
			item.Name = fs::path(item.Path).stem().wstring();
		item.FileId = ini.GetString(section, L"FileId");
		item.Group = NormalizeGroup(ini.GetString(section, L"Group"));
		item.Notes = Unescape(ini.GetString(section, L"Notes"));
		for (auto const& tag : Split(ini.GetString(section, L"Tags"), L','))
			if (!tag.empty())
				item.Tags.push_back(tag);
		loaded.EnsureGroup(item.Group);
		loaded.m_Items.push_back(std::move(item));
	}

	for (auto const& id : Split(ini.GetString(L"Session", L"Open"), L','))
		if (loaded.Find(id))
			loaded.m_Session.Open.push_back(id);
	if (auto active = ini.GetString(L"Session", L"Active"); loaded.Find(active))
		loaded.m_Session.Active = active;

	// what isn't ours goes back when it is saved
	for (auto const& section : ini.Sections()) {
		if (SameText(section, L"Project") || SameText(section, L"Groups") || SameText(section, L"Session") ||
			(section.size() > prefix.size() && SameText(std::wstring_view(section).substr(0, prefix.size()), prefix)))
			continue;
		for (auto const& key : ini.Keys(section))
			loaded.m_Extra.SetString(section, key, ini.GetString(section, key));
	}

	loaded.Refresh();
	loaded.m_Dirty = false;
	*this = std::move(loaded);
	return true;
}

bool Project::Write(fs::path const& path, std::wstring& error) {
	IniDocument ini;
	ini.SetHeader(L"Astro Studio project\n"
		L"The files (charts, analyses, aspect and colour sets) are linked, not stored here: Path is relative to this file's folder when they\n"
		L"are on the same drive. Id names an item in this project, FileId is the id kept in the file itself, by which a moved file is found.\n"
		L"Groups exist only in the project; Tags are separated by commas.");
	ini.SetInt(L"Project", L"Version", CurrentVersion);
	ini.SetString(L"Project", L"Name", m_Name);
	if (!m_Description.empty())
		ini.SetString(L"Project", L"Description", Escape(m_Description));
	for (size_t i = 0; i < m_Groups.size(); i++)
		ini.SetString(L"Groups", L"G" + std::to_wstring(i + 1), m_Groups[i]);

	for (size_t i = 0; i < m_Items.size(); i++) {
		auto const& item = m_Items[i];
		auto section = std::wstring(ItemPrefix) + std::to_wstring(i + 1);
		ini.SetString(section, L"Id", item.Id);
		ini.SetString(section, L"Type", TypeName(item.Type));
		ini.SetString(section, L"Name", item.Name);
		ini.SetString(section, L"Path", item.Path);
		if (!item.FileId.empty())
			ini.SetString(section, L"FileId", item.FileId);
		if (!item.Group.empty())
			ini.SetString(section, L"Group", item.Group);
		if (!item.Tags.empty()) {
			std::wstring tags;
			for (auto const& tag : item.Tags)
				tags += (tags.empty() ? L"" : L", ") + tag;
			ini.SetString(section, L"Tags", tags);
		}
		if (!item.Notes.empty())
			ini.SetString(section, L"Notes", Escape(item.Notes));
	}

	if (!m_Session.Open.empty() || !m_Session.Active.empty()) {
		std::wstring open;
		for (auto const& id : m_Session.Open)
			open += (open.empty() ? L"" : L", ") + id;
		ini.SetString(L"Session", L"Open", open);
		ini.SetString(L"Session", L"Active", m_Session.Active);
	}
	for (auto const& section : m_Extra.Sections())
		for (auto const& key : m_Extra.Keys(section))
			ini.SetString(section, key, m_Extra.GetString(section, key));

	if (!ini.Save(path.c_str())) {
		error = ini.Error();
		return false;
	}
	return true;
}

bool Project::Save(std::wstring& error) {
	if (m_FilePath.empty()) {
		error = L"the project has no file yet";
		return false;
	}
	if (!Write(m_FilePath, error))
		return false;
	m_Dirty = false;
	return true;
}

bool Project::SaveAs(fs::path const& path, std::wstring& error) {
	std::error_code code;
	auto target = fs::absolute(path, code).lexically_normal();
	// the paths are kept as full paths while the base changes, then made relative to the new one
	std::vector<fs::path> full;
	for (auto const& item : m_Items)
		full.push_back(Resolve(item));
	auto oldFile = m_FilePath;
	m_FilePath = target;
	for (size_t i = 0; i < m_Items.size(); i++)
		m_Items[i].Path = StoredPath(full[i]);
	if (!Write(target, error)) {
		// nothing changed
		m_FilePath = oldFile;
		for (size_t i = 0; i < m_Items.size(); i++)
			m_Items[i].Path = StoredPath(full[i]);
		return false;
	}
	m_Dirty = false;
	return true;
}
