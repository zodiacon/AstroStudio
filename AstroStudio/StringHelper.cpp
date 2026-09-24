#include "pch.h"
#include "StringHelper.h"
#include <AstroCalculator.h>

PCWSTR StringHelper::HouseSystemToString(HouseSystem system) {
	switch (system) {
		using enum HouseSystem;
		case Placidus:			return L"Placidus";
		case Koch:				return L"Koch";
		case Porphyrius:		return L"Porphyrius";
		case Regiomontanus:		return L"Regiomontanus";
		case Campanus:			return L"Campanus";
		case Equal:				return L"Equal";
		case Morinus:			return L"Morinus";
		case Topocentric:		return L"Topocentric";
		case Alcabitus:			return L"Alcabitus";
		case Horizontal:		return L"Horizontal";
		case Krusinski:			return L"Krusinski";
		case EqualWholeSign:	return L"Equal / Whole Sign";
		case CarterPoliEqu:		return L"Carter Poli-Equal";
		case EqualMC:			return L"Equal (MC)";
		case Sunshine:			return L"Sunshine";
		case SunshineAlt:		return L"Sunshine / Alt";
		case APCHouses:			return L"APC Houses";
	}
	ATLASSERT(false);
	return L"";
}

std::wstring StringHelper::Utf8ToWide(std::string const& text) {
	if (text.empty())
		return L"";
	int count = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0);
	std::wstring result(count, L'\0');
	::MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), result.data(), count);
	return result;
}

std::string StringHelper::WideToUtf8(std::wstring const& text) {
	if (text.empty())
		return "";
	int count = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
	std::string result(count, '\0');
	::WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), result.data(), count, nullptr, nullptr);
	return result;
}

std::wstring StringHelper::UrlEncode(std::wstring const& text) {
	std::wstring encoded;
	for (unsigned char ch : WideToUtf8(text)) {
		if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
			encoded += (wchar_t)ch;
		}
		else {
			WCHAR hex[4];
			swprintf_s(hex, L"%%%02X", ch);
			encoded += hex;
		}
	}
	return encoded;
}
