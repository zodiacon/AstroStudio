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

	// what is drawn in one colour: the glyphs, the outlines of the belts and the lines of the angles; the house lines; the
	// dots that mark the planets' places
	D2D1_COLOR_F TextColor{ D2D1::ColorF(D2D1::ColorF::Black) };
	D2D1_COLOR_F GridColor{ D2D1::ColorF(D2D1::ColorF::Gray) };
	D2D1_COLOR_F DotColor{ D2D1::ColorF(D2D1::ColorF::Blue) };

	float MajorAspectWidth{ 3 };
	float MinorAspectWidth{ 1.5f };
	float ZodiacBeltWidth{ 40 };

	bool DrawAspects{ true };
	bool FillZodiacBelts{ true };
	bool DrawHouseLines{ true };
	bool DrawVeryMinorAspects{ true };
	bool DrawNonStandardPlanetAspects{ false };

	// the colours for a dark background (the defaults are for a light one)
	static ChartDrawingParameters Dark() {
		ChartDrawingParameters p;
		p.BackColor = ColorFromRgb(30, 30, 30);
		p.ElementColor = { ColorFromRgb(190, 65, 15), ColorFromRgb(140, 128, 45), ColorFromRgb(50, 135, 70), ColorFromRgb(50, 105, 150) };
		p.SoftAspectColor = ColorFromRgb(100, 160, 255);
		p.HardAspectColor = ColorFromRgb(255, 95, 95);
		p.MinorAspectColor = ColorFromRgb(195, 120, 235);
		p.AspectColor = ColorFromRgb(225, 225, 225);
		p.TextColor = ColorFromRgb(230, 230, 230);
		p.GridColor = ColorFromRgb(115, 115, 115);
		p.DotColor = ColorFromRgb(110, 170, 255);
		p.TransitBandColor = ColorFromRgb(42, 52, 72);
		p.TransitColor = ColorFromRgb(255, 115, 135);
		return p;
	}

	// with transits: the band the transiting planets stand in, and their color
	D2D1_COLOR_F TransitBandColor{ D2D1::ColorF(0.90f, 0.93f, 0.98f) };
	D2D1_COLOR_F TransitColor{ D2D1::ColorF(D2D1::ColorF::Crimson) };
};

// What is under a point of the chart wheel.
struct ChartHit {
	enum class Kind { None, Planet, Aspect, TransitPlanet, TransitAspect };
	Kind Type{ Kind::None };
	// a planet: its index in the chart's (or, for a transit, the transits') planets; an aspect: its index in the
	// aspects (or transit aspects) the drawing was given
	int Index{ -1 };
};

// A planet picked to highlight: in the chart, or among the transiting planets (a chart has both when it shows transits).
struct ChartSelection {
	Planet Planet;
	bool Transit{ false };
	bool operator==(ChartSelection const&) const = default;
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
	D2DChartDrawing& Highlight(std::optional<ChartSelection> planet);
	std::optional<ChartSelection> Highlight() const;

	// Transits: a bi-wheel with the chart shrunk inside a band holding the planets of another moment. The aspects are
	// the ones between the transiting planets (Planet1) and the chart's (Planet2); the chart's own aspects are not
	// drawn then. The caption is written in a corner. Null data: no transits.
	D2DChartDrawing& Transits(ChartData* data);
	D2DChartDrawing& TransitAspects(std::vector<AspectData>* aspects);
	D2DChartDrawing& TransitCaption(std::wstring caption);

	// The planet or aspect line at a point in the 1000x1000 logical space, as the last Draw drew them.
	// Planets win over the aspect lines that end at them.
	ChartHit HitTest(D2D1_POINT_2F const& point) const;
	static constexpr D2D1_POINT_2F Center{ 500, 500 };

private:
	HRESULT DrawChart(ID2D1RenderTarget* rt);
	HRESULT DrawNatal(ID2D1RenderTarget* rt);
	HRESULT DrawTransitBand(ID2D1RenderTarget* rt);
	HRESULT DrawTransits(ID2D1RenderTarget* rt);
	// a point of the chart's own (possibly shrunk) part of the wheel as it is on the wheel
	D2D1_POINT_2F Map(D2D1_POINT_2F const& pt) const;
	D2D1_POINT_2F PointByAngle(D2D1_POINT_2F const& center, float radius, double angle) const;

	ChartDrawingParameters m_params;
	ChartData* m_data{ nullptr };
	std::vector<AspectData>* m_aspects{ nullptr };
	double m_rotation{ 0 };
	std::optional<ChartSelection> m_highlight;
	ChartData* m_transits{ nullptr };
	std::vector<AspectData>* m_transitAspects{ nullptr };
	std::wstring m_transitCaption;
	float m_scale{ 1 };

	// where the last Draw put things
	struct PlanetSpot {
		D2D1_POINT_2F Point;
		int Index;
	};
	struct AspectLine {
		D2D1_POINT_2F From, To, Middle;
		int Index;
	};
	std::vector<PlanetSpot> m_planetSpots, m_transitSpots;
	std::vector<AspectLine> m_aspectLines, m_transitLines;
};
