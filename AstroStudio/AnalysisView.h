#pragma once

#include <VirtualListView.h>
#include <FrameView.h>
#include "resource.h"
#include "Interfaces.h"
#include "Analysis.h"
#include "WTLHelper.h"
#include <atomic>
#include <memory>
#include <thread>

// A tab with the events an analysis found in a range of dates (see Analysis in AstroCore) in a list, in time order until a
// column is clicked. The Options button opens the analysis dialog again with what the tab was made from, and running it there
// updates this tab; Refresh runs it again as it is, reading the chart afresh if it is still open.
//
// An analysis runs on a worker thread, with its percentage in the status bar and a Cancel button; until it is done (or if it is
// cancelled) the tab goes on showing what it had, and the chart and settings it was made from change only when the new events
// come in.
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
	void TextFontChanged() override;
	// the aspect settings (orbs) changed: what the tab shows was found with the old ones
	void AspectSettingsChanged() override;

	DWORD OnPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnItemPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd);

	BEGIN_MSG_MAP(CAnalysisView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		COMMAND_ID_HANDLER(ID_ANALYSIS_OPTIONS, OnOptions)
		COMMAND_ID_HANDLER(ID_ANALYSIS_REFRESH, OnRefresh)
		COMMAND_ID_HANDLER(ID_ANALYSIS_CANCEL, OnCancel)
		COMMAND_HANDLER(IDC_AN_FILTER, EN_CHANGE, OnFilterChanged)
		NOTIFY_CODE_HANDLER(NM_DBLCLK, OnDoubleClick)
		COMMAND_ID_HANDLER(ID_VIEW_GLYPHS, OnViewGlyphs)
		COMMAND_ID_HANDLER(ID_FONT_BIGGER, OnChangeFontSize)
		COMMAND_ID_HANDLER(ID_FONT_SMALLER, OnChangeFontSize)
		COMMAND_ID_HANDLER(ID_FONT_SIZE_DEFAULT, OnChangeFontSize)
		MESSAGE_HANDLER(WM_ANALYSIS_DONE, OnDone)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WTLHelper::ThemeChangedMessage, OnThemeChanged)
		CHAIN_MSG_MAP(CCustomDraw)
		CHAIN_MSG_MAP(CVirtualListView)
		CHAIN_MSG_MAP(BaseFrame)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_FILE_EXPORT, OnExport)
		COMMAND_ID_HANDLER(ID_FILE_PRINT, OnPrint)
		COMMAND_ID_HANDLER(ID_FILE_PRINT_PREVIEW, OnPrint)
	END_MSG_MAP()

private:
	enum class ColumnType {
		Date, Analysis, Event, Mover, Aspect, Target, Orb, Pass, Longitude, Stay,
	};

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOptions(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewGlyphs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFilterChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// a double click on an event opens its chart (a copy, if it was closed) with the moment around it
	LRESULT OnDoubleClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnChangeFontSize(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDone(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnExport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPrint(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	// Starts the analysis of a chart (a copy) by settings on a worker thread, stopping the one that was running if there was one.
	// The tab takes the chart, the settings and the events when it is done.
	void Start(OpenChart chart, AnalysisSettings settings);
	// stops the run in progress, if any, and waits for its thread
	void StopWorker();
	// The events the filter lets through, as indexes into m_Events: a text of words that must all be in a row's text (in words,
	// whatever the list shows: "square saturn", "2027/03", "enters house 5"). Called when the events, their order or the filter change.
	void ApplyFilter();
	bool Matches(AnalysisEvent const& event) const;
	// the event a row of the list shows
	AnalysisEvent const* EventAt(int row) const;
	// "12345 events", "36 of 12345 events", or that the aspect settings have changed since
	void ShowCount();
	// the status bar, the Cancel button and the timer that reads the worker's progress, for a run that is on or not
	void ShowRunning(bool running);
	void SetStatus(PCWSTR text);
	// the text size, applied to the list and the glyph font
	void ApplyFontSize(int oldSize);
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
	// resetWidths: also put the Mover, Aspect and Target columns to the width that suits glyphs or words (otherwise a glyph column
	// is only widened, when a longer header needs it, so that what the user dragged is kept)
	void UpdateHeaders(bool resetWidths);
	void UpdateViewUI();
	CString EventText(AnalysisEvent const& event) const;
	// a moment as the chart's own time is shown: in its zone
	CString LocalText(DateTime const& ut, bool withTime) const;
	// for the row that ends a stay within an orb: when it entered, was exact and left
	CString StayText(AnalysisEvent const& event) const;
	// the events (the given rows of the list) as a table with a header line: tab separated, or CSV for a file
	CString BuildTable(std::vector<int> const& rows, bool csv) const;

	// what a worker thread works on and reports through; shared with it, so that it outlives a tab that is closed meanwhile
	struct Job {
		OpenChart Chart;
		AnalysisSettings Settings;
		std::atomic<bool> Cancel{ false };
		std::atomic<int> Percent{ 0 };
		AnalysisResult Result;
	};
	static constexpr UINT WM_ANALYSIS_DONE = WM_APP + 20;
	static constexpr UINT_PTR ProgressTimer = 1, FilterTimer = 2;

	CListViewCtrl m_List;
	CFont m_StdFont, m_SymbolFont;
	int m_FontSize{ 90 };			// tenths of a point
	bool m_Glyphs{ true };
	bool m_PageActive{ false };
	CEdit m_Filter;
	std::vector<CString> m_Terms;		// the filter's words, in lower case
	std::vector<int> m_Shown;			// what the list shows: indexes into m_Events
	bool m_Stale{ false };				// the aspect settings changed since the events were found
	std::shared_ptr<Job> m_Job;
	std::thread m_Worker;
	WPARAM m_JobId{ 0 };
	OpenChart m_Chart;
	AnalysisSettings m_Settings;
	std::vector<AnalysisEvent> m_Events;
};
