#pragma once

#include <IniDocument.h>
#include <filesystem>
#include <string>
#include <vector>

// A project: a set of links to files - charts, analyses, aspect and colour sets - kept together in one small file (.astroproj) that
// also remembers how the user arranged them (names, groups, tags, notes, the order) and what was open. It holds no data of its own and
// has no folder: the files stay where they are, a file can be in several projects or in none, and taking one out of a project (or
// deleting the project) never touches the file. One project is open at a time.
//
// The project file is UTF-8 INI text (see IniDocument), written to be read and edited by hand:
//
//   [Project]     Version, Name, Description
//   [Groups]      G1, G2 ... : the groups (folders that exist only in the project), by path: "Clients/Smith"
//   [Item.1] ...  one section for each linked file, in the order shown: Id, Type, Name, Path, FileId, Group, Tags, Notes
//   [Session]     Open (the Ids of the items that were open, in tab order), Active
//
// Path is relative to the folder of the project file when the file is on the same drive (so that a project and its charts can be
// moved together), otherwise absolute. Id names the item inside the project (a GUID); FileId is the id kept inside the linked file
// itself (blank for a file that has none), by which a file that was moved or renamed can be found again. Sections and keys this
// program doesn't know are kept when the project is saved again.
//
// This class knows nothing of the user interface and needs no more than the standard library and Windows.

enum class ProjectItemType {
	Chart,			// .chart: a chart or a chart worked out from others
	Analysis,		// .analysis
	AspectSet,		// .aspects
	ColorSet,		// .colors
	Other,
};

struct ProjectItem {
	std::wstring Id;
	ProjectItemType Type{ ProjectItemType::Other };
	std::wstring Name;			// as shown; the name of the file to begin with
	std::wstring Path;			// as stored (see above); Project::Resolve gives the full path
	std::wstring FileId;
	std::vector<std::wstring> Tags;
	std::wstring Notes;
	std::wstring Group;			// "" for the top level; groups nest as "Clients/Smith"
	bool Missing{ false };		// (not saved) the file wasn't there when Refresh last looked
};

struct ProjectSession {
	std::vector<std::wstring> Open;		// item Ids
	std::wstring Active;
};

class Project {
public:
	static constexpr int CurrentVersion = 1;
	static constexpr const wchar_t* Extension = L"astroproj";
	// for the Open and Save dialogs
	static constexpr wchar_t Filter[] = L"Astro Studio projects (*.astroproj)\0*.astroproj\0All files (*.*)\0*.*\0";

	// what kind of item a file is, by its extension
	static ProjectItemType TypeOfFile(std::filesystem::path const& path);
	// the id a chart, derived chart or analysis file carries in itself ([Chart], [Derived] or [Analysis] Id); blank if it has none
	// or can't be read
	static std::wstring ReadFileId(std::filesystem::path const& path);
	// a new GUID as text, like 3f2504e0-4f89-41d3-9a0c-0305e82c3301
	static std::wstring NewId();
	// a group path in its usual form: parts separated by '/', no blanks, no empty parts ("Clients\ Smith/" -> "Clients/Smith")
	static std::wstring NormalizeGroup(std::wstring_view group);

	// ---- the project itself
	std::wstring const& Name() const noexcept {
		return m_Name;
	}
	void Name(std::wstring name);
	std::wstring const& Description() const noexcept {
		return m_Description;
	}
	void Description(std::wstring description);
	// where the project file is, empty until it has been saved or loaded
	std::filesystem::path const& FilePath() const noexcept {
		return m_FilePath;
	}
	// changed since it was loaded or saved - in what it holds: the items, their names, groups and so on
	bool Dirty() const noexcept {
		return m_Dirty;
	}
	// The session (what is open) changed since it was loaded or saved. That is not something to ask the user about, only to save
	// when the project is closed.
	bool SessionDirty() const noexcept {
		return m_SessionDirty;
	}
	bool NeedsSave() const noexcept {
		return m_Dirty || m_SessionDirty;
	}

	// ---- the files
	std::vector<ProjectItem> const& Items() const noexcept {
		return m_Items;
	}
	ProjectItem const* Find(std::wstring_view id) const;
	// the item that links this file (the same file however it is spelled: capitals, "..", relative or absolute), or null
	ProjectItem const* FindByPath(std::filesystem::path const& path) const;
	// the full path of an item's file
	std::filesystem::path Resolve(ProjectItem const& item) const;
	// Links a file, in a group, at the end. A file that is already linked is not linked again: that item is returned. The file need
	// not exist yet (a chart about to be saved), but if it does, its FileId is read.
	ProjectItem const* Add(std::filesystem::path const& path, std::wstring_view group = {});
	// takes the link out; the file is not touched. False if there is no such item.
	bool Remove(std::wstring_view id);
	bool Rename(std::wstring_view id, std::wstring name);
	// tags can't contain commas or line breaks (they become spaces) and are kept once each, in the order given
	bool SetTags(std::wstring_view id, std::vector<std::wstring> tags);
	bool SetNotes(std::wstring_view id, std::wstring notes);
	// Puts the item in a group (which is created if it isn't there) and in the order before another item - at the end if that is empty.
	bool Move(std::wstring_view id, std::wstring_view group, std::wstring_view beforeId = {});

	// ---- groups: they exist in the project only. Every group's parents are groups too.
	std::vector<std::wstring> const& Groups() const noexcept {
		return m_Groups;
	}
	bool AddGroup(std::wstring_view group);
	// Renames a group and everything under it ("Clients" -> "Customers" also makes "Clients/Smith" "Customers/Smith"). False if
	// there is no such group or the new name is taken by another.
	bool RenameGroup(std::wstring_view from, std::wstring_view to);
	// Takes a group away: its items, and its groups, move up to its parent.
	bool RemoveGroup(std::wstring_view group);

	// ---- files that have gone
	// Looks at every item's file and sets Missing; returns how many are missing.
	int Refresh();
	// points the item at another file (its FileId is read again and it is no longer missing if the file is there)
	bool Relink(std::wstring_view id, std::filesystem::path const& path);
	// Looks for an item's file in these folders (not below them): the same file name, or - if the item has a FileId - a file of the
	// same kind that has the same id in it. On success the item is linked to it; returns whether that happened.
	bool Locate(std::wstring_view id, std::vector<std::filesystem::path> const& folders);

	// ---- what was open
	ProjectSession const& Session() const noexcept {
		return m_Session;
	}
	void Session(ProjectSession session);

	// ---- the file. On failure they return false and say why in error (a message fit to show the user).
	bool Load(std::filesystem::path const& path, std::wstring& error);
	// writes to FilePath (which must be set: by Load or SaveAs)
	bool Save(std::wstring& error);
	// writes to a new place; the items' paths are worked out again relative to it, and it becomes the project's FilePath
	bool SaveAs(std::filesystem::path const& path, std::wstring& error);

private:
	ProjectItem* FindItem(std::wstring_view id);
	std::wstring StoredPath(std::filesystem::path const& absolute) const;
	void EnsureGroup(std::wstring_view group);
	bool Write(std::filesystem::path const& path, std::wstring& error);

	std::wstring m_Name, m_Description;
	std::filesystem::path m_FilePath;
	std::vector<ProjectItem> m_Items;
	std::vector<std::wstring> m_Groups;
	ProjectSession m_Session;
	IniDocument m_Extra;							// what the project file had that this program doesn't use, put back when it is saved
	bool m_Dirty{ false };
	bool m_SessionDirty{ false };
};
