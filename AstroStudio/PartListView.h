#pragma once

#include <FrameView.h>
#include <VirtualListView.h>
#include "ArabicParts.h"
#include "Interfaces.h"
#include "Helpers.h"

// The Arabic parts of a chart with the aspects each makes to the chart's planets (a tab of the chart view).
class CPartListView :
	public CFrameView<CPartListView, IMainFrame>,
	public CVirtualListView<CPartListView>,
	public CCustomDraw<CPartListView> {
public:
	explicit CPartListView(IMainFrame* frame);

	// Works the standard parts out for the chart and their aspects to its planets and puts them on the screen; null (or a
	// chart with no planets): none. Which aspects and planets count is by these settings, but the orb is always PartAspectOrb
	// (no planet gets extra).
	static constexpr float PartAspectOrb = 1;
	void SetChartData(ChartData const* chart, AspectSettings const& settings);
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

	BEGIN_MSG_MAP(CPartListView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CCustomDraw)
		CHAIN_MSG_MAP(CVirtualListView)
		CHAIN_MSG_MAP(BaseFrame)
	END_MSG_MAP()

private:
	enum class ColumnType {
		Part, Position, House, Formula, Aspects,
	};

	struct Row {
		PartData Data;
		int Order;			// the place in the list of parts, for the default order
		std::vector<PartAspect> Aspects;
		CString AspectText;	// "Square Sun 0.12°, Trine Moon 0.80°" (the cell itself is drawn with glyphs)
	};

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	void GetCellColors(LPNMCUSTOMDRAW cd, COLORREF backColorOverride, COLORREF& backColor, COLORREF& textColor) const;
	// fills the cell with its background and gives its rectangle and text colour
	void FillCell(LPNMCUSTOMDRAW cd, COLORREF backColorOverride, CRect& rc, COLORREF& textColor) const;
	// the aspects of a part as glyphs (the aspect and the planet, in the glyph font) each followed by its orb
	void DrawAspects(LPNMCUSTOMDRAW cd, std::vector<PartAspect> const& aspects) const;
	void DrawCell(LPNMCUSTOMDRAW cd, PCWSTR text, HFONT font, COLORREF backColorOverride = CLR_INVALID, bool right = false) const;

	CListViewCtrl m_List;
	CFont m_Font, m_TextFont;
	std::vector<Row> m_Rows;
};
