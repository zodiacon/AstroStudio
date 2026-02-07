#pragma once

#include "ChartData.h"
#include "Aspects.h"
#include <array>

struct ChartDrawingParameters {
	Gdiplus::Color BackColor{ Gdiplus::Color(Gdiplus::Color::WhiteSmoke) };
	std::array<Gdiplus::Color, 4> ElementColor {
		Gdiplus::Color(Gdiplus::Color::OrangeRed),
		Gdiplus::Color(Gdiplus::Color::LightGoldenrodYellow),
		Gdiplus::Color(Gdiplus::Color::LightGreen),
		Gdiplus::Color(Gdiplus::Color::LightBlue)
	};

	Gdiplus::Color SoftAspectColor{ Gdiplus::Color(Gdiplus::Color::Blue) };
	Gdiplus::Color HardAspectColor{ Gdiplus::Color(Gdiplus::Color::Red) };
	Gdiplus::Color MinorAspectColor{ Gdiplus::Color(Gdiplus::Color::Purple) };
	Gdiplus::Color AspectColor{ Gdiplus::Color(Gdiplus::Color::Black) };

	float MajorAspectWidth{ 3 };
	float MinorAspectWidth{ 1.5f };
	float ZodiacBeltWidth{ 40 };

	bool DrawAspects{ true };
	bool FillZodiacBelts{ true };
	bool DrawHouseLines{ true };
	bool DrawVeryMinorAspects{ true };
	bool DrawNonStandardPlanetAspects{ false };
};

class ChartDrawing {
public:
	ChartDrawing() = default;

	virtual bool Draw(Gdiplus::Graphics& g, int size);
	ChartDrawing& DrawingParameters(ChartDrawingParameters const&);
	ChartDrawingParameters const& DrawingParameters() const;
	ChartDrawingParameters& DrawingParameters();
	ChartDrawing& Chart(ChartData* data);
	ChartData* Chart() const;
	ChartDrawing& Aspects(std::vector<AspectData>* aspects);

private:
	Gdiplus::PointF PointByAngle(Gdiplus::PointF const& center, float radius, double angle) const;

	ChartDrawingParameters m_params;
	ChartData* m_data{ nullptr };
	std::vector<AspectData>* m_aspects { nullptr};
};

