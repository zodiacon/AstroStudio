#pragma once

// pch.h targets Windows 8.1, which hides the DirectWrite in-memory font loader (Windows 10 1709+) that
// the embedded glyph font needs; raise the target for these headers only (d2d1.h pulls in dcommon.h,
// which dwrite_3.h needs at the same level)
#pragma push_macro("NTDDI_VERSION")
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN10_RS3
#include <d2d1.h>
#include <dwrite_3.h>
#pragma pop_macro("NTDDI_VERSION")

inline D2D1_COLOR_F ColorFromRgb(int r, int g, int b) {
	return D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f);
}

// Device-independent Direct2D/DirectWrite objects shared by every chart view: the factories, the
// collection holding the embedded glyph font and the text formats built from it. Only the render target
// is per window. UI thread only (the D2D factory is single-threaded).
class D2DResources final {
public:
	static D2DResources& Get();

	// Creates everything on first use; call before using the accessors. Safe to call again after a failure.
	HRESULT Ensure();

	ID2D1Factory* Factory() const noexcept {
		return m_d2dFactory;
	}
	IDWriteTextFormat* GlyphFormat() const noexcept {	// zodiac sign and aspect glyphs
		return m_glyphFormat;
	}
	IDWriteTextFormat* PlanetFormat() const noexcept {
		return m_planetFormat;
	}

	// render target for a window's client area; fixed at 96 DPI so a DIP is a pixel
	HRESULT CreateWindowRenderTarget(HWND hWnd, ID2D1HwndRenderTarget** rt) const;

	// centered text formats of arbitrary size (in DIPs), for the embedded glyph font or for ordinary text
	HRESULT CreateGlyphFormat(float size, IDWriteTextFormat** format) const;
	HRESULT CreateTextFormat(float size, IDWriteTextFormat** format) const;
	// the family CreateTextFormat makes text in: Segoe UI, or the font the user chose with Options > Font
	std::wstring const& TextFontFamily() const noexcept {
		return m_textFamily;
	}
	void TextFontFamily(std::wstring family) {
		m_textFamily = family.empty() ? L"Segoe UI" : std::move(family);
	}

private:
	HRESULT CreateFormat(PCWSTR family, IDWriteFontCollection* collection, float size, IDWriteTextFormat** format) const;
	D2DResources() = default;
	HRESULT LoadAstroFontCollection();

	CComPtr<ID2D1Factory> m_d2dFactory;
	CComPtr<IDWriteFactory5> m_dwFactory;
	CComPtr<IDWriteInMemoryFontFileLoader> m_fontLoader;
	CComPtr<IDWriteFontCollection1> m_fontCollection;
	std::wstring m_glyphFamily;
	std::wstring m_textFamily{ L"Segoe UI" };
	CComPtr<IDWriteTextFormat> m_glyphFormat;
	CComPtr<IDWriteTextFormat> m_planetFormat;
};
