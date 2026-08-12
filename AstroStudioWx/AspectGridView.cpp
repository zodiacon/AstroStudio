#include "pch.h"
#include "AspectGridView.h"

#include <wx/dcbuffer.h>
#include <wx/graphics.h>

AspectGridView::AspectGridView(wxWindow* parent)
	: wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxHSCROLL | wxVSCROLL) {
	SetBackgroundStyle(wxBG_STYLE_PAINT);
	SetScrollRate(FromDIP(8), FromDIP(8));

	Bind(wxEVT_PAINT, &AspectGridView::OnPaint, this);
}

void AspectGridView::SetChartData(ChartData const* data) {
	m_ChartData = data;
	m_Drawing.Chart(data);
}

void AspectGridView::SetAspects(std::vector<AspectData> aspects) {
	m_Aspects = std::move(aspects);
	m_Drawing.Aspects(&m_Aspects);
}

void AspectGridView::Refresh() {
	// Replaces CChartView::UpdateAspectGridScrollSize(): the scroll extent is
	// this window's own business now, not the parent's.
	auto size = m_Drawing.GridSize();
	SetVirtualSize(size, size);
	wxScrolledWindow::Refresh();
}

void AspectGridView::OnPaint(wxPaintEvent&) {
	wxAutoBufferedPaintDC dc(this);

	dc.SetBackground(wxBrush(GetBackgroundColour()));
	dc.Clear();

	if (!m_ChartData)
		return;

	//
	// The scroll offset is passed to the drawing rather than applied with
	// DoPrepareDC: the drawing paints through both a graphics context and the
	// DC, and a wxGraphicsContext built from a wxDC does not reliably inherit
	// that DC's logical origin, so the two would disagree about where the
	// origin is.
	//
	int viewX = 0, viewY = 0, unitX = 0, unitY = 0;
	GetViewStart(&viewX, &viewY);
	GetScrollPixelsPerUnit(&unitX, &unitY);

	m_Drawing.Draw(dc, wxPoint(-viewX * unitX, -viewY * unitY));
}
