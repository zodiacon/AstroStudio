#pragma once

#include <DialogHelper.h>
#include "resource.h"

// The properties of an item of a project - its name, the file it links, its tags and notes - or, for the project itself, its name and
// description.
class CProjectItemDlg :
	public CDialogImpl<CProjectItemDlg>,
	public CDialogHelper<CProjectItemDlg> {
public:
	enum { IDD = IDD_PROJECTITEM };

	// for an item; `path` is shown, not edited
	void InitItem(std::wstring name, std::vector<std::wstring> tags, std::wstring notes, std::wstring path) {
		m_ForProject = false;
		m_Name = std::move(name);
		m_Tags = std::move(tags);
		m_Notes = std::move(notes);
		m_Path = std::move(path);
	}
	// for the project: a name and a description (which is what Notes then are). Without a path (a project that hasn't been saved yet)
	// the file isn't shown; the caption, if given, is the window's title.
	void InitProject(std::wstring name, std::wstring description, std::wstring path, PCWSTR caption = nullptr) {
		m_ForProject = true;
		m_Name = std::move(name);
		m_Notes = std::move(description);
		m_Path = std::move(path);
		m_Caption = caption ? caption : L"";
	}
	// valid after DoModal returned IDOK
	std::wstring const& Name() const {
		return m_Name;
	}
	std::vector<std::wstring> const& Tags() const {
		return m_Tags;
	}
	std::wstring const& Notes() const {
		return m_Notes;
	}

	BEGIN_MSG_MAP(CProjectItemDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnOK(WORD, WORD, HWND, BOOL&);
	LRESULT OnCancel(WORD, WORD, HWND, BOOL&);

	bool m_ForProject{ false };
	std::wstring m_Name, m_Notes, m_Path, m_Caption;
	std::vector<std::wstring> m_Tags;
};
