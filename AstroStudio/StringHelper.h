#pragma once

enum class HouseSystem;

struct StringHelper abstract final {
	static PCWSTR HouseSystemToString(HouseSystem system);

	static std::wstring Utf8ToWide(std::string const& text);
	static std::string WideToUtf8(std::wstring const& text);
	// percent-encodes the UTF-8 form of the text for a URL query value
	static std::wstring UrlEncode(std::wstring const& text);
};

