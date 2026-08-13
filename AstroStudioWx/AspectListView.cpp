#include "pch.h"
#include "AspectListView.h"

#include "AstroFont.h"
#include "AstroHelpers.h"

#include <algorithm>

namespace {
	// Column order, matching CAspectListView::OnCreate.
	enum Column { P1, P1Pos, P2, P2Pos, Aspect, Orb, Applying, ColumnCount };

	// Colour the glyph bitmaps are drawn against and then masked out with.
	// Anything the glyph font will never paint works; magenta is conventional.
	const wxColour MaskColour(255, 0, 255);
}

AspectListView::AspectListView(wxWindow* parent)
	: wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL) {

	AppendColumn("P1", wxLIST_FORMAT_LEFT, FromDIP(100));
	AppendColumn("P1 Pos", wxLIST_FORMAT_LEFT, FromDIP(110));
	AppendColumn("P2", wxLIST_FORMAT_LEFT, FromDIP(100));
	AppendColumn("P2 Pos", wxLIST_FORMAT_LEFT, FromDIP(110));
	AppendColumn("Aspect", wxLIST_FORMAT_LEFT, FromDIP(120));
	AppendColumn("Orb", wxLIST_FORMAT_RIGHT, FromDIP(60));
	AppendColumn("A/S", wxLIST_FORMAT_CENTRE, FromDIP(50));

	BuildGlyphImages();

	Bind(wxEVT_LIST_COL_CLICK, &AspectListView::OnColumnClick, this);
	Bind(wxEVT_SYS_COLOUR_CHANGED, &AspectListView::OnSysColourChanged, this);
}

int AspectListView::PlanetImage(Planet p) const {
	auto index = static_cast<int>(p);
	return index < DefaultFont::PlanetGlyphCount ? index : -1;
}

int AspectListView::AspectImage(AspectType t) const {
	auto index = static_cast<int>(t);
	return index >= 0 && index < DefaultFont::AspectGlyphCount
		? DefaultFont::PlanetGlyphCount + index
		: -1;
}

//
// Renders every planet and aspect glyph into a masked image list.
//
// The glyph colour is baked in, so this is rebuilt when the system colours
// change. It also means a glyph does not invert when its row is selected - the
// one visible compromise of doing it this way rather than owner-drawing.
//
void AspectListView::BuildGlyphImages() {
	AstroHelpers::LoadAstroFont();

	auto size = GetCharHeight();
	if (size <= 0)
		size = FromDIP(16);

	m_GlyphFont = AstroHelpers::GlyphFont(size);
	auto colour = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);

	auto images = new wxImageList(size, size, true);

	auto add = [&](wxString const& glyph) {
		wxBitmap bmp(size, size);
		{
			wxMemoryDC dc(bmp);
			dc.SetBackground(wxBrush(MaskColour));
			dc.Clear();
			dc.SetFont(m_GlyphFont);
			dc.SetTextForeground(colour);
			dc.SetBackgroundMode(wxBRUSHSTYLE_TRANSPARENT);
			auto extent = dc.GetTextExtent(glyph);
			dc.DrawText(glyph, (size - extent.x) / 2, (size - extent.y) / 2);
		}
		images->Add(bmp, MaskColour);
		};

	// Bounded by the glyph tables, not by Planet::NumPlanets - the enum runs to
	// Vesta but HamburgSymbols stops at Chiron.
	for (int i = 0; i < DefaultFont::PlanetGlyphCount; i++)
		add(DefaultFont::Get().GetPlanetGlyphAsString(static_cast<Planet>(i)));

	// AspectType::None is -1 and never reaches a cell, so the table starts at
	// Conjunction and runs to BiNovile.
	for (int i = 0; i < DefaultFont::AspectGlyphCount; i++)
		add(DefaultFont::Get().GetAspectGlyphAsString(static_cast<AspectType>(i)));

	AssignImageList(images, wxIMAGE_LIST_SMALL);
}

void AspectListView::OnSysColourChanged(wxSysColourChangedEvent& e) {
	BuildGlyphImages();
	Refresh();
	e.Skip();
}

