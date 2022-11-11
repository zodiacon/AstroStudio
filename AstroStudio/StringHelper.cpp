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
