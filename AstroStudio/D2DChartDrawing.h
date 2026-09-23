#pragma once

#include "ChartData.h"
#include "Aspects.h"
#include "D2DResources.h"
#include <array>

struct ChartDrawingParameters {
	D2D1_COLOR_F BackColor{ D2D1::ColorF(D2D1::ColorF::WhiteSmoke) };
	std::array<D2D1_COLOR_F, 4> ElementColor {
		D2D1::ColorF(D2D1::ColorF::OrangeRed),
		D2D1::ColorF(D2D1::ColorF::LightGoldenrodYellow),
		D2D1::ColorF(D2D1::ColorF::LightGreen),
		D2D1::ColorF(D2D1::ColorF::LightBlue)
	};

	D2D1_COLOR_F SoftAspectColor{ D2D1::ColorF(D2D1::ColorF::Blue) };
	D2D1_COLOR_F HardAspectColor{ D2D1::ColorF(D2D1::ColorF::Red) };
	D2D1_COLOR_F MinorAspectColor{ D2D1::ColorF(D2D1::ColorF::Purple) };
	D2D1_COLOR_F AspectColor{ D2D1::ColorF(D2D1::ColorF::Black) };

	float MajorAspectWidth{ 3 };
	float MinorAspectWidth{ 1.5f };
	float ZodiacBeltWidth{ 40 };

	bool DrawAspects{ true };
	bool FillZodiacBelts{ true };
	bool DrawHouseLines{ true };
	bool DrawVeryMinorAspects{ true };
	bool DrawNonStandardPlanetAspects{ false };
};

// Draws the chart wheel with Direct2D/DirectWrite in a 1000x1000 logical coordinate space.
class D2DChartDrawing {
public:
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
	HRESULT DrawChart(ID2D1RenderTarget* rt);
	D2D1_POINT_2F PointByAngle(D2D1_POINT_2F const& center, float radius, double angle) const;

	ChartDrawingParameters m_params;
	ChartData* m_data{ nullptr };
	std::vector<AspectData>* m_aspects{ nullptr };
};
