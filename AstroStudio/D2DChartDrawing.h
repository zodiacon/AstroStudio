#pragma once

#include "ChartData.h"
#include "Aspects.h"
#include "D2DResources.h"
#include <array>
#include <optional>

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

// What is under a point of the chart wheel.
struct ChartHit {
	enum class Kind { None, Planet, Aspect };
	Kind Type{ Kind::None };
	// a planet: its index in the chart's planets; an aspect: its index in the aspects the drawing was given
	int Index{ -1 };
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

	// Turns the wheel counterclockwise by this many degrees; 0 has the ascendant at the left.
	D2DChartDrawing& Rotation(double degrees);
	double Rotation() const;
	// Draws this planet's aspects as usual and fades all the others (none: no fading).
	D2DChartDrawing& Highlight(std::optional<Planet> planet);
	std::optional<Planet> Highlight() const;

	// The planet or aspect line at a point in the 1000x1000 logical space, as the last Draw drew them.
	// Planets win over the aspect lines that end at them.
	ChartHit HitTest(D2D1_POINT_2F const& point) const;
	static constexpr D2D1_POINT_2F Center{ 500, 500 };

private:
	HRESULT DrawChart(ID2D1RenderTarget* rt);
	D2D1_POINT_2F PointByAngle(D2D1_POINT_2F const& center, float radius, double angle) const;

	ChartDrawingParameters m_params;
	ChartData* m_data{ nullptr };
	std::vector<AspectData>* m_aspects{ nullptr };
	double m_rotation{ 0 };
	std::optional<Planet> m_highlight;

	// where the last Draw put things
	struct PlanetSpot {
		D2D1_POINT_2F Point;
		int Index;
	};
	struct AspectLine {
		D2D1_POINT_2F From, To, Middle;
		int Index;
	};
	std::vector<PlanetSpot> m_planetSpots;
	std::vector<AspectLine> m_aspectLines;
};
