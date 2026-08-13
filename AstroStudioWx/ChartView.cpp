#include "pch.h"
#include "ChartView.h"

#include "AspectGridView.h"
#include "AspectListView.h"
#include "AstroHelpers.h"
#include "ChartDetailsView.h"
#include "GraphicChartView.h"
#include "Aspects.h"

#include <wx/splitter.h>
#include <wx/notebook.h>


ChartView::ChartView(wxWindow* parent, IMainFrame* frame)
	: wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxTAB_TRAVERSAL | wxCLIP_CHILDREN), m_Frame(frame) {

	//
	// wxCLIP_CHILDREN throughout the chain is what CChartView::OnCreate got
	// from passing WS_CLIPCHILDREN | WS_CLIPSIBLINGS to every Create() call.
	// Without it, a live sash drag has each parent repaint its background
	// across the area its children are about to paint over, which shows up as
	// flicker.
	//
	m_Splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxSP_LIVE_UPDATE | wxSP_THIN_SASH | wxCLIP_CHILDREN);
	m_Splitter->SetMinimumPaneSize(FromDIP(150));

	m_ChartDrawing = new GraphicChartView(m_Splitter);
	m_DetailsTabs = new wxNotebook(m_Splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxCLIP_CHILDREN);

	//
	// Clipping children is not enough for the tab strip itself: that band is
	// the notebook's own client area, not covered by any child, so the
	// notebook still erases and redraws it on every size step of the drag.
	// Compositing the notebook (WS_EX_COMPOSITED on MSW) moves that
	// erase-then-draw off-screen.
	//
	m_DetailsTabs->SetDoubleBuffered(true);

	// Edits here feed straight back into the model, so the view asks us to
	// recalculate - the callback that replaces SendMessage(WM_RECALC).
	m_DetailsView = new ChartDetailsView(m_DetailsTabs,
		[this](Recalc what) { Recalculate(what); });

	m_AspectGrid = new AspectGridView(m_DetailsTabs);

	m_AspectList = new AspectListView(m_DetailsTabs);

	m_DetailsTabs->AddPage(m_DetailsView, "Details", true);
	m_DetailsTabs->AddPage(m_AspectGrid, "Aspect Grid");
	m_DetailsTabs->AddPage(m_AspectList, "Aspect List");

	// CChartView used SetSplitterPanes + SetSplitterPosPct(50); sash gravity
	// keeps that 50/50 split as the window resizes, which the WTL splitter
	// did not do on its own.
	m_Splitter->SplitVertically(m_ChartDrawing, m_DetailsTabs);
	m_Splitter->SetSashGravity(0.5);

	auto sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(m_Splitter, wxSizerFlags(1).Expand());
	SetSizer(sizer);

	//
	// Centre the sash once, on the first real size.
	//
	// Two things matter here. It has to be deferred with CallAfter: this
	// handler runs before the sizer lays the splitter out, so reading a width
	// now gives a stale value that sash gravity then scales - which is what
	// pushed the sash hard right on the first attempt. And it has to happen
	// exactly once, otherwise every resize would undo the user's drag.
	// Gravity keeps the 50/50 ratio from then on.
	//
	Bind(wxEVT_SIZE, [this](wxSizeEvent& e) {
		e.Skip();
		if (m_SashCentred)
			return;
		m_SashCentred = true;
		CallAfter([this] {
			if (auto width = m_Splitter->GetClientSize().x; width > 0)
				m_Splitter->SetSashPosition(width / 2);
			});
		});
}

void ChartView::SetChart(ChartData data) {
	AstroCalculator calc;
	calc.Calculate(data);
	m_Data = std::move(data);

	m_ChartDrawing->SetChartData(&m_Data);
	m_AspectGrid->SetChartData(&m_Data);
	m_DetailsView->SetChartData(&m_Data);

	RefreshFromData();
}

void ChartView::ChartForNow() {
	ChartData data;

	auto& info = data.Info();
	info = m_Frame->DefaultChartInfo();
	info.Time = DateTime::Now();

	data.AddPlanets(AstroHelpers::GetStandardPlanets());
	data.AddPlanets({ Planet::Chiron, Planet::TrueNode, Planet::Lilith });

	SetChart(std::move(data));
}

ChartData const& ChartView::Chart() const {
	return m_Data;
}

void ChartView::Recalculate(Recalc what) {
	switch (what) {
		case Recalc::Houses:
			m_Data.CalcHouses(m_Calc);
			break;
		case Recalc::Planets:
			m_Data.CalcPlanets(m_Calc);
			break;
		case Recalc::All:
			m_Data.CalcPlanets(m_Calc);
			m_Data.CalcHouses(m_Calc);
			break;
	}
	RefreshFromData();

	// CChartDetailsView called UpdateControls() itself after posting WM_RECALC;
	// here the frame that owns both does it, so the details view does not have
	// to know it triggered the recalculation. Its own m_Updating guard stops
	// the write-back from raising further change events.
	m_DetailsView->UpdateControls(what);
}

void ChartView::RefreshFromData() {
	AspectCalculator ac;
	auto aspects = ac.Calculate(m_Data.AllPlanets());

	m_AspectGrid->SetAspects(aspects);
	m_AspectGrid->Refresh();
	m_AspectList->SetAspects(aspects);

	m_ChartDrawing->SetAspects(std::move(aspects));
	m_ChartDrawing->Refresh();
}

void ChartView::PageActivated(bool active) {
}
