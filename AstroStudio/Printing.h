#pragma once

#include "Helpers.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

// Printing: a document that knows how to draw its pages on any device (a printer, or the preview's bitmap), and the parts of the
// program that do the printing - the Print dialog, Page Setup and Print Preview. All measures are in device units, worked out from
// the dots per inch of the device, so a page looks the same wherever it goes.
namespace Printing {
	// where a page is drawn
	struct Page {
		HDC Dc{};
		// where the document draws: the sheet inside its margins, and above the footer
		CRect Area;
		int DpiX{ 96 }, DpiY{ 96 };

		// points (1/72 inch) and inches, vertically and horizontally, as device units
		int Pt(double points) const {
			return static_cast<int>(std::lround(points * DpiY / 72.0));
		}
		int InchX(double inches) const {
			return static_cast<int>(std::lround(inches * DpiX));
		}
	};

	class Document {
	public:
		virtual ~Document() = default;
		// what the spooler and the footer of every page call it
		virtual CString Title() const = 0;
		// a wide document (an ephemeris with many columns) asks for the paper turned round
		virtual bool PreferLandscape() const {
			return false;
		}
		// the number of pages on a page like this one (the size and the resolution decide it)
		virtual int PageCount(Page const& page) = 0;
		// draws page `index` (from 0) in page.Area; the engine adds the footer
		virtual void Draw(Page const& page, int index) = 0;
	};

	// A table laid out for a page: the font is the largest up to a size that lets the columns (and, if given, the rows) fit.
	class TableLayout {
	public:
		// measures every cell at the base size and picks the font; `rows` is the number of rows to fit in `height` (0: any number)
		void Measure(Page const& page, Helpers::TableSource const& table, int width, int height = 0, double basePoints = 9, double minPoints = 5.5);
		int RowHeight() const {
			return m_RowHeight;
		}
		int Width() const {
			return m_Width;
		}
		int RowsIn(int height) const {
			return m_RowHeight > 0 ? std::max(0, height / m_RowHeight - 1) : 0;		// (one row for the headers)
		}
		// the row of headers, then the rows from `first` for `count` of them, from the top left corner at (x, y); returns the y below
		int Draw(Page const& page, Helpers::TableSource const& table, int x, int y, int first, int count);

	private:
		CFont m_Font, m_Bold;
		std::vector<int> m_Widths;
		int m_RowHeight{ 0 };
		int m_Width{ 0 };
		int m_Pad{ 0 };
	};

	// A list as pages of table, with the headers on each; the title and subtitle come first on the first page.
	class TableDocument : public Document {
	public:
		TableDocument(CString title, CString subtitle, Helpers::TableSource table, bool landscape = false);
		CString Title() const override {
			return m_Title;
		}
		bool PreferLandscape() const override {
			return m_Landscape;
		}
		int PageCount(Page const& page) override;
		void Draw(Page const& page, int index) override;

	private:
		CString m_Title, m_Subtitle;
		Helpers::TableSource m_Table;
		bool m_Landscape;
		TableLayout m_Layout;
		int m_FirstRows{ 0 }, m_OtherRows{ 0 };
		int m_TitleHeight{ 0 };
	};

	// what a chart prints: the wheel with the details of the chart (a title, some lines of text, the planets and the houses) on the
	// first page, and after it, if there is one, a list as pages of table
	struct ChartSheet {
		CString Title;
		std::vector<CString> Lines;
		std::vector<BYTE> Wheel;			// a packed DIB (see ChartImage::ToDib); empty: no picture
		Helpers::TableSource Planets, Houses;
		std::optional<Helpers::TableSource> Extra;
		CString ExtraTitle;
	};
	std::unique_ptr<Document> MakeChartDocument(ChartSheet sheet);

	// a table from rows of cells (which it keeps)
	Helpers::TableSource TableFromRows(std::vector<CString> headers, std::vector<std::vector<CString>> rows);

	// a table from text with tab separated cells and a line of headers first (what BuildTable of the ephemeris and the analysis view make)
	Helpers::TableSource TableFromText(CString const& text);

	// Shows the Print dialog for the document and prints the pages asked for. False if the user cancelled or nothing was printed.
	bool Print(HWND owner, Document& document);
	// shows the pages on the screen, as they would come out of the printer, with a button to print them
	void Preview(HWND owner, Document& document);
	// Page Setup: the paper, its orientation and the margins (which are kept between runs)
	void PageSetup(HWND owner);
}
