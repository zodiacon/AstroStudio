#include "pch.h"
#include "D2DResources.h"
#include "resource.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace {
	// 17 and 20 points, in DIPs at 96 DPI
	constexpr float GlyphFontSize = 17 * 96.0f / 72;
	constexpr float PlanetFontSize = 20 * 96.0f / 72;
}

D2DResources& D2DResources::Get() {
	// deliberately never destroyed: the objects live for the whole process, and releasing COM objects
	// during static destruction at exit is order-dependent for no benefit
	static auto resources = new D2DResources;
	return *resources;
}

HRESULT D2DResources::Ensure() {
	if (m_d2dFactory && m_glyphFormat && m_planetFormat)
		return S_OK;

	HRESULT hr = S_OK;
	if (!m_d2dFactory) {
		hr = ::D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2dFactory);
		if (FAILED(hr))
			return hr;
	}
	if (!m_dwFactory) {
		hr = ::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory5), reinterpret_cast<IUnknown**>(&m_dwFactory));
		if (FAILED(hr))
			return hr;
	}
	if (!m_fontCollection) {
		hr = LoadAstroFontCollection();
		if (FAILED(hr))
			return hr;
	}

	CComPtr<IDWriteFontFamily> family;
	hr = m_fontCollection->GetFontFamily(0, &family);
	if (FAILED(hr))
		return hr;
	CComPtr<IDWriteLocalizedStrings> names;
	hr = family->GetFamilyNames(&names);
	if (FAILED(hr))
		return hr;
	UINT32 length = 0;
	hr = names->GetStringLength(0, &length);
	if (FAILED(hr))
		return hr;
	std::wstring familyName(length, L'\0');
	hr = names->GetString(0, familyName.data(), length + 1);
	if (FAILED(hr))
		return hr;
	m_glyphFamily = std::move(familyName);

	CComPtr<IDWriteTextFormat> glyphFormat, planetFormat;
	hr = CreateGlyphFormat(GlyphFontSize, &glyphFormat);
	if (FAILED(hr))
		return hr;
	hr = CreateGlyphFormat(PlanetFontSize, &planetFormat);
	if (FAILED(hr))
		return hr;

	m_glyphFormat = glyphFormat;
	m_planetFormat = planetFormat;
	return S_OK;
}

HRESULT D2DResources::CreateWindowRenderTarget(HWND hWnd, ID2D1HwndRenderTarget** rt) const {
	if (!m_d2dFactory)
		return E_UNEXPECTED;	// Ensure() not called
	RECT rc;
	::GetClientRect(hWnd, &rc);
	auto props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(), 96, 96);
	return m_d2dFactory->CreateHwndRenderTarget(props, D2D1::HwndRenderTargetProperties(hWnd, D2D1::SizeU(rc.right, rc.bottom)), rt);
}

HRESULT D2DResources::CreateGlyphFormat(float size, IDWriteTextFormat** format) const {
	return CreateFormat(m_glyphFamily.c_str(), m_fontCollection, size, format);
}

HRESULT D2DResources::CreateTextFormat(float size, IDWriteTextFormat** format) const {
	return CreateFormat(L"Segoe UI", nullptr, size, format);	// nullptr: the system font collection
}

HRESULT D2DResources::CreateFormat(PCWSTR family, IDWriteFontCollection* collection, float size, IDWriteTextFormat** format) const {
	auto hr = m_dwFactory->CreateTextFormat(family, collection, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", format);
	if (SUCCEEDED(hr))
		hr = (*format)->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	if (SUCCEEDED(hr))
		hr = (*format)->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
	return hr;
}

// DirectWrite cannot see the font GDI registered with AddFontMemResourceEx, and neither can GDI+ (see
// Helpers::LoadAstroFont), so it gets its own collection built from the same embedded resource.
HRESULT D2DResources::LoadAstroFontCollection() {
	auto res = ::FindResource(nullptr, MAKEINTRESOURCE(IDR_FONT), L"TTF");
	if (!res)
		return HRESULT_FROM_WIN32(::GetLastError());
	auto hGlobal = ::LoadResource(nullptr, res);
	if (!hGlobal)
		return HRESULT_FROM_WIN32(::GetLastError());
	auto size = ::SizeofResource(nullptr, res);
	auto data = ::LockResource(hGlobal);
	if (!data)
		return E_FAIL;

	CComPtr<IDWriteInMemoryFontFileLoader> loader;
	auto hr = m_dwFactory->CreateInMemoryFontFileLoader(&loader);
	if (FAILED(hr))
		return hr;
	hr = m_dwFactory->RegisterFontFileLoader(loader);
	if (FAILED(hr))
		return hr;

	// the resource lives as long as the module, so the loader needs no owner object to keep the data alive
	CComPtr<IDWriteFontFile> file;
	CComPtr<IDWriteFontSetBuilder1> builder;
	CComPtr<IDWriteFontSet> fontSet;
	CComPtr<IDWriteFontCollection1> collection;
	hr = loader->CreateInMemoryFontFileReference(m_dwFactory, data, size, nullptr, &file);
	if (SUCCEEDED(hr))
		hr = m_dwFactory->CreateFontSetBuilder(&builder);
	if (SUCCEEDED(hr))
		hr = builder->AddFontFile(file);
	if (SUCCEEDED(hr))
		hr = builder->CreateFontSet(&fontSet);
	if (SUCCEEDED(hr))
		hr = m_dwFactory->CreateFontCollectionFromFontSet(fontSet, &collection);

	if (FAILED(hr)) {
		m_dwFactory->UnregisterFontFileLoader(loader);
		return hr;
	}
	// the loader stays registered (and referenced) for the life of the process, as the collection reads through it
	m_fontLoader = loader;
	m_fontCollection = collection;
	return S_OK;
}
