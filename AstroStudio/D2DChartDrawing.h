#pragma once

#include "ChartDrawing.h"
// pch.h targets Windows 7, which hides the DirectWrite in-memory font loader (Windows 10 1709+) that
// the embedded glyph font needs; raise the target for these headers only (d2d1.h pulls in dcommon.h,
// which dwrite_3.h needs at the same level)
#pragma push_macro("NTDDI_VERSION")
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN10_RS3
#include <d2d1.h>
#include <dwrite_3.h>
#pragma pop_macro("NTDDI_VERSION")

// Direct2D/DirectWrite counterpart of ChartDrawing. Uses the same ChartDrawingParameters and the same
// 1000x1000 logical coordinate space, so the two renderers produce the same chart.
class D2DChartDrawing {
public:
	D2DChartDrawing() = default;
	D2DChartDrawing(D2DChartDrawing const&) = delete;
	D2DChartDrawing& operator=(D2DChartDrawing const&) = delete;
	~D2DChartDrawing();

	// Draws the chart into the top-left size x size DIPs of the render target, clearing just that square.
	// The caller owns BeginDraw/EndDraw (and so D2DERR_RECREATE_TARGET handling).
	HRESULT Draw(ID2D1RenderTarget* rt, float size);
	D2DChartDrawing& DrawingParameters(ChartDrawingParameters const&);
	ChartDrawingParameters const& DrawingParameters() const;
	ChartDrawingParameters& DrawingParameters();
	D2DChartDrawing& Chart(ChartData* data);
	ChartData* Chart() const;
	D2DChartDrawing& Aspects(std::vector<AspectData>* aspects);

private:
	HRESULT EnsureTextResources();
	HRESULT LoadAstroFontCollection();
	HRESULT DrawChart(ID2D1RenderTarget* rt);
	D2D1_POINT_2F PointByAngle(D2D1_POINT_2F const& center, float radius, double angle) const;

	ChartDrawingParameters m_params;
	ChartData* m_data{ nullptr };
	std::vector<AspectData>* m_aspects{ nullptr };

	// device-independent resources, kept across draws and render target recreation
	CComPtr<IDWriteFactory5> m_dwFactory;
	CComPtr<IDWriteInMemoryFontFileLoader> m_fontLoader;
	CComPtr<IDWriteFontCollection1> m_fontCollection;
	CComPtr<IDWriteTextFormat> m_glyphFormat;	// zodiac sign and aspect glyphs
	CComPtr<IDWriteTextFormat> m_planetFormat;
};