void AspectListView::SetAspects(std::vector<AspectData> aspects) {
	m_Aspects = std::move(aspects);
	SortAspects();
	SetItemCount(static_cast<long>(m_Aspects.size()));
	Refresh();
}

void AspectListView::SortAspects() {
	if (m_SortColumn == wxNOT_FOUND)
		return;

	auto column = m_SortColumn;
	auto ascending = m_SortAscending;

	// Replaces CAspectListView::DoSort + SortHelper::Sort. The vector is the
	// model, so sorting it is all a virtual list needs.
	std::ranges::sort(m_Aspects, [column, ascending](auto const& a, auto const& b) {
		auto less = [ascending](auto const& x, auto const& y) {
			return ascending ? x < y : y < x;
			};
		switch (column) {
			case P1:		return less(a.Planet1.Planet, b.Planet1.Planet);
			case P2:		return less(a.Planet2.Planet, b.Planet2.Planet);
			case Aspect:	return less(a.Type, b.Type);
			case Orb:		return less(a.Orb, b.Orb);
			case Applying:	return less(a.Applying, b.Applying);
			case P1Pos:		return less(static_cast<double>(a.Planet1.Longitude),
								static_cast<double>(b.Planet1.Longitude));
			case P2Pos:		return less(static_cast<double>(a.Planet2.Longitude),
								static_cast<double>(b.Planet2.Longitude));
		}
		return false;
		});
}

void AspectListView::OnColumnClick(wxListEvent& e) {
	auto column = e.GetColumn();
	if (column == m_SortColumn)
		m_SortAscending = !m_SortAscending;
	else {
		m_SortColumn = column;
		m_SortAscending = true;
	}

	SortAspects();
	Refresh();
}

wxString AspectListView::OnGetItemText(long item, long column) const {
	if (item >= static_cast<long>(m_Aspects.size()))
		return wxString();

	auto const& aspect = m_Aspects[item];
	constexpr auto positionOptions = FormatOptions::ShowSeconds
		| FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph;

	switch (column) {
		// The glyph for these three arrives as an image; the text is the name.
		case P1:		return AstroHelpers::GetPlanetName(aspect.Planet1.Planet);
		case P2:		return AstroHelpers::GetPlanetName(aspect.Planet2.Planet);
		case Aspect:	return AstroHelpers::GetAspectName(aspect.Type);

		case P1Pos:		return AstroHelpers::FormatLongitude(aspect.Planet1.Longitude, positionOptions);
		case P2Pos:		return AstroHelpers::FormatLongitude(aspect.Planet2.Longitude, positionOptions);
		case Orb:		return wxString::Format("%.2f%s", aspect.Orb, wxString(wxUniChar(0x00B0)));
		case Applying:	return aspect.Applying ? "A" : "S";
	}
	return wxString();
}

int AspectListView::OnGetItemColumnImage(long item, long column) const {
	if (item >= static_cast<long>(m_Aspects.size()))
		return -1;

	auto const& aspect = m_Aspects[item];
	switch (column) {
		case P1:		return PlanetImage(aspect.Planet1.Planet);
		case P2:		return PlanetImage(aspect.Planet2.Planet);
		case Aspect:	return AspectImage(aspect.Type);
	}
	return -1;
}

wxItemAttr* AspectListView::OnGetItemColumnAttr(long item, long column) const {
	if (item >= static_cast<long>(m_Aspects.size()))
		return nullptr;

	m_Attr = wxItemAttr();
	auto const& aspect = m_Aspects[item];

	//
	// The position columns are element-coloured and carry the sign glyph inside
	// the same string as the digits (see FormatLongitude), so one glyph font
	// covers the whole cell - the observation the WTL code made at
	// AspectListView.cpp:78.
	//
	if (column == P1Pos || column == P2Pos) {
		auto const& point = column == P1Pos ? aspect.Planet1.Longitude : aspect.Planet2.Longitude;
		m_Attr.SetBackgroundColour(AstroHelpers::ElementColour(point.Sign()));
		m_Attr.SetTextColour(*wxBLACK);	// element colours are light-ish in both themes
		m_Attr.SetFont(m_GlyphFont);
	}

	return &m_Attr;
}
