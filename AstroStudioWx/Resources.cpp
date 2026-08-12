#include "pch.h"
#include "Resources.h"

#include <wx/iconbndl.h>
#include <unordered_map>

wxBitmapBundle Resources::Icon(wxString const& name) {
	// Decoding an .ico group is not free and the toolbar/tab code asks for the
	// same handful of icons repeatedly, so keep them.
	static std::unordered_map<std::wstring, wxBitmapBundle> cache;

	auto key = name.ToStdWstring();
	if (auto it = cache.find(key); it != cache.end())
		return it->second;

	// wxGetInstance() lives in the internal wx/msw/private.h, so ask Win32
	// directly for this executable's module handle.
	auto module = reinterpret_cast<WXHINSTANCE>(::GetModuleHandle(nullptr));

	auto bundle = wxBitmapBundle::FromIconBundle(wxIconBundle(name, module));
	cache.emplace(key, bundle);
	return bundle;
}
