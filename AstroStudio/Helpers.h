#pragma once

#include "Calendar.h"

#include "AstroPoint.h"
#include "DefaultFont.h"
#include "ChartData.h"
#include <functional>
class DateTime;
enum class AspectType;

enum class FormatOptions {
	None = 0,
	ShowSeconds = 1,
	UseGlyphs = 2,
	ShowDegreeGlyph = 4,
	Default = ShowDegreeGlyph | UseGlyphs,
};
DEFINE_ENUM_FLAG_OPERATORS(FormatOptions);

enum class DateTimeFormatOptions {
	None = 0,
	DateOnly = 1,
	TimeOnly = 2,
	Default = DateOnly,
};
DEFINE_ENUM_FLAG_OPERATORS(DateTimeFormatOptions);

struct Helpers abstract final {
	static bool LoadAstroFont(UINT id);
	static CString FormatDateTime(DateTime const& dt, DateTimeFormatOptions options = DateTimeFormatOptions::Default);
	static CString FormatLongitude(AstroPoint const& longitude, FormatOptions options = FormatOptions::Default, AstroFontBase const& font = DefaultFont::Get());
	static CString FormatLatitude(double lat);
	static std::tuple<int, int, int> GetDegMinSec(double angle, bool sign = false);
	static PCWSTR GetPlanetName(Planet type);
	// "Tropical" or "Sidereal (Lahiri)" for a zodiac (AstroCalculator's numbers)
	static CString ZodiacText(int zodiac);
	static PCWSTR GetAspectName(AspectType type);
	static CString GetZodiacSignName(ZodiacSign sign);
	static std::vector<Planet> GetStandardPlanets();
	// a chart for the info with the usual planet set (the standard planets plus Chiron, True Node and Lilith)
	// every house system the app offers, in the order of the drop-down lists
	static std::vector<HouseSystem> const& HouseSystems();
	// fills a house system drop-down list; the item data of each entry is the HouseSystem
	static void FillHouseSystems(CComboBox combo);
	static ChartData CreateChartData(ChartInfo info, HouseSystem houseSystem = HouseSystem::Koch);
	// A table for Copy and Export: the headers and, for a row and a column, the cell as plain words (no glyphs) - what the lists
	// show, in the order they are declared.
	struct TableSource {
		std::vector<CString> Headers;
		int Rows{ 0 };
		std::function<CString(int row, int column)> Cell;
	};
	// the headers and the given rows (all of them if none are given), as tab-separated text or as CSV with quoted cells
	static CString TableText(TableSource const& table, std::vector<int> const& rows, bool csv);
	// puts the selected rows of a list view (whose row numbers are the table's) on the clipboard as tab-separated text; false, having
	// done nothing, if no row is selected
	static bool CopyListRows(HWND owner, HWND list, TableSource const& table);
	// writes all the rows as a CSV file for Excel (UTF-8 with a byte order mark); says so if it can't
	static bool SaveTable(HWND owner, TableSource const& table, PCWSTR path, PCWSTR what);
	// puts text on the clipboard; false if the clipboard could not be opened
	static bool CopyTextToClipboard(HWND owner, PCWSTR text);
	// Writes text as UTF-8 with a byte order mark (which is how Excel knows it is UTF-8: the degree signs need it). On failure says
	// so in a message box - "<what> could not be saved to <path>: <reason>" - and returns false.
	static bool SaveTextFileUtf8(HWND owner, PCWSTR path, CString const& text, PCWSTR what);
	// Writes the bytes as they are, with the same message on failure.
	static bool SaveBytes(HWND owner, PCWSTR path, std::string const& bytes, PCWSTR what);
	// Writes the events as an iCalendar file (.ics: ICalendar::ToText; no byte order mark, which calendar programs don't want).
	static bool SaveCalendar(HWND owner, PCWSTR path, std::wstring const& name, std::vector<CalendarEvent> const& events, PCWSTR what);
	// whether a file chosen in an Export dialog is to be an iCalendar file: by its extension, or else by the type chosen (filterIndex)
	static bool IsCalendarFile(PCWSTR path, DWORD filterIndex, DWORD calendarIndex);
	// The text font the user chose with Options > Font (AppSettings::TextFont), as a font of a size in tenths of a point (0: the
	// size they chose, or 9 points if that wasn't given). False, with nothing made, if they have not chosen one - the views then
	// keep the fonts they have.
	static bool UserTextFont(CFont& font, UINT dpi, int deciPoints = 0, int* size = nullptr);
	//
	// fonts for a DPI (the DPI of the window they are for, DpiHelper::GetDpi): CFont::CreatePointFont measures points with the
	// screen's DC, which is the system DPI, not the DPI of the monitor a window is on. The font is deleted first if it exists.
	//
	// deciPoints: tenths of a point
	static bool PointFont(CFont& font, int deciPoints, PCWSTR face, UINT dpi, int weight = FW_NORMAL);
	// lf.lfHeight in tenths of a point, like CreatePointFontIndirect
	static bool PointFontIndirect(CFont& font, LOGFONT lf, UINT dpi);
	// the font of the controls in tool bars (DEFAULT_GUI_FONT, which is made for the system DPI)
	static bool GuiFont(CFont& font, UINT dpi);
	// the message box font (the system's text font)
	static bool MessageFont(CFont& font, UINT dpi);
	// a band of a rebar sized again to its child (a tool bar whose buttons changed size, a strip of another height)
	static void FitReBarBand(HWND rebar, HWND child, bool fullWidth = false);
	// sets a column's width even if it is a fixed width one (which the control otherwise leaves as it is)
	static void SetColumnWidth(CListViewCtrl& list, int column, int width);
	static COLORREF Darken(COLORREF color, int offset);
	static COLORREF Lighten(COLORREF color, int offset);
};

