// View.h : interface of the CEphemerisView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <VirtualListView.h>
#include "AstroCalculator.h"
#include "AstroPoint.h"
#include "DateTime.h"
#include "Helpers.h"
#include <FrameView.h>

#include "resource.h"
#include "Interfaces.h"
#include "WTLHelper.h"

struct EphemerisSettings;

struct ColorOptions {
	COLORREF RetroBackColor{ RGB(220, 220, 220) };
	COLORREF RetroTextColor{ CLR_INVALID };
	COLORREF ElementBackColor[4] {
		RGB(255, 128, 0),
		RGB(224, 224, 0),
		RGB(0, 255, 128),
		RGB(0, 192, 255),
	};
	bool PaintRetro: 1 { true };
	bool PaintSigns : 1 { true };
};

// A single line of text in a rebar band; the ephemeris view shows the planets' current longitudes in one.
class CPlanetStrip : public CWindowImpl<CPlanetStrip> {
public:
	DECLARE_WND_CLASS(L"AstroPlanetStrip")

	// the label (when the positions were calculated) is in labelFont, the text after it in font
	void SetText(PCWSTR label, HFONT labelFont, PCWSTR text, HFONT font);

	BEGIN_MSG_MAP(CPlanetStrip)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_PRINTCLIENT, OnPaint)
	END_MSG_MAP()

private:
	LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&) { return 1; }

	CString m_Label, m_Text;
	HFONT m_LabelFont{ nullptr }, m_Font{ nullptr };
};

class CEphemerisView : 
	public CFrameView<CEphemerisView, IMainFrame>,
	public IView,
	public CCustomDraw<CEphemerisView>,
	public CVirtualListView<CEphemerisView> {
public:
	using CFrameView::CFrameView;

	CString GetColumnText(HWND, int row, int col);
	bool IsSortable(HWND, int col) const;
	bool OnRightClickList(HWND, int row, int col, POINT const& pt);

	DWORD OnPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnItemPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd);

protected:
	BEGIN_MSG_MAP(CEphemerisView)
		COMMAND_ID_HANDLER(ID_VIEW_GLYPHS, OnViewGlyphs)
		COMMAND_ID_HANDLER(ID_VIEW_SECONDS, OnViewSeconds)
		COMMAND_ID_HANDLER(ID_FONT_BIGGER, OnChangeFontSize)
		COMMAND_ID_HANDLER(ID_FONT_SMALLER, OnChangeFontSize)
		COMMAND_ID_HANDLER(ID_FONT_SIZE_DEFAULT, OnChangeFontSize)
		COMMAND_ID_HANDLER(ID_VIEW_GRIDLINES, OnViewGridLines)
		COMMAND_ID_HANDLER(ID_EPHEMERIS_OPTIONS, OnOptions)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WTLHelper::ThemeChangedMessage, OnThemeChanged)
		CHAIN_MSG_MAP(CCustomDraw)
		CHAIN_MSG_MAP(CVirtualListView)
		CHAIN_MSG_MAP(BaseFrame)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_NEW_CHART, OnNewChart)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_FILE_EXPORT, OnExport)
	END_MSG_MAP()

	// Planet is last: the columns of the bodies follow it, one value each
	enum class ColumnType {
		Time, SiderealTime, Phenom, MoonVoid, Planet
	};

	void UpdateUI(CUpdateUIBase& ui);
	// the menu and shared toolbar show this view's state while it is the page showing
	void PageActivated(bool active) override;
	void TextFontChanged() override;

private:
	void UpdateList();
	// what happens in the row's day (ingresses, stations), with glyphs or in words; the row after it must exist
	CString GetRowPhenom(int row, bool glyphs) const;
	// calculates the rows up to (not including) index count
	void EnsureRows(size_t count);
	// The text of one cell as plain text (no glyphs, no font needed), for the clipboard and files.
	CString PlainCellText(int row, ColumnType type);
	// The rows as a table with a header line: tab separated for the clipboard, or CSV for a file. Columns are in
	// the order they are shown in.
	CString BuildTable(std::vector<int> const& rows, bool csv);
	void CreateFonts();
	void UpdateViewUI();
	void AutoSizeColumns();
	void UpdateNowStrip();
	// the columns for the bodies and the extras that are on
	void RebuildColumns();
	// keeps what the toolbar and the options dialog set for the next run
	void SaveSettings();
	// starts the list over with other settings
	void ApplySettings(EphemerisSettings const& settings);
	// The void of course text of a row (the row after it must exist). Eclipses (which go in the Phenomena column) and voids are worked out
	// for as far as rows have been asked for, a stretch at a time, and kept.
	void CalcExtras(int row) const;
	void EnsureEclipses(double until) const;
	void EnsureVoids(double until) const;

	struct PlanetData {
		Planet Planet;
		PlanetPosition Position;
	};

	struct RowData {
		DateTime Date;
		std::vector<PlanetData> Planets;
		mutable CString PhenomGlyph, PhenomText;
		mutable bool PhenomCalculated{ false };
		mutable bool HasEclipse{ false };		// the Phenomena cell has an eclipse in it (which is then shown in colour)
		mutable CString VoidText, VoidGlyph;
		mutable double VoidFraction{ 0 };		// how much of the row's stretch of time the Moon is void of course (0 to 1)
		mutable bool ExtrasCalculated{ false };
	};
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnViewGlyphs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewSeconds(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnChangeFontSize(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewGridLines(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOptions(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnExport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNewChart(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	static constexpr UINT_PTR NowTimerId = 1;
	static constexpr UINT NowIntervalMs = 30 * 1000;
	static constexpr int NowBand = 1;		// the rebar band of the strip

	CListViewCtrl m_List;
	CPlanetStrip m_NowStrip;
	AstroCalculator m_Calc;
	ChartInfo m_ChartInfo;
	DateTime m_StartTime;
	double m_Increment{ 1 };
	CFont m_Font, m_StdFont;
	FormatOptions m_FormatOptions;
	ColorOptions m_ColorOptions;
	int m_FontSize{ 100 };
	std::vector<RowData> m_Items;
	std::vector<Planet> m_Planets;
	bool m_ShowEclipses{ false }, m_ShowVoid{ false };
	mutable std::vector<EclipseData> m_Eclipses;
	mutable double m_EclipsesUntil{ 0 };
	mutable std::vector<VoidOfCourseData> m_Voids;
	mutable double m_VoidsUntil{ 0 };
};
