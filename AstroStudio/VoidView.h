#pragma once

#include <VirtualListView.h>
#include <FrameView.h>
#include <DialogHelper.h>
#include "AstroCalculator.h"
#include "DateTime.h"
#include "DateBoxes.h"
#include "Interfaces.h"
#include "Helpers.h"
#include "WTLHelper.h"
#include "resource.h"

// The "Void of Course Range" dialog: the first and last day (local calendar dates, both included) of the void of course list.
class CVoidRangeDlg :
	public CDialogImpl<CVoidRangeDlg>,
	public CDialogHelper<CVoidRangeDlg> {
public:
	enum { IDD = IDD_VOIDRANGE };

	// the days shown at first (Year/Month/Day are read, as local dates)
	void SetRange(DateTime const& from, DateTime const& to);
	// valid after DoModal returned IDOK
	DateTime const& From() const {
		return m_From;
	}
	DateTime const& To() const {
		return m_To;
	}

	// a week ago to three months ahead, as local dates
	static void DefaultRange(DateTime& from, DateTime& to);

	BEGIN_MSG_MAP(CVoidRangeDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_VOID_DEFAULT, OnDefault)
		COMMAND_HANDLER(IDC_AN_FROM_MONTH, CBN_SELCHANGE, OnMonthOrYearChanged)
		COMMAND_HANDLER(IDC_AN_FROM_YEAR, EN_KILLFOCUS, OnMonthOrYearChanged)
		COMMAND_HANDLER(IDC_AN_TO_MONTH, CBN_SELCHANGE, OnMonthOrYearChanged)
		COMMAND_HANDLER(IDC_AN_TO_YEAR, EN_KILLFOCUS, OnMonthOrYearChanged)
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnOK(WORD, WORD, HWND, BOOL&);
	LRESULT OnCancel(WORD, WORD, HWND, BOOL&);
	LRESULT OnDefault(WORD, WORD, HWND, BOOL&);
	LRESULT OnMonthOrYearChanged(WORD, WORD, HWND, BOOL&);

	CDateBoxes m_FromBoxes, m_ToBoxes;
	DateTime m_From, m_To;
};

// A tab listing the Moon's void of course periods over a range of dates: when each starts (with the last aspect the Moon makes in
// its sign) and ends (when it enters the next sign), in this machine's local time.
class CVoidView :
	public CFrameView<CVoidView, IMainFrame>,
	public IView,
	public CCustomDraw<CVoidView>,
	public CVirtualListView<CVoidView> {
public:
	using CFrameView::CFrameView;

	CString GetColumnText(HWND, int row, int col);
	bool IsSortable(HWND, int col) const {
		return false;
	}

	DWORD OnPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnItemPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd);

	void PageActivated(bool active) override;
	void TextFontChanged() override;
	bool GetStatusInfo(StatusInfo& info) const override;

	BEGIN_MSG_MAP(CVoidView)
		COMMAND_ID_HANDLER(ID_VIEW_GLYPHS, OnViewGlyphs)
		COMMAND_ID_HANDLER(ID_VOID_RANGE, OnRange)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WTLHelper::ThemeChangedMessage, OnThemeChanged)
		CHAIN_MSG_MAP(CCustomDraw<CVoidView>)
		CHAIN_MSG_MAP(CVirtualListView<CVoidView>)
		CHAIN_MSG_MAP(BaseFrame)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_FILE_EXPORT, OnExport)
		COMMAND_ID_HANDLER(ID_FILE_PRINT, OnPrint)
		COMMAND_ID_HANDLER(ID_FILE_PRINT_PREVIEW, OnPrint)
	END_MSG_MAP()

private:
	enum class ColumnType { Start, LastAspect, Sign, End, Enters, Duration };

	LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnViewGlyphs(WORD, WORD, HWND, BOOL&);
	LRESULT OnRange(WORD, WORD, HWND, BOOL&);
	LRESULT OnEditCopy(WORD, WORD, HWND, BOOL&);
	LRESULT OnExport(WORD, WORD, HWND, BOOL&);
	LRESULT OnPrint(WORD, WORD, HWND, BOOL&);

	// works the periods out for the range and shows them
	void Calculate();
	// fontChanged: Options > Font was just used, and its size wins (the ephemeris takes it too)
	void CreateFonts(bool fontChanged = false);
	void UpdateViewUI();
	void AutoSizeColumns();
	bool Glyphs() const {
		return m_Glyphs;
	}
	// a cell in words (the clipboard, files and print) or as the list shows it
	CString CellText(int row, ColumnType type, bool glyphs) const;
	// the whole list as a table in words
	Helpers::TableSource Table() const;
	CString RangeText() const;

	static constexpr UINT_PTR NowTimerId = 1;
	static constexpr UINT NowIntervalMs = 60 * 1000;		// the row of the void going on now is marked

	CListViewCtrl m_List;
	AstroCalculator m_Calc;
	DateTime m_From, m_To;		// the first and last day of the range (local dates)
	std::vector<VoidOfCourseData> m_Periods;
	CFont m_Font, m_StdFont;
	bool m_Glyphs{ true };
};
