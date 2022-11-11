#pragma once

enum class HouseSystem;

struct StringHelper abstract final {
	static PCWSTR HouseSystemToString(HouseSystem system);
};

