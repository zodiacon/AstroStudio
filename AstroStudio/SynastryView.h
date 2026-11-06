#pragma once

#include <VirtualListView.h>
#include <FrameView.h>
#include "Synastry.h"
#include "Interfaces.h"
#include "Helpers.h"
#include "WTLHelper.h"
#include "DpiAware.h"
#include "ToolbarStrip.h"
#include "resource.h"

// A tab comparing two charts (Chart > Synastry Report...): the aspects between their planets and of each one's planets to the
// other's Ascendant and Midheaven, tightest first, then where each one's planets fall in the other's houses. The charts are copies,
// kept up to date from their tabs while those are open (edits, Step, Live, the zodiac).
class CSynastryView :
	public CFrameView<CSynastryView, IMainFrame>,
	public CDpiAware<CSynastryView>,
	public IView,
	public CCustomDraw<CSynastryView>,
	public CVirtualListView<CSynastryView> {
public:
	using CFrameView::CFrameView;

	// the two charts, before the window is created
	void SetCharts(OpenChart a, OpenChart b);
	// "Synastry: A & B", the tab's text
	CString Title() const;

	CString GetColumnText(HWND, int row, int col);
	bool IsSortable(HWND, int) const {
		return true;
	}
	void DoSort(SortInfo const* si);
	void OnDpiChanged(UINT dpi);

	DWORD OnPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnItemPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd);

	void PageActivated(bool active) override;
	void TextFontChanged() override;
	void AspectSettingsChanged() override;
	void PartOfFortuneChanged() override;
	void ZodiacChanged() override;
	void SourceChartChanged(IView* chart, bool edit) override;
	void SourceChartClosing(IView* chart) override;
	bool GetStatusInfo(StatusInfo& info) const override;

	BEGIN_MSG_MAP(CSynastryView)
		COMMAND_ID_HANDLER(ID_SYNASTRY_SWAP, OnSwap)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CDpiAware<CSynastryView>)
		MESSAGE_HANDLER(WTLHelper::ThemeChangedMessage, OnThemeChanged)
		MESSAGE_HANDLER(WM_REREAD, OnReread)
		CHAIN_MSG_MAP(CCustomDraw<CSynastryView>)
		CHAIN_MSG_MAP(CVirtualListView<CSynastryView>)
		CHAIN_MSG_MAP(BaseFrame)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_FILE_EXPORT, OnExport)
		COMMAND_ID_HANDLER(ID_FILE_PRINT, OnPrint)
		COMMAND_ID_HANDLER(ID_FILE_PRINT_PREVIEW, OnPrint)
	END_MSG_MAP()

private:
	// posted to itself to read the charts again from their tabs once those have dealt with a change of the zodiac or the Part of Fortune
	static constexpr UINT WM_REREAD = WM_APP + 140;

	// what a row is: an aspect between planets, a planet of A to an angle of B or one of B to an angle of A, a planet of A in a
	// house of B or one of B in a house of A
	enum class RowKind { Aspect, AToAngle, BToAngle, AInB, BInA };
	struct Row {
		RowKind Kind;
		int Index;		// into the report's list of that kind (for a house row: the planet's index in its chart)
	};
	// the first two columns are A's side, the two after the contact B's
	enum class ColumnType { First, FirstPosition, Contact, Second, SecondPosition, Orb };

	LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnReread(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSwap(WORD, WORD, HWND, BOOL&);
	LRESULT OnEditCopy(WORD, WORD, HWND, BOOL&);
	LRESULT OnExport(WORD, WORD, HWND, BOOL&);
	LRESULT OnPrint(WORD, WORD, HWND, BOOL&);

	// works the report out from the two charts as they are and shows it (in the order the list is sorted by, if it is)
	void Rebuild();
	// the charts again from their tabs, if those are still open
	void ReadCharts();
	void CreateFonts();
	void UpdateHeaders();
	void AutoSizeColumns();
	CString CellText(Row const& row, ColumnType column) const;
	// one side of a row: A's (first) or B's - a planet, an angle or a house - with its place and its place in a sort
	struct Side {
		CString Name;
		AstroPoint Longitude;
		double Order{ 0 };
	};
	Side SideOf(Row const& row, bool first) const;
	// the aspect a row is about (None for a house row), and its orb (negative for a house row)
	AspectType AspectOf(Row const& row) const;
	float OrbOf(Row const& row) const;
	Helpers::TableSource Table() const;

	CListViewCtrl m_List;
	OpenChart m_A, m_B;
	SynastryReport m_Report;
	std::vector<Row> m_Rows;
	CFont m_Font;
	ToolbarStrip m_Strip;
	int m_Zodiac{ -1 };		// the zodiac the copies are in
};
