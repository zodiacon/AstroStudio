#include "pch.h"
#include "NetworkHelper.h"
#include "ChartData.h"
#include "WinHttp.h"
#include "Json.h"
#include <atlcomcli.h>
#include <LocationApi.h>

#pragma comment(lib, "wininet")
#pragma comment(lib, "uuid")

std::string NetworkHelper::GetExternalIP() {
	std::string result;
	auto net = ::InternetOpen(L"IP retriever",
		INTERNET_OPEN_TYPE_PRECONFIG,
		nullptr, nullptr, 0);
	if (net) {
		auto conn = ::InternetOpenUrl(net,
			L"http://myexternalip.com/raw",
			nullptr, 0, INTERNET_FLAG_RELOAD, 0);
		if (conn) {
			char buffer[256];
			DWORD read;

			if (::InternetReadFile(conn, buffer, sizeof(buffer) / sizeof(buffer[0]), &read))
				result = std::string(buffer, read);
			::InternetCloseHandle(conn);
		}
		::InternetCloseHandle(net);
	}
	return result;
}

bool NetworkHelper::FillInfoFromLocal(ChartInfo& info) {
	auto ip = GetExternalIP();
	if (ip.empty())
		return false;

	//const std::wstring domain = L"tools.keycdn.com";
	//const std::wstring requestHeader = L"User-Agent: keycdn-tools:https://scorpiosoftware.net";
	//int port = 443;
	//bool https = true;

	//HttpRequest req(domain, port, https);
	//HttpResponse response;

	//if (req.Get(L"/geo.json?host=" + std::wstring(ip.begin(), ip.end()), requestHeader, response)) {
	//    using namespace nlohmann;
	//    try {
	//        auto json = json::parse(response.text.begin(), response.text.end());
	//        auto geo = json["data"]["geo"];
	//        info.Latitude = (double)geo["latitude"];
	//        info.Longitude = (double)geo["longitude"];
	//        auto country = (std::string)geo["country_name"];
	//        info.Country.assign(country.begin(), country.end());
	//        auto state = (std::string)geo["region_name"];
	//        if (!state.empty())
	//            info.State.assign(state.begin(), state.end());
	//        auto city = (std::string)geo["city"];
	//        info.City.assign(city.begin(), city.end());
	//    }
	//    catch (...) {
	//        return false;
	//    }
	//    return true;
	//}

	HttpRequest req(L"ipwho.is", 443, true);
	HttpResponse response;
	if (req.Get(L"/" + std::wstring(ip.begin(), ip.end()), L"", response)) {
		using namespace nlohmann;
		auto geo = json::parse(response.text.begin(), response.text.end());
		info.Latitude = (double)geo["latitude"];
		info.Longitude = (double)geo["longitude"];
		auto country = (std::string)geo["country"];
		info.Country.assign(country.begin(), country.end());
		auto state = (std::string)geo["region"];
		if (!state.empty())
			info.State.assign(state.begin(), state.end());
		auto city = (std::string)geo["city"];
		info.City.assign(city.begin(), city.end());
		return true;
	}
	return false;
}

bool NetworkHelper::FillInfoFromGPS(ChartInfo& info, HWND hParent, DWORD timeoutMs) {
	CComPtr<ILocation> location;
	if (FAILED(location.CoCreateInstance(__uuidof(Location))))
		return false;

	IID reportTypes[] = { __uuidof(ILatLongReport) };
	if (FAILED(location->RequestPermissions(hParent, reportTypes, _countof(reportTypes), TRUE)))
		return false;

	location->SetReportInterval(__uuidof(ILatLongReport), 1000);

	CComPtr<ILocationReport> report;
	auto start = ::GetTickCount64();
	do {
		if (SUCCEEDED(location->GetReport(__uuidof(ILatLongReport), &report)) && report)
			break;
		::Sleep(250);
	} while (::GetTickCount64() - start < timeoutMs);

	if (!report)
		return false;

	CComQIPtr<ILatLongReport> latLong(report);
	if (!latLong)
		return false;

	DOUBLE lat, lon, alt;
	if (FAILED(latLong->GetLatitude(&lat)) || FAILED(latLong->GetLongitude(&lon)))
		return false;

	info.Latitude = lat;
	info.Longitude = lon;
	if (SUCCEEDED(latLong->GetAltitude(&alt)))
		info.Elevation = alt;

	return true;
}

bool NetworkHelper::FillInfoFromCurrentLocation(ChartInfo& info, HWND hParent, DWORD gpsTimeoutMs) {
	if (FillInfoFromGPS(info, hParent, gpsTimeoutMs)) {
		info.City = L"GPS location";
		info.State.clear();
		info.Country.clear();
		return true;
	}
	return FillInfoFromLocal(info);
}
