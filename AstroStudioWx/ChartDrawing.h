#pragma once

#include "ChartData.h"
#include "Aspects.h"
#include <array>

class wxGraphicsContext;

//
// Port of AstroStudio\ChartDrawing.h. Same 1000x1000 logical coordinate space,
// same geometry; Gdiplus::Color becomes wxColour.
//
// Three colours that the WTL version hard-coded inside Draw() are parameters
// here: ForeColor (Color::Black for the ASC/MC pen, the zodiac outline and
// every glyph), HouseLineColor (Color::Gray) and PlanetDotColor (Color::Blue).
// They keep the original values by default, so output is unchanged - but the
// wheel can now be themed without editing the drawing code, which it could not
// be before.
//
struct ChartDrawingParameters {
	wxColour BackColor{ 245, 245, 245 };			// WhiteSmoke
	wxColour ForeColor{ 0, 0, 0 };					// was hard-coded Color::Black
	wxColour HouseLineColor{ 128, 128, 128 };		// was hard-coded Color::Gray
	wxColour PlanetDotColor{ 0, 0, 255 };			// was hard-coded Color::Blue

	std::array<wxColour, 4> ElementColor{
		wxColour(255, 69, 0),		// OrangeRed
		wxColour(250, 250, 210),	// LightGoldenrodYellow
		wxColour(144, 238, 144),	// LightGreen
		wxColour(173, 216, 230),	// LightBlue
	};

	wxColour SoftAspectColor{ 0, 0, 255 };			// Blue
	wxColour HardAspectColor{ 255, 0, 0 };			// Red
	wxColour MinorAspectColor{ 128, 0, 128 };		// Purple
	wxColour AspectColor{ 0, 0, 0 };				// Black

	double MajorAspectWidth{ 3 };
	double MinorAspectWidth{ 1.5 };
	double ZodiacBeltWidth{ 40 };

	bool DrawAspects{ true };
	bool FillZodiacBelts{ true };
	bool DrawHouseLines{ true };
	bool DrawVeryMinorAspects{ true };
	bool DrawNonStandardPlanetAspects{ false };
};

class ChartDrawing {
public:
	ChartDrawing() = default;
	virtual ~ChartDrawing() = default;

	//
	// size is the edge of the square the wheel is drawn into, in device pixels.
	//
	// Takes the DC rather than a wxGraphicsContext because the two have to be
	// used together: shapes go through a graphics context created here, glyphs
	// through the DC. See AstroHelpers::GlyphRun.
	//
	virtual bool Draw(wxDC& dc, int size);

	ChartDrawing& DrawingParameters(ChartDrawingParameters const& params);
	ChartDrawingParameters const& DrawingParameters() const;
	ChartDrawingParameters& DrawingParameters();

	ChartDrawing& Chart(ChartData const* data);
	ChartData const* Chart() const;
	ChartDrawing& Aspects(std::vector<AspectData> const* aspects);

private:
	wxPoint2DDouble PointByAngle(wxPoint2DDouble const& center, double radius, double angle) const;

	ChartDrawingParameters m_params;
	ChartData const* m_data{};
	std::vector<AspectData> const* m_aspects{};
};
