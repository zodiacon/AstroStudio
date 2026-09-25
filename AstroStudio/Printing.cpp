#include "pch.h"
#include "Printing.h"
#include "AppSettings.h"
#include <WTLHelper.h>
#include <commdlg.h>
#include <algorithm>
#include <cmath>

namespace Printing {
	namespace {
		// the printer and paper chosen in the last Print or Page Setup dialog (for the run of the program)
		HGLOBAL g_devMode = nullptr, g_devNames = nullptr;

		// thousandths of an inch
		struct Margins {
			int Left{ 750 }, Top{ 750 }, Right{ 750 }, Bottom{ 750 };
		};

		Margins LoadMargins() {
			Margins margins;
			int l, t, r, b;
			auto text = AppSettings::Get().PrintMargins();
			if (swscanf_s(text.c_str(), L"%d,%d,%d,%d", &l, &t, &r, &b) == 4) {
				auto ok = [](int value) { return value >= 0 && value <= 4000; };
				if (ok(l) && ok(t) && ok(r) && ok(b))
					margins = { l, t, r, b };
			}
			return margins;
		}

		void SaveMargins(Margins const& margins) {
			CString text;
			text.Format(L"%d,%d,%d,%d", margins.Left, margins.Top, margins.Right, margins.Bottom);
			AppSettings::Get().PrintMargins(std::wstring((PCWSTR)text));
		}

		// A hook procedure that does nothing: giving the common dialogs one makes Windows show the classic dialog rather than the
		// newer one (which has no way to show a page range or the printer's own settings the way a program of this kind expects).
		UINT_PTR CALLBACK NoHook(HWND, UINT, WPARAM, LPARAM) {
			return 0;
		}

