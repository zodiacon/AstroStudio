#pragma once

#include "Aspects.h"
#include <wx/listctrl.h>

//
// Port of CAspectListView (AstroStudio\AspectListView.h).
//
// The WTL version custom-painted every cell through NM_CUSTOMDRAW. Two of the
// comments there describe problems that simply do not exist in wx and are not
// carried over:
//
//   * "every column is fully custom-painted ... so behavior - in particular,
//     whether hovering highlights a row - is consistent across all of them"
//   * "cd->uItemState isn't reliable for LVS_OWNERDATA lists"
//
// wx asks for text, colour, font and image per cell, so there is no painting
// to keep consistent and no item-state to distrust.
//
// The one genuinely hard cell was the glyph+name pair (DrawGlyphAndName): a
// planet glyph in HamburgSymbols followed by its name in the UI font, which no
// single font can render. Here the glyph is a pre-rendered image supplied by
// OnGetItemColumnImage and the name is ordinary cell text, so the control draws
// the cell itself.
//
class AspectListView : public wxListCtrl {
public:
	explicit AspectListView(wxWindow* parent);

	// Adopts, sorts and displays the aspects.
	void SetAspects(std::vector<AspectData> aspects);

protected:
	wxString OnGetItemText(long item, long column) const override;
	wxItemAttr* OnGetItemColumnAttr(long item, long column) const override;
	int OnGetItemColumnImage(long item, long column) const override;

private:
	void BuildGlyphImages();
	void SortAspects();
	void OnColumnClick(wxListEvent& e);
	void OnSysColourChanged(wxSysColourChangedEvent& e);

	std::vector<AspectData> m_Aspects;
	wxFont m_GlyphFont;

	// Image indices into the small image list: planets occupy the first
	// Planet::NumPlanets slots, aspect types follow.
	int PlanetImage(Planet p) const;
	int AspectImage(AspectType t) const;

	int m_SortColumn{ wxNOT_FOUND };
	bool m_SortAscending{ true };

	mutable wxItemAttr m_Attr;	// returned by pointer, so it has to outlive the call
};
