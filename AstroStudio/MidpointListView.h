#pragma once

#include <FrameView.h>
#include <VirtualListView.h>
#include "Midpoints.h"
#include "ChartOverlay.h"
#include "Interfaces.h"
#include "Helpers.h"

// The midpoints of a chart, one row for every pair of its planets and its Ascendant and Midheaven (a tab of the chart view).
class CMidpointListView :
	public CFrameView<CMidpointListView, IMainFrame>,
	public CVirtualListView<CMidpointListView>,
	public CCustomDraw<CMidpointListView> {
public:
	explicit CMidpointListView(IMainFrame* frame);

	// Works the midpoints out for the chart and puts them on the screen; null (or a chart with no planets): none.
	void SetChartData(ChartData const* chart);
	// With an overlay (transits...) the list is the midpoints between its planets and the chart's (the overlay's as the first point of
	// each, with its label: "Transit Sun"), and the points that stand on them are those of both; null: the chart's own midpoints.
	// Takes effect at once; the overlay must outlive its use here.
	void SetOverlay(ChartOverlay const* overlay);
	// puts the text font the user chose with Options > Font on the list (nothing if they have not chosen one)
	void ApplyTextFont();

	// for Copy and Export: the list as a table of plain words, in the order it is shown now, and its window (for the selection)
	Helpers::TableSource Table() const;
	HWND ListWindow() const {
		return m_List;
	}

	CString GetColumnText(HWND, int row, int col) const;
	void DoSort(SortInfo const* si);

	DWORD OnPrePaint(int, LPNMCUSTOMDRAW cd) noexcept;
	DWORD OnItemPrePaint(int, LPNMCUSTOMDRAW cd) noexcept;
	DWORD OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) const noexcept;

	BEGIN_MSG_MAP(CMidpointListView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CCustomDraw)
		CHAIN_MSG_MAP(CVirtualListView)
		CHAIN_MSG_MAP(BaseFrame)
	END_MSG_MAP()

private:
	enum class ColumnType {
		PointA, PointB, Midpoint, Opposite, Arc, House, On,
	};

	// what is on the axis of a midpoint (Midpoints::Contacts, the axis kind), for the list
	struct Row {
		MidpointData Data;
		int House;
		CString On;
	};

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	static CString PointName(ChartPoint const& point);
	// the name, with the overlay's label or the chart's before it when there is an overlay ("Transit Sun", "natal Moon")
	CString PointText(ChartPoint const& point) const;
	// the glyph of a point: a planet's, or Z and X for the Ascendant and Midheaven (as in the analysis view)
	static CString PointGlyph(ChartPoint const& point);
	static int PointOrder(ChartPoint const& point) noexcept;
	void GetCellColors(LPNMCUSTOMDRAW cd, COLORREF backColorOverride, COLORREF& backColor, COLORREF& textColor) const;
	void DrawGlyphAndName(LPNMCUSTOMDRAW cd, PCWSTR glyph, PCWSTR name) const;
	void DrawCell(LPNMCUSTOMDRAW cd, PCWSTR text, HFONT font, COLORREF backColorOverride = CLR_INVALID, bool right = false) const;

	CListViewCtrl m_List;
	CFont m_Font, m_TextFont;
	std::vector<Row> m_Rows;
	ChartData const* m_Chart{ nullptr };
	ChartOverlay const* m_Overlay{ nullptr };
};