		// (into a font of the caller: a CFont is not safe to copy or return, both copies would delete the same font)
		void MakeFont(CFont& font, Page const& page, double points, bool bold) {
			if (!font.IsNull())
				font.DeleteObject();
			font.CreateFont(-page.Pt(points), 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
				CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
		}

		// a page on a printer: the sheet inside the margins, allowing for the part of the paper the printer can't reach
		Page PrinterPage(HDC dc) {
			auto margins = LoadMargins();
			Page page;
			page.Dc = dc;
			page.DpiX = ::GetDeviceCaps(dc, LOGPIXELSX);
			page.DpiY = ::GetDeviceCaps(dc, LOGPIXELSY);
			int offsetX = ::GetDeviceCaps(dc, PHYSICALOFFSETX), offsetY = ::GetDeviceCaps(dc, PHYSICALOFFSETY);
			int width = ::GetDeviceCaps(dc, HORZRES), height = ::GetDeviceCaps(dc, VERTRES);
			int paperWidth = ::GetDeviceCaps(dc, PHYSICALWIDTH), paperHeight = ::GetDeviceCaps(dc, PHYSICALHEIGHT);
			int left = std::max(0, margins.Left * page.DpiX / 1000 - offsetX);
			int top = std::max(0, margins.Top * page.DpiY / 1000 - offsetY);
			int right = std::max(0, margins.Right * page.DpiX / 1000 - (paperWidth - width - offsetX));
			int bottom = std::max(0, margins.Bottom * page.DpiY / 1000 - (paperHeight - height - offsetY));
			page.Area = CRect(left, top, width - right, height - bottom);
			return page;
		}

		// draws one page: the document, and under it the footer with the title and the page number
		void DrawSheet(Document& document, Page const& sheet, int index, int count) {
			Page content = sheet;
			int footer = sheet.Pt(20);
			content.Area.bottom = std::max<LONG>(content.Area.top, content.Area.bottom - footer);
			int saved = ::SaveDC(sheet.Dc);
			document.Draw(content, index);
			::RestoreDC(sheet.Dc, saved);

			CDCHandle dc(sheet.Dc);
			CFont font; MakeFont(font, sheet, 8, false);
			auto old = dc.SelectFont(font);
			dc.SetBkMode(TRANSPARENT);
			dc.SetTextColor(RGB(90, 90, 90));
			CRect strip(sheet.Area.left, sheet.Area.bottom - footer + sheet.Pt(6), sheet.Area.right, sheet.Area.bottom);
			CString number;
			number.Format(L"Page %d of %d", index + 1, count);
			dc.DrawText(document.Title(), -1, &strip, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
			dc.DrawText(number, -1, &strip, DT_RIGHT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
			CPen pen;
			pen.CreatePen(PS_SOLID, std::max(1, sheet.Pt(0.4)), RGB(170, 170, 170));
			auto oldPen = dc.SelectPen(pen);
			dc.MoveTo(sheet.Area.left, sheet.Area.bottom - footer + sheet.Pt(3));
			dc.LineTo(sheet.Area.right, sheet.Area.bottom - footer + sheet.Pt(3));
			dc.SelectPen(oldPen);
			dc.SelectFont(old);
		}
	}

	//
	// tables
	//

	void TableLayout::Measure(Page const& page, Helpers::TableSource const& table, int width, int height, double basePoints, double minPoints) {
		CDCHandle dc(page.Dc);
		int columns = static_cast<int>(table.Headers.size());
		CFont baseRegular; MakeFont(baseRegular, page, basePoints, false);
		CFont baseBold; MakeFont(baseBold, page, basePoints, true);

		// every cell at the base size
		std::vector<int> widths(columns, 0);
		auto measure = [&](CString const& text, int column) {
			CSize size;
			dc.GetTextExtent(text, text.GetLength(), &size);
			widths[column] = std::max<int>(widths[column], size.cx);
		};
		auto old = dc.SelectFont(baseBold);
		for (int column = 0; column < columns; column++)
			measure(table.Headers[column], column);
		dc.SelectFont(baseRegular);
		for (int row = 0; row < table.Rows; row++)
			for (int column = 0; column < columns; column++)
				measure(table.Cell(row, column), column);
		TEXTMETRIC metrics{};
		dc.GetTextMetrics(&metrics);
		dc.SelectFont(old);

		int pad = page.Pt(basePoints * 0.4);
		int baseRow = metrics.tmHeight + page.Pt(basePoints * 0.3);
		double total = 0;
		for (int column = 0; column < columns; column++)
			total += widths[column] + 2 * pad;

		// the largest font, no more than the base, that lets the columns fit across and the rows down
		double points = basePoints;
		if (total > width && total > 0)
			points = std::min(points, basePoints * width / total);
		if (height > 0 && baseRow > 0 && (table.Rows + 1) * static_cast<double>(baseRow) > height)
			points = std::min(points, basePoints * height / ((table.Rows + 1) * static_cast<double>(baseRow)));
		points = std::max(std::floor(points * 2) / 2, minPoints);

		double scale = points / basePoints;
		MakeFont(m_Font, page, points, false);
		MakeFont(m_Bold, page, points, true);
		m_Pad = static_cast<int>(std::lround(pad * scale));
		m_Widths.assign(columns, 0);
		double used = 0;
		for (int column = 0; column < columns; column++) {
			m_Widths[column] = static_cast<int>(std::ceil((widths[column] + 2 * pad) * scale));
			used += m_Widths[column];
		}
		if (used > width && used > 0) {
			// at the smallest font it still does not fit: the columns give way in proportion (their text ends in dots)
			for (auto& w : m_Widths)
				w = static_cast<int>(w * width / used);
			used = width;
		}
		m_Width = static_cast<int>(used);
		auto oldFont = dc.SelectFont(m_Font);
		dc.GetTextMetrics(&metrics);
		dc.SelectFont(oldFont);
		m_RowHeight = metrics.tmHeight + page.Pt(points * 0.3);
	}

	int TableLayout::Draw(Page const& page, Helpers::TableSource const& table, int x, int y, int first, int count) {
		CDCHandle dc(page.Dc);
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(RGB(0, 0, 0));
		CPen thin, thick;
		thin.CreatePen(PS_SOLID, std::max(1, page.Pt(0.4)), RGB(200, 200, 200));
		thick.CreatePen(PS_SOLID, std::max(1, page.Pt(0.8)), RGB(60, 60, 60));
		auto oldPen = dc.SelectPen(thin);

		auto drawRow = [&](int top, auto&& text, bool bold) {
			dc.SelectFont(bold ? m_Bold : m_Font);
			int left = x;
			for (int column = 0; column < static_cast<int>(m_Widths.size()); column++) {
				CRect rc(left + m_Pad, top, left + m_Widths[column] - m_Pad, top + m_RowHeight);
				if (rc.Width() > 0)
					dc.DrawText(text(column), -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
				left += m_Widths[column];
			}
		};
		auto oldFont = dc.SelectFont(m_Bold);

		drawRow(y, [&](int column) { return table.Headers[column]; }, true);
		y += m_RowHeight;
		dc.SelectPen(thick);
		dc.MoveTo(x, y);
		dc.LineTo(x + m_Width, y);
		dc.SelectPen(thin);
		for (int row = first; row < first + count && row < table.Rows; row++) {
			drawRow(y, [&](int column) { return table.Cell(row, column); }, false);
			y += m_RowHeight;
			dc.MoveTo(x, y);
			dc.LineTo(x + m_Width, y);
		}
		dc.SelectPen(oldPen);
		dc.SelectFont(oldFont);
		return y;
	}

	TableDocument::TableDocument(CString title, CString subtitle, Helpers::TableSource table, bool landscape) :
		m_Title(std::move(title)), m_Subtitle(std::move(subtitle)), m_Table(std::move(table)), m_Landscape(landscape) {
	}

	int TableDocument::PageCount(Page const& page) {
		m_TitleHeight = page.Pt(14 * 1.4) + (m_Subtitle.IsEmpty() ? 0 : page.Pt(9 * 1.5)) + page.Pt(8);
		m_Layout.Measure(page, m_Table, page.Area.Width());
		m_FirstRows = std::max(1, m_Layout.RowsIn(page.Area.Height() - m_TitleHeight));
		m_OtherRows = std::max(1, m_Layout.RowsIn(page.Area.Height()));
		int rest = std::max(0, m_Table.Rows - m_FirstRows);
		return 1 + (rest + m_OtherRows - 1) / m_OtherRows;
	}

	void TableDocument::Draw(Page const& page, int index) {
		CDCHandle dc(page.Dc);
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(RGB(0, 0, 0));
		int y = page.Area.top, first = 0, count = m_FirstRows;
		if (index == 0) {
			CFont bold; MakeFont(bold, page, 14, true);
			auto old = dc.SelectFont(bold);
			CRect rc(page.Area.left, y, page.Area.right, y + page.Pt(14 * 1.4));
			dc.DrawText(m_Title, -1, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
			if (!m_Subtitle.IsEmpty()) {
				CFont smallFont; MakeFont(smallFont, page, 9, false);
				dc.SelectFont(smallFont);
				CRect sub(page.Area.left, y + page.Pt(14 * 1.4), page.Area.right, y + m_TitleHeight);
				dc.DrawText(m_Subtitle, -1, &sub, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
			}
			dc.SelectFont(old);
			y += m_TitleHeight;
		}
		else {
			first = m_FirstRows + (index - 1) * m_OtherRows;
			count = m_OtherRows;
		}
		m_Layout.Draw(page, m_Table, page.Area.left, y, first, count);
	}

	//
	// a chart
	//

	namespace {
		class ChartDocument : public Document {
		public:
			explicit ChartDocument(ChartSheet sheet) : m_Sheet(std::move(sheet)) {
			}
			CString Title() const override {
				return m_Sheet.Title;
			}
			int PageCount(Page const& page) override {
				if (!m_Sheet.Extra)
					return 1;
				m_ExtraTitle = page.Pt(12 * 1.5) + page.Pt(6);
				m_ExtraLayout.Measure(page, *m_Sheet.Extra, page.Area.Width());
				m_ExtraRows = std::max(1, m_ExtraLayout.RowsIn(page.Area.Height() - m_ExtraTitle));
				return 1 + std::max(1, (m_Sheet.Extra->Rows + m_ExtraRows - 1) / m_ExtraRows);
			}
			void Draw(Page const& page, int index) override {
				if (index == 0)
					DrawFirst(page);
				else
					DrawExtra(page, index - 1);
			}

		private:
			void DrawFirst(Page const& page) {
				CDCHandle dc(page.Dc);
				dc.SetBkMode(TRANSPARENT);
				dc.SetTextColor(RGB(0, 0, 0));
				CRect a = page.Area;
				int y = a.top;

				CFont bold; MakeFont(bold, page, 15, true);
				auto old = dc.SelectFont(bold);
				CRect rc(a.left, y, a.right, y + page.Pt(15 * 1.35));
				dc.DrawText(m_Sheet.Title, -1, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
				y += page.Pt(15 * 1.35);
				CFont smallFont; MakeFont(smallFont, page, 9.5, false);
				dc.SelectFont(smallFont);
				for (auto const& line : m_Sheet.Lines) {
					CRect lr(a.left, y, a.right, y + page.Pt(9.5 * 1.45));
					dc.DrawText(line, -1, &lr, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
					y += page.Pt(9.5 * 1.45);
				}
				dc.SelectFont(old);
				y += page.Pt(6);

				// the wheel, and where the two tables go: below it on a tall page, beside it on a wide one
				int gap = page.Pt(12);
				CRect wheel, planets, houses;
				if (a.Width() <= a.Height()) {
					int side = std::min<int>(a.Width(), static_cast<int>((a.bottom - y) * 0.56));
					wheel = CRect(a.left + (a.Width() - side) / 2, y, a.left + (a.Width() - side) / 2 + side, y + side);
					int tableTop = y + side + page.Pt(8);
					int planetsWidth = (a.Width() - gap) * 62 / 100;
					planets = CRect(a.left, tableTop, a.left + planetsWidth, a.bottom);
					houses = CRect(a.left + planetsWidth + gap, tableTop, a.right, a.bottom);
				}
				else {
					int side = std::min<int>(a.bottom - y, a.Width() * 45 / 100);
					wheel = CRect(a.left, y, a.left + side, y + side);
					int left = a.left + side + gap;
					int planetsHeight = (a.bottom - y) * 62 / 100;
					planets = CRect(left, y, a.right, y + planetsHeight);
					houses = CRect(left, y + planetsHeight + gap, a.right, a.bottom);
				}

				if (m_Sheet.Wheel.size() > sizeof(BITMAPINFOHEADER)) {
					auto header = reinterpret_cast<BITMAPINFO const*>(m_Sheet.Wheel.data());
					auto bits = m_Sheet.Wheel.data() + sizeof(BITMAPINFOHEADER);
					dc.SetStretchBltMode(HALFTONE);
					::SetBrushOrgEx(dc, 0, 0, nullptr);
					::StretchDIBits(dc, wheel.left, wheel.top, wheel.Width(), wheel.Height(), 0, 0, header->bmiHeader.biWidth,
						header->bmiHeader.biHeight, bits, header, DIB_RGB_COLORS, SRCCOPY);
				}
				for (auto const& [box, table] : { std::pair{ planets, &m_Sheet.Planets }, std::pair{ houses, &m_Sheet.Houses } }) {
					TableLayout layout;
					layout.Measure(page, *table, box.Width(), box.Height());
					layout.Draw(page, *table, box.left, box.top, 0, table->Rows);
				}
			}

			void DrawExtra(Page const& page, int index) {
				CDCHandle dc(page.Dc);
				dc.SetBkMode(TRANSPARENT);
				dc.SetTextColor(RGB(0, 0, 0));
				CFont bold; MakeFont(bold, page, 12, true);
				auto old = dc.SelectFont(bold);
				CRect rc(page.Area.left, page.Area.top, page.Area.right, page.Area.top + page.Pt(12 * 1.5));
				dc.DrawText(m_Sheet.ExtraTitle, -1, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
				dc.SelectFont(old);
				m_ExtraLayout.Draw(page, *m_Sheet.Extra, page.Area.left, page.Area.top + m_ExtraTitle, index * m_ExtraRows, m_ExtraRows);
			}

			ChartSheet m_Sheet;
			TableLayout m_ExtraLayout;
			int m_ExtraRows{ 1 }, m_ExtraTitle{ 0 };
		};
	}

	std::unique_ptr<Document> MakeChartDocument(ChartSheet sheet) {
		return std::make_unique<ChartDocument>(std::move(sheet));
	}

	Helpers::TableSource TableFromRows(std::vector<CString> headers, std::vector<std::vector<CString>> rows) {
		auto shared = std::make_shared<std::vector<std::vector<CString>>>(std::move(rows));
		Helpers::TableSource table;
		table.Headers = std::move(headers);
		table.Rows = static_cast<int>(shared->size());
		table.Cell = [shared](int row, int column) -> CString {
			auto& cells = (*shared)[row];
			return column < static_cast<int>(cells.size()) ? cells[column] : CString();
		};
		return table;
	}

	Helpers::TableSource TableFromText(CString const& text) {
		auto lines = std::make_shared<std::vector<std::vector<CString>>>();
		int position = 0;
		while (position < text.GetLength()) {
			int end = text.Find(L'\n', position);
			if (end < 0)
				end = text.GetLength();
			CString line = text.Mid(position, end - position);
			line.TrimRight(L"\r");
			position = end + 1;
			auto& cells = lines->emplace_back();
			int start = 0;
			while (true) {
				int tab = line.Find(L'\t', start);
				cells.push_back(line.Mid(start, tab < 0 ? line.GetLength() - start : tab - start));
				if (tab < 0)
					break;
				start = tab + 1;
			}
		}
		Helpers::TableSource table;
		if (lines->empty())
			return table;
		table.Headers = (*lines)[0];
		table.Rows = static_cast<int>(lines->size()) - 1;
		table.Cell = [lines](int row, int column) -> CString {
			auto& cells = (*lines)[static_cast<size_t>(row) + 1];
			return column < static_cast<int>(cells.size()) ? cells[column] : CString();
		};
		return table;
	}

	//
	// the printer
	//

	namespace {
		// the printer to start from, if none has been chosen: the default one
		bool EnsurePrinter(HWND owner) {
			if (g_devMode)
				return true;
			PRINTDLG pd{ sizeof(pd) };
			pd.Flags = PD_RETURNDEFAULT;
			if (!::PrintDlg(&pd)) {
				AtlMessageBox(owner, L"There is no printer to print to. Install a printer, or choose one in the Print dialog of another program.", L"Astro Studio", MB_ICONINFORMATION);
				return false;
			}
			g_devMode = pd.hDevMode;
			g_devNames = pd.hDevNames;
			return true;
		}

		// the document's orientation goes into the printer's settings (the Print dialog can still change it)
		void ApplyOrientation(Document const& document) {
			if (auto mode = static_cast<DEVMODE*>(::GlobalLock(g_devMode))) {
				if ((mode->dmFields & DM_ORIENTATION) != 0) {
					mode->dmOrientation = document.PreferLandscape() ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT;
					mode->dmFields |= DM_ORIENTATION;
				}
				::GlobalUnlock(g_devMode);
			}
		}

		// an information context (the measures of the printer, no output) for the printer chosen
		HDC PrinterInfo() {
			HDC dc = nullptr;
			auto names = static_cast<DEVNAMES*>(::GlobalLock(g_devNames));
			auto mode = static_cast<DEVMODE*>(::GlobalLock(g_devMode));
			if (names) {
				auto base = reinterpret_cast<PCWSTR>(names);
				dc = ::CreateIC(base + names->wDriverOffset, base + names->wDeviceOffset, base + names->wOutputOffset, mode);
			}
			if (mode)
				::GlobalUnlock(g_devMode);
			if (names)
				::GlobalUnlock(g_devNames);
			return dc;
		}
	}

	bool Print(HWND owner, Document& document) {
		if (!EnsurePrinter(owner))
			return false;
		ApplyOrientation(document);

		// how many pages there would be, for the range in the dialog
		int pages = 1;
		if (HDC info = PrinterInfo()) {
			pages = std::max(1, document.PageCount(PrinterPage(info)));
			::DeleteDC(info);
		}

		PRINTDLG pd{ sizeof(pd) };
		pd.hwndOwner = owner;
		pd.hDevMode = g_devMode;
		pd.hDevNames = g_devNames;
		pd.nMinPage = 1;
		pd.nMaxPage = static_cast<WORD>(std::min(pages, 65535));
		pd.nFromPage = 1;
		pd.nToPage = pd.nMaxPage;
		pd.Flags = PD_RETURNDC | PD_NOSELECTION | PD_ALLPAGES | PD_USEDEVMODECOPIESANDCOLLATE | PD_ENABLEPRINTHOOK | (pages < 2 ? PD_NOPAGENUMS : 0);
		pd.lpfnPrintHook = NoHook;
		WTLHelper::SuspendHook();
		bool ok = ::PrintDlg(&pd) != FALSE;
		WTLHelper::ResumeHook();
		g_devMode = pd.hDevMode;
		g_devNames = pd.hDevNames;
		if (!ok)
			return false;

		CWaitCursor wait;
		HDC dc = pd.hDC;
		auto sheet = PrinterPage(dc);
		Page content = sheet;
		content.Area.bottom = std::max<LONG>(content.Area.top, content.Area.bottom - sheet.Pt(20));
		int count = std::max(1, document.PageCount(content));
		int first = 1, last = count;
		if ((pd.Flags & PD_PAGENUMS) != 0) {
			first = std::max<int>(1, pd.nFromPage);
			last = std::min<int>(count, pd.nToPage);
		}
		int copies = (pd.Flags & PD_USEDEVMODECOPIESANDCOLLATE) != 0 ? 1 : std::max<int>(1, pd.nCopies);

		CString title = document.Title();
		DOCINFO info{ sizeof(info) };
		info.lpszDocName = title;
		bool printed = false;
		if (::StartDoc(dc, &info) > 0) {
			printed = true;
			for (int copy = 0; copy < copies && printed; copy++) {
				for (int page = first; page <= last; page++) {
					if (::StartPage(dc) <= 0) {
						printed = false;
						break;
					}
					::SetMapMode(dc, MM_TEXT);
					DrawSheet(document, sheet, page - 1, count);
					if (::EndPage(dc) <= 0) {
						printed = false;
						break;
					}
				}
			}
			if (printed)
				::EndDoc(dc);
			else
				::AbortDoc(dc);
		}
		::DeleteDC(dc);
		if (!printed)
			AtlMessageBox(owner, L"The document could not be printed.", L"Astro Studio", MB_ICONWARNING);
		return printed;
	}

	void PageSetup(HWND owner) {
		auto margins = LoadMargins();
		PAGESETUPDLG psd{ sizeof(psd) };
		psd.hwndOwner = owner;
		psd.hDevMode = g_devMode;
		psd.hDevNames = g_devNames;
		psd.Flags = PSD_MARGINS | PSD_INTHOUSANDTHSOFINCHES | PSD_ENABLEPAGESETUPHOOK;
		psd.lpfnPageSetupHook = NoHook;
		psd.rtMargin = { margins.Left, margins.Top, margins.Right, margins.Bottom };
		WTLHelper::SuspendHook();
		bool ok = ::PageSetupDlg(&psd) != FALSE;
		WTLHelper::ResumeHook();
		g_devMode = psd.hDevMode;
		g_devNames = psd.hDevNames;
		if (ok)
			SaveMargins({ psd.rtMargin.left, psd.rtMargin.top, psd.rtMargin.right, psd.rtMargin.bottom });
	}

	//
	// the preview
	//

	namespace {
		class CPreviewWnd : public CWindowImpl<CPreviewWnd, CWindow, CWinTraits<WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_CLIPCHILDREN, WS_EX_DLGMODALFRAME>> {
		public:
			DECLARE_WND_CLASS(L"AstroStudioPrintPreview")

			Document* Doc{ nullptr };
			double PaperWidth{ 8.27 }, PaperHeight{ 11.69 };		// inches

			BEGIN_MSG_MAP(CPreviewWnd)
				MESSAGE_HANDLER(WM_CREATE, OnCreate)
				MESSAGE_HANDLER(WM_SIZE, OnSize)
				MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
				MESSAGE_HANDLER(WM_PAINT, OnPaint)
				MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
				MESSAGE_HANDLER(WM_GETMINMAXINFO, OnMinMax)
				COMMAND_ID_HANDLER(IDC_PV_PREV, OnPrev)
				COMMAND_ID_HANDLER(IDC_PV_NEXT, OnNext)
				COMMAND_ID_HANDLER(IDC_PV_PRINT, OnPrint)
				COMMAND_ID_HANDLER(IDCANCEL, OnClose)
			END_MSG_MAP()

		private:
			enum { IDC_PV_PREV = 9001, IDC_PV_NEXT, IDC_PV_PRINT };
			static constexpr int BarHeight = 40;

			LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
				const struct { int Id; PCWSTR Text; CButton* Button; } buttons[] = {
					{ IDC_PV_PREV, L"&Previous", &m_Prev }, { IDC_PV_NEXT, L"&Next", &m_Next },
					{ IDC_PV_PRINT, L"P&rint...", &m_Print }, { IDCANCEL, L"&Close", &m_Close },
				};
				// the buttons and the page label in the font the system uses for its own windows (a new button has the old System font)
				NONCLIENTMETRICS metrics{ sizeof(metrics) };
				if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
					m_Font.CreateFontIndirect(&metrics.lfMessageFont);
				for (auto const& b : buttons) {
					b.Button->Create(m_hWnd, rcDefault, b.Text, WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, b.Id);
					if (!m_Font.IsNull())
						b.Button->SetFont(m_Font);
				}
				// (the page count uses a fixed resolution so it doesn't change as the window is resized)
				Page reference = MakePage(nullptr, 150);
				HDC screen = ::GetDC(nullptr);
				reference.Dc = screen;
				m_Count = std::max(1, Doc->PageCount(ContentOf(reference)));
				::ReleaseDC(nullptr, screen);
				return 0;
			}

			LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&) {
				CRect rc;
				GetClientRect(&rc);
				int x = 8;
				for (auto* button : { &m_Prev, &m_Next, &m_Print }) {
					button->MoveWindow(x, 6, 90, 28);
					x += 96;
				}
				m_Close.MoveWindow(rc.right - 98, 6, 90, 28);
				if (!m_Cache.IsNull())
					m_Cache.DeleteObject();
				Invalidate();
				return 0;
			}

			LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&) {
				return 1;
			}

			LRESULT OnMinMax(UINT, WPARAM, LPARAM lParam, BOOL&) {
				reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = { 480, 360 };
				return 0;
			}

			// the sheet at `dpi` dots per inch, with the margins from the settings
			Page MakePage(HDC dc, int dpi) const {
				auto margins = LoadMargins();
				Page page;
				page.Dc = dc;
				page.DpiX = page.DpiY = dpi;
				page.Area = CRect(margins.Left * dpi / 1000, margins.Top * dpi / 1000,
					static_cast<int>(PaperWidth * dpi) - margins.Right * dpi / 1000, static_cast<int>(PaperHeight * dpi) - margins.Bottom * dpi / 1000);
				return page;
			}
			static Page ContentOf(Page page) {
				page.Area.bottom = std::max<LONG>(page.Area.top, page.Area.bottom - page.Pt(20));
				return page;
			}

			LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&) {
				CPaintDC dc(m_hWnd);
				CRect rc;
				GetClientRect(&rc);
				dc.FillSolidRect(&rc, ::GetSysColor(COLOR_APPWORKSPACE));
				CRect bar(0, 0, rc.right, BarHeight);
				dc.FillSolidRect(&bar, ::GetSysColor(COLOR_BTNFACE));

				// the page as large as the window allows
				int availableWidth = rc.Width() - 40, availableHeight = rc.Height() - BarHeight - 40;
				int dpi = static_cast<int>(std::min(availableWidth / PaperWidth, availableHeight / PaperHeight));
				if (dpi < 20)
					return 0;
				int width = static_cast<int>(PaperWidth * dpi), height = static_cast<int>(PaperHeight * dpi);
				CRect paper((rc.Width() - width) / 2, BarHeight + 20 + (availableHeight - height) / 2, 0, 0);
				paper.right = paper.left + width;
				paper.bottom = paper.top + height;

				if (m_Cache.IsNull() || m_CachedPage != m_Page || m_CachedDpi != dpi) {
					CClientDC screen(m_hWnd);
					CDC memory;
					memory.CreateCompatibleDC(screen);
					if (!m_Cache.IsNull())
						m_Cache.DeleteObject();
					m_Cache.CreateCompatibleBitmap(screen, width, height);
					auto old = memory.SelectBitmap(m_Cache);
					CRect all(0, 0, width, height);
					memory.FillSolidRect(&all, RGB(255, 255, 255));
					memory.SetMapMode(MM_TEXT);
					auto sheet = MakePage(memory, dpi);
					Doc->PageCount(ContentOf(sheet));		// (measures the document for this resolution)
					DrawSheet(*Doc, sheet, m_Page, m_Count);
					memory.SelectBitmap(old);
					m_CachedPage = m_Page;
					m_CachedDpi = dpi;
				}
				CDC memory;
				memory.CreateCompatibleDC(dc);
				auto old = memory.SelectBitmap(m_Cache);
				CRect shadow = paper;
				shadow.OffsetRect(3, 3);
				dc.FillSolidRect(&shadow, RGB(60, 60, 60));
				dc.BitBlt(paper.left, paper.top, width, height, memory, 0, 0, SRCCOPY);
				memory.SelectBitmap(old);

				CString label;
				label.Format(L"Page %d of %d", m_Page + 1, m_Count);
				dc.SetBkMode(TRANSPARENT);
				dc.SetTextColor(::GetSysColor(COLOR_BTNTEXT));
				dc.SelectFont(m_Font.IsNull() ? AtlGetDefaultGuiFont() : m_Font.m_hFont);
				CRect text(300, 0, rc.right - 110, BarHeight);
				dc.DrawText(label, -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
				m_Prev.EnableWindow(m_Page > 0);
				m_Next.EnableWindow(m_Page + 1 < m_Count);
				return 0;
			}

			void Go(int page) {
				page = std::clamp(page, 0, m_Count - 1);
				if (page != m_Page) {
					m_Page = page;
					Invalidate();
				}
			}
			LRESULT OnPrev(WORD, WORD, HWND, BOOL&) {
				Go(m_Page - 1);
				return 0;
			}
			LRESULT OnNext(WORD, WORD, HWND, BOOL&) {
				Go(m_Page + 1);
				return 0;
			}
			LRESULT OnPrint(WORD, WORD, HWND, BOOL&) {
				Printing::Print(m_hWnd, *Doc);
				return 0;
			}
			LRESULT OnClose(WORD, WORD, HWND, BOOL&) {
				DestroyWindow();
				return 0;
			}
			LRESULT OnKeyDown(UINT, WPARAM wParam, LPARAM, BOOL& handled) {
				switch (wParam) {
					case VK_NEXT: case VK_RIGHT: Go(m_Page + 1); break;
					case VK_PRIOR: case VK_LEFT: Go(m_Page - 1); break;
					case VK_HOME: Go(0); break;
					case VK_END: Go(m_Count - 1); break;
					case VK_ESCAPE: DestroyWindow(); break;
					default: handled = FALSE; break;
				}
				return 0;
			}

			CButton m_Prev, m_Next, m_Print, m_Close;
			CBitmap m_Cache;
			CFont m_Font;
			int m_Page{ 0 }, m_Count{ 1 }, m_CachedPage{ -1 }, m_CachedDpi{ 0 };
		};
	}

	void Preview(HWND owner, Document& document) {
		if (!EnsurePrinter(owner))
			return;
		ApplyOrientation(document);

		CPreviewWnd wnd;
		wnd.Doc = &document;
		// the paper of the printer chosen
		if (HDC info = PrinterInfo()) {
			int dpiX = ::GetDeviceCaps(info, LOGPIXELSX), dpiY = ::GetDeviceCaps(info, LOGPIXELSY);
			if (dpiX > 0 && dpiY > 0) {
				wnd.PaperWidth = static_cast<double>(::GetDeviceCaps(info, PHYSICALWIDTH)) / dpiX;
				wnd.PaperHeight = static_cast<double>(::GetDeviceCaps(info, PHYSICALHEIGHT)) / dpiY;
			}
			::DeleteDC(info);
		}

		CRect owner_rc;
		::GetWindowRect(owner, &owner_rc);
		CRect rc(owner_rc);
		rc.DeflateRect(40, 30);
		CString title = L"Print Preview - " + document.Title();
		if (!wnd.Create(owner, rc, title))
			return;
		wnd.ShowWindow(SW_SHOW);
		::EnableWindow(owner, FALSE);
		MSG msg;
		int result = 1;
		while (wnd.IsWindow() && (result = ::GetMessage(&msg, nullptr, 0, 0)) > 0) {
			// the keys that turn the pages work whichever button has the focus
			bool paging = msg.message == WM_KEYDOWN && (msg.wParam == VK_NEXT || msg.wParam == VK_PRIOR || msg.wParam == VK_HOME || msg.wParam == VK_END);
			if (paging)
				wnd.SendMessage(WM_KEYDOWN, msg.wParam, msg.lParam);
			else if (!::IsDialogMessage(wnd, &msg)) {
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}
		}
		if (result == 0)
			::PostQuitMessage(static_cast<int>(msg.wParam));		// (the program is quitting: pass it on)
		::EnableWindow(owner, TRUE);
		::SetForegroundWindow(owner);
	}
}
