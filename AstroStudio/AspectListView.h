#pragma once

#include <FrameView.h>
#include <VirtualListView.h>
#include "Aspects.h"
#include "Interfaces.h"

class CAspectListView :
	public CFrameView<CAspectListView, IMainFrame>,
	public CVirtualListView<CAspectListView>,
	public CCustomDraw<CAspectListView> {
public:
	explicit CAspectListView(IMainFrame* frame);

	void SetAspects(std::vector<AspectData> aspects) noexcept;
	void Refresh();

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
	static COLORREF GetElementColor(ZodiacSign sign);

	CListViewCtrl m_List;
	CFont m_Font;
	std::vector<AspectData> m_Aspects;
};
