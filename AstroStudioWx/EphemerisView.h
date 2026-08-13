#pragma once

#include "Interfaces.h"
#include "AstroCalculator.h"
#include "AstroHelpers.h"

#include <wx/listctrl.h>

class wxToolBar;

//
// Port of CEphemerisView (AstroStudio\EphemerisView.h).
//
// Structure differs in one visible way: the WTL view was a CFrameView owning a
// rebar + toolbar plus a client list, and it pushed its toolbar into the
// frame's shared CUpdateUIBase (Frame()->AddToolBarToUI). Here it is a panel
// with its own wxToolBar above the list, and the toolbar's checked/enabled
// state comes from wxEVT_UPDATE_UI bound on the view itself - nothing has to be
// registered with the frame.
//
class EphemerisView : public wxPanel, public IView {
public:
	EphemerisView(wxWindow* parent, IMainFrame* frame);

private:
	//
	// Element / retro / today colours. Kept as its own palette rather than
	// reusing AstroHelpers::ElementColour: the WTL build deliberately gave the
	// ephemeris different, more saturated element colours than the chart views.
	//
	struct ColourOptions {
		wxColour RetroBack;
		wxColour Element[4];
		wxColour TodayTime;
		bool PaintRetro{ true };
		bool PaintSigns{ true };
	};

	class List : public wxListCtrl {
	public:
		explicit List(EphemerisView* owner);

	protected:
		wxString OnGetItemText(long item, long column) const override;
		wxItemAttr* OnGetItemColumnAttr(long item, long column) const override;

	private:
		EphemerisView* m_Owner;
		mutable wxItemAttr m_Attr;
	};

	friend class List;

	struct PlanetData {
		Planet Planet;
		PlanetPosition Position;
	};

	struct RowData {
		DateTime Date;
		std::vector<PlanetData> Planets;
		mutable wxString PhenomGlyph, PhenomText;
		mutable bool PhenomCalculated{ false };
	};

	void BuildToolBar(wxSizer* sizer);
	void CreateFonts();
	void ApplyTheme();
	void AutoSizeColumns();

	// Grows m_Items until row (and row + 1, which GetRowPhenom reads) exist.
	void EnsureRow(long row) const;
	wxString CellText(long row, long column) const;
	wxString GetRowPhenom(long row) const;

	void OnToggleGlyphs(wxCommandEvent& e);
	void OnToggleSeconds(wxCommandEvent& e);
	void OnChangeFontSize(wxCommandEvent& e);
	void OnToggleGridLines(wxCommandEvent& e);
	void OnRightClick(wxListEvent& e);
	void OnNewChart(wxCommandEvent& e);
	void OnUpdateUI(wxUpdateUIEvent& e);

	IMainFrame* m_Frame;
	List* m_List{};
	wxToolBar* m_ToolBar{};

	mutable AstroCalculator m_Calc;
	DateTime m_StartTime;
	double m_Increment{ 1 };
	mutable std::vector<RowData> m_Items;
	std::vector<Planet> m_Planets;

	FormatOptions m_FormatOptions{ FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph };
	ColourOptions m_Colours;
	wxFont m_GlyphFont, m_StdFont;
	int m_FontSize{ 10 };
	bool m_GridLines{ false };
};
