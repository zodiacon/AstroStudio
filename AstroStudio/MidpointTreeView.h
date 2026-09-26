#pragma once

#include "resource.h"
#include "Midpoints.h"
#include "Helpers.h"
#include "ChartOverlay.h"

// The midpoint tree of a chart (a tab of the chart view): a branch for every point that has a midpoint on it - or at a half or whole
// square to it, on the 90 degree dial - in the order of the points' places on that dial, and under it the midpoints, the tightest
// first ("Sun/Moon = 12 Cap 30', 0.42 degrees"). The orb and the kind of contact are chosen at the top of the tab.
class CMidpointTreeView : public CWindowImpl<CMidpointTreeView> {
public:
	DECLARE_WND_CLASS(L"AstroStudioMidpointTree")

	// Works the tree out for the chart (its planets and its Ascendant and Midheaven) and shows it; null (or no planets): none.
	void SetChartData(ChartData const* chart);
	// With an overlay (transits...) a choice appears for whose midpoints the tree is of: the chart's (with the overlay's points on them -
	// transits to the natal midpoints), the overlay's (with the chart's points on them) or those between the two (with both). Null: the
	// chart's own tree. The overlay must outlive its use here.
	void SetOverlay(ChartOverlay const* overlay);
	// puts the text font the user chose with Options > Font on the tree (nothing if they have not chosen one)
	void ApplyTextFont();
	// glyphs or names: the tree is written in astrological symbols (the toolbar's Glyphs button)
	bool Glyphs() const {
		return m_Glyphs;
	}
	// the orb, the kind of contact and the points that take part (MidpointSettings::Current) changed: the two boxes show them, and the tree starts over
	void SyncSettings();

	// for Export and Print: the branches and their midpoints as a flat table of plain words
	Helpers::TableSource Table() const;

	BEGIN_MSG_MAP(CMidpointTreeView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColorStatic)
		COMMAND_HANDLER(IDC_MT_ORB, CBN_SELCHANGE, OnChoice)
		COMMAND_HANDLER(IDC_MT_KIND, CBN_SELCHANGE, OnChoice)
		COMMAND_HANDLER(IDC_MT_PAIRS, CBN_SELCHANGE, OnChoice)
		COMMAND_ID_HANDLER(ID_VIEW_GLYPHS, OnGlyphs)
	END_MSG_MAP()

private:
	enum { IDC_MT_ORB = 7001, IDC_MT_KIND, IDC_MT_TREE, IDC_MT_PAIRS };
	int StripHeight() const {
		return m_Overlay ? 60 : 32;
	}

	LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSetFocus(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnCtlColorStatic(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnChoice(WORD, WORD, HWND, BOOL&);
	LRESULT OnGlyphs(WORD, WORD, HWND, BOOL&);
	// the tree's font: the symbol font with glyphs, otherwise the text font (the user's, or the system's)
	void ApplyTreeFont();
	void Rebuild();
	void Layout();
	ContactOptions Options() const;
	// the size of the dial the tree is read on, in degrees (90, or 45), and the choice of contacts that shows the setting
	int DialSize() const;
	void SelectKind(ContactKind kind);
	static CString PointName(ChartPoint const& point);
	// a point in words ("Transit Sun"), which is what Export and Print have; and as the tree shows it (a glyph, with a prime after the
	// glyph of a point of the overlay, when glyphs are on)
	CString PointWords(ChartPoint const& point) const;
	CString PointText(ChartPoint const& point) const;
	CString Dial(double dial) const;
	CString Position(AstroPoint const& longitude) const;
	// degrees and minutes as the tree shows them (in the glyph font the degree sign is another character)
	CString Degrees(double value, int decimals) const;

	// what a row of the table is: a midpoint standing on a point
	struct Entry {
		CString Point, PointPosition, Dial, Midpoint, MidpointPosition, Angle;
		double Orb;
	};

	ChartData const* m_Data{ nullptr };
	CStatic m_OrbLabel, m_KindLabel, m_PairsLabel;
	CComboBox m_Orb, m_Kind, m_Pairs;
	ChartOverlay const* m_Overlay{ nullptr };
	CTreeViewCtrl m_Tree;
	CFont m_UiFont, m_TextFont, m_SymbolFont;
	CToolBarCtrl m_Toolbar;
	bool m_HasTextFont{ false };
	bool m_Glyphs{ false };
	std::vector<Entry> m_Entries;
};
