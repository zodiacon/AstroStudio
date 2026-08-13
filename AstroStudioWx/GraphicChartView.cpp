#include "pch.h"
#include "GraphicChartView.h"
#include "AstroHelpers.h"

#include <wx/dcbuffer.h>
#include <wx/graphics.h>

GraphicChartView::GraphicChartView(wxWindow* parent)
	: wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE) {
	// Together these replace the m_Bitmap backbuffer and the WM_ERASEBKGND
	// handler: wx composites the whole paint off-screen.
	SetBackgroundStyle(wxBG_STYLE_PAINT);

	Bind(wxEVT_PAINT, &GraphicChartView::OnPaint, this);
}

void GraphicChartView::SetChartData(ChartData const* data) {
	m_ChartData = data;
}

void GraphicChartView::SetAspects(std::vector<AspectData> aspects) {
	m_Aspects = std::move(aspects);
}

void GraphicChartView::Refresh() {
	wxWindow::Refresh();
}

void GraphicChartView::OnPaint(wxPaintEvent&) {
	wxAutoBufferedPaintDC dc(this);

	// CGraphicChartView::OnPaint filled the strip left over beside the square
	// wheel with LightGray; using the window background instead means it
	// follows the theme.
	dc.SetBackground(wxBrush(GetBackgroundColour()));
	dc.Clear();

	auto client = GetClientSize();
	auto size = std::min(client.x, client.y);
	if (size <= 0)
		return;

	//
	// Re-read the theme on every paint rather than caching it: the drawing
	// classes stay free of any UI/theme dependency, and a system theme change
	// needs no notification plumbing to take effect.
	//
	// The aspect colours are deliberately left alone. Red for hard and blue for
	// soft are semantic to astrologers, not decoration.
	//
	auto& params = m_Drawing.DrawingParameters();
	params.BackColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
	params.ForeColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
	params.HouseLineColor = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
	params.AspectColor = params.ForeColor;
	for (int i = 0; i < 4; i++)
		params.ElementColor[i] = AstroHelpers::ElementColour(i);

	m_Drawing.Chart(m_ChartData);
	m_Drawing.Aspects(&m_Aspects);
	m_Drawing.Draw(dc, size);
}
