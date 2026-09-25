#include "pch.h"
#include "ProjectItemDlg.h"

namespace {
	// a text with its line breaks as a Windows edit box wants them
	CString ForEdit(std::wstring const& text) {
		CString result(text.c_str());
		result.Replace(L"\r\n", L"\n");
		result.Replace(L"\n", L"\r\n");
		return result;
	}

	std::wstring FromEdit(CString text) {
		text.Replace(L"\r\n", L"\n");
		return std::wstring((PCWSTR)text);
	}
}

LRESULT CProjectItemDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	CenterWindow(GetParent());
	SetWindowText(!m_Caption.empty() ? m_Caption.c_str() : m_ForProject ? L"Project Properties" : L"Properties");
	SetDlgItemText(IDC_PI_NAME, m_Name.c_str());
	SetDlgItemText(IDC_PI_NOTES, ForEdit(m_Notes));
	SetDlgItemText(IDC_PI_PATH, m_Path.c_str());
	CString tags;
	for (auto const& tag : m_Tags)
		tags += (tags.IsEmpty() ? L"" : L", ") + CString(tag.c_str());
	SetDlgItemText(IDC_PI_TAGS, tags);

	if (m_ForProject) {
		// a project has no tags, and the notes are its description
		for (int id : { IDC_PI_TAGS, IDC_PI_TAGSLABEL, IDC_PI_TAGSHINT })
			GetDlgItem(id).ShowWindow(SW_HIDE);
		SetDlgItemText(IDC_PI_NOTESLABEL, L"Description:");
		SetDlgItemText(IDC_PI_PATHLABEL, L"File:");
		if (m_Path.empty())
			for (int id : { IDC_PI_PATH, IDC_PI_PATHLABEL })
				GetDlgItem(id).ShowWindow(SW_HIDE);		// (not saved yet: it has no file)
	}
	GotoDlgCtrl(GetDlgItem(IDC_PI_NAME));
	return FALSE;
}

LRESULT CProjectItemDlg::OnOK(WORD, WORD, HWND, BOOL&) {
	CString text;
	GetDlgItemText(IDC_PI_NAME, text);
	m_Name = (PCWSTR)text;
	GetDlgItemText(IDC_PI_NOTES, text);
	m_Notes = FromEdit(text);
	m_Tags.clear();
	GetDlgItemText(IDC_PI_TAGS, text);
	int position = 0;
	for (auto token = text.Tokenize(L",", position); !token.IsEmpty(); token = text.Tokenize(L",", position))
		m_Tags.push_back((PCWSTR)token.Trim());
	EndDialog(IDOK);
	return 0;
}

LRESULT CProjectItemDlg::OnCancel(WORD, WORD, HWND, BOOL&) {
	EndDialog(IDCANCEL);
	return 0;
}
