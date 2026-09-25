#pragma once

#include <VirtualListView.h>
#include <FrameView.h>
#include "resource.h"
#include "Interfaces.h"
#include "Analysis.h"
#include "WTLHelper.h"

// A tab with the events an analysis found in a range of dates (see Analysis in AstroCore) in a list, in time order until a
// column is clicked. The Options button opens the analysis dialog again with what the tab was made from, and running it there
// updates this tab; Refresh runs it again as it is, reading the chart afresh if it is still open.
class CAnalysisView :
	public CFrameView<CAnalysisView, IMainFrame>,
	public IView,
	public CCustomDraw<CAnalysisView>,
	public CVirtualListView<CAnalysisView> {
public:
	using CFrameView::CFrameView;

	// Takes a chart (a copy) and the settings, runs the analysis and shows its events. False if the user cancelled a warning.
	void Analyse(OpenChart chart, AnalysisSettings const& settings);

	CString GetColumnText(HWND, int row, int col) const;
	void DoSort(SortInfo const* si);

	void PageActivated(bool active) override;

	DWORD OnPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnItemPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd);

	BEGIN_MSG_MAP(CAnalysisView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		COMMAND_ID_HANDLER(ID_ANALYSIS_OPTIONS, OnOptions)
		COMMAND_ID_HANDLER(ID_ANALYSIS_REFRESH, OnRefresh)
		COMMAND_ID_HANDLER(ID_VIEW_GLYPHS, OnViewGlyphs)
		MESSAGE_HANDLER(WTLHelper::ThemeChangedMessage, OnThemeChanged)
		CHAIN_MSG_MAP(CCustomDraw)
		CHAIN_MSG_MAP(CVirtualListView)
		CHAIN_MSG_MAP(BaseFrame)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_FILE_EXPORT, OnExport)
	END_MSG_MAP()

private:
	enum class ColumnType {
		Date, Analysis, Event, Mover, Aspect, Target, Orb, Pass, Longitude,
	};

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOptions(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewGlyphs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnExport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	// runs the analysis of m_Chart by m_Settings and shows the events
	void Run();
	void UpdateTitle();
	// The text of a cell. With glyphs it holds planet, aspect and sign glyphs (and the position's digits) for the glyph font, and
	// no words; the clipboard and the files always get the words.
	CString CellText(AnalysisEvent const& event, ColumnType column, bool glyphs) const;
	// which columns are drawn in the glyph font when glyphs are on
	static bool IsGlyphColumn(ColumnType column);
	// the row's colour by what happened: green, red and blue aspects, purple house ingresses; CLR_INVALID for the rest
	static COLORREF RowColor(AnalysisEvent const& event);
	void CreateFonts();
	// with glyphs the Mover and Target columns say what they hold in their headers, since the cells have only the glyph
	void UpdateHeaders();
	void UpdateViewUI();
	CString EventText(AnalysisEvent const& event) const;
	// the events (the given rows of the list) as a table with a header line: tab separated, or CSV for a file
	CString BuildTable(std::vector<int> const& rows, bool csv) const;

	CListViewCtrl m_List;
	CFont m_SymbolFont;
	bool m_Glyphs{ true };
	AstroCalculator m_Calc;
	OpenChart m_Chart;
	AnalysisSettings m_Settings;
	std::vector<AnalysisEvent> m_Events;
};
