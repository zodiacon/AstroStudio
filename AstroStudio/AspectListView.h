#pragma once

#include <FrameView.h>
#include <VirtualListView.h>
#include "Aspects.h"
#include "Interfaces.h"
#include "Helpers.h"

class CAspectListView :
	public CFrameView<CAspectListView, IMainFrame>,
	public CVirtualListView<CAspectListView>,
	public CCustomDraw<CAspectListView> {
public:
	explicit CAspectListView(IMainFrame* frame);

	void SetAspects(std::vector<AspectData> aspects) noexcept;
	// With an overlay (transits...) the list is the aspects between its planets and the chart's - as on the wheel and in the grid
	// the chart's own aren't shown then - with the overlay's label before the planets that belong to it ("Transit Sun") and the
	// chart's label ("natal") before the others. Refresh puts it on the screen.
	void SetOverlayAspects(std::vector<AspectData> aspects, std::wstring label, std::wstring baseLabel);
	// no overlay: the chart's own aspects again
	void ClearOverlayAspects();
	void Refresh();
	// puts the text font the user chose with Options > Font on the list (nothing if they have not chosen one)
	void ApplyTextFont();

	// the colour for a longitude by its element (fire, earth, air, water), for the cells that show one; the midpoint list uses it too
	static COLORREF GetElementColor(ZodiacSign sign);

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

	BEGIN_MSG_MAP(CAspectListView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CCustomDraw)
		CHAIN_MSG_MAP(CVirtualListView)
		CHAIN_MSG_MAP(BaseFrame)
	END_MSG_MAP()

private:
	enum class ColumnType {
		Planet1, Planet2, Aspect, Orb, Applying, Planet1Pos, Planet2Pos,
	};

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	void GetCellColors(LPNMCUSTOMDRAW cd, COLORREF backColorOverride, COLORREF& backColor, COLORREF& textColor) const;
	void DrawGlyphAndName(LPNMCUSTOMDRAW cd, PCWSTR glyph, PCWSTR name) const;
	void DrawCell(LPNMCUSTOMDRAW cd, PCWSTR text, HFONT font, COLORREF backColorOverride = CLR_INVALID) const;

	CListViewCtrl m_List;
	CFont m_Font, m_TextFont;
	// a row of the list: a chart's aspect or an overlay's
	struct Row {
		AspectData Data;
		bool Overlay;
	};
	std::vector<AspectData> m_Natal, m_OverlayAspects;
	std::wstring m_OverlayLabel, m_BaseLabel;
	bool m_HasOverlay{ false };
	std::vector<Row> m_Rows;
	// the name of a planet of a row, with the label of the chart or the overlay it is in when the row is an overlay's
	CString PlanetLabel(Row const& row, bool first) const;
};
