#include "pch.h"
#include "GraphicChartView.h"

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

	m_Drawing.Chart(m_ChartData);
	m_Drawing.Aspects(&m_Aspects);
	m_Drawing.Draw(dc, size);
}
