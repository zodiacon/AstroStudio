#include "pch.h"
#include "EphemerisView.h"
#include "ChartData.h"

void CEphemerisView::OnFinalMessage(HWND) {
	delete this;
}

CString CEphemerisView::GetColumnText(HWND h, int row, int col) {
	auto type = GetColumnManager(h)->GetColumnTag<ColumnType>(col);
	CString text;
	switch (type) {
		case ColumnType::Time: return Helpers::FormatDateTime(m_StartTime.AddDays(row));
		case ColumnType::Phenom: return GetRowPhenom(row);
			break;

		default:
			while (m_Items.size() <= row + 1) {
				RowData data;
				data.Date = m_StartTime.AddDays(m_Increment * (int)m_Items.size());
				for (auto p : m_Planets) {
					PlanetData pd;
					pd.Position = m_Calc.CalcPlanet(p, data.Date);
					pd.Planet = p;
					data.Planets.push_back(pd);
				}
				m_Items.push_back(std::move(data));
			}
			ATLASSERT(row < m_Items.size());
			auto& item = m_Items[row];
			int index = int(type) - int(ColumnType::Planet);
			auto planet = (PlanetType)(int(type) - int(ColumnType::Planet));
			auto& pp = item.Planets[index];
			pp.Position.Longitude.Flags |= (pp.Position.Speed < 0 ? AstroPointFlags::Retro : AstroPointFlags::None);
			text = Helpers::FormatLongitude(pp.Position.Longitude, m_FormatOptions);
			text = ((m_FormatOptions & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs ?
				(PCWSTR)DefaultFont::Get().GetPlanetGlyphAsString(pp.Planet) : L"") + CString(L" ") + text;
			break;
	}
	return text;
}

bool CEphemerisView::IsSortable(HWND, int col) const {
	return false;
}

bool CEphemerisView::OnRightClickList(HWND, int row, int col, POINT const& pt) {
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	return GetFrame()->TrackPopupMenu(menu.GetSubMenu(0), 0, pt.x, pt.y);
}

DWORD CEphemerisView::OnPrePaint(int, LPNMCUSTOMDRAW cd) {
	return CDRF_NOTIFYITEMDRAW;
}

DWORD CEphemerisView::OnItemPrePaint(int, LPNMCUSTOMDRAW cd) {
	return CDRF_NOTIFYSUBITEMDRAW;
}

DWORD CEphemerisView::OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) {
	if ((int)cd->dwItemSpec >= m_Items.size())
		return CDRF_DODEFAULT;

	auto lv = (NMLVCUSTOMDRAW*)cd;
	auto colType = GetColumnManager(m_List)->GetColumnTag<ColumnType>(lv->iSubItem);
	CDCHandle dc(cd->hdc);

	lv->clrTextBk = CLR_INVALID;
	auto& item = m_Items[(int)cd->dwItemSpec];
	bool highlight = item.Date == DateTime::Today();
	if (colType >= ColumnType::Planet) {
		if (m_ColorOptions.PaintSigns) {
			int element = int(item.Planets[lv->iSubItem - 1].Position.Longitude.Sign()) % 4;
			lv->clrTextBk = m_ColorOptions.ElementBackColor[element];
		}
		if (m_ColorOptions.PaintRetro) {
			if (item.Planets[lv->iSubItem - 1].Position.Speed < 0) {
				if (m_ColorOptions.PaintSigns)
					lv->clrTextBk = Helpers::Darken(lv->clrTextBk, 10);
				else
					lv->clrTextBk = m_ColorOptions.RetroBackColor;
			}
		}
		if (highlight)
			lv->clrTextBk = Helpers::Lighten(lv->clrTextBk, 25);
	}
	else if (highlight && colType == ColumnType::Time)
		lv->clrTextBk = RGB(220, 220, 0);

	dc.SelectFont(colType != ColumnType::Time && (m_FormatOptions & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs ? m_Font : m_StdFont);
	return CDRF_NEWFONT | CDRF_SKIPPOSTPAINT;
}

void CEphemerisView::UpdateUI(CUpdateUIBase& ui) {
	UpdateViewUI();
}

void CEphemerisView::UpdateList() {
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
}

CString CEphemerisView::GetRowPhenom(int row) const {
	CString text;
	auto& item = m_Items[row];
	if (!item.PhenomCalculated) {
		auto& next = m_Items[row + 1];
		item.PhenomCalculated = true;
		auto count = (int)item.Planets.size();
		for (int i = 0; i < count; i++) {
			auto& c = next.Planets[i];
			auto& p = item.Planets[i];

			//
			// sign ingress
			//
			if (c.Position.Longitude.Sign() != p.Position.Longitude.Sign()) {
				if (!item.PhenomGlyph.IsEmpty())
					item.PhenomGlyph += L" | ";
				if (!item.PhenomText.IsEmpty())
					item.PhenomText += L" | ";
				auto ingress = m_Calc.CalcPlanetIngress(c.Planet, item.Date, c.Position.Speed < 0);
				item.PhenomGlyph += DefaultFont::Get().GetPlanetGlyphAsString(c.Planet) + CString(L" ") +
					DefaultFont::Get().GetSignGlyphAsString(c.Position.Longitude.Sign());
				item.PhenomText += Helpers::GetPlanetName(c.Planet) + CString(L" to ") +
					Helpers::GetZodiacSignName(c.Position.Longitude.Sign()).Left(3);
				auto dt = L" (" + Helpers::FormatDateTime(ingress.Time, DateTimeFormatOptions::TimeOnly) + L")";
				item.PhenomGlyph += dt;
				item.PhenomText += dt;
			}
			//
			// Retro/Direct
			//
			bool direct = c.Position.Speed > 0 && p.Position.Speed < 0;
			bool retro = c.Position.Speed < 0 && p.Position.Speed > 0;
			if (direct || retro) {
				auto station = m_Calc.CalcPlanetStation(c.Planet, item.Date);
				if (!item.PhenomGlyph.IsEmpty())
					item.PhenomGlyph += L" | ";
				if (!item.PhenomText.IsEmpty())
					item.PhenomText += L" | ";
				if (direct) {
					item.PhenomGlyph += DefaultFont::Get().GetPlanetGlyphAsString(c.Planet) + CString(L" ") + DefaultFont::Get().GetDirectGlyphAsString();
					item.PhenomText += Helpers::GetPlanetName(c.Planet) + CString(L" D");
				}
				else {
					item.PhenomGlyph += DefaultFont::Get().GetPlanetGlyphAsString(c.Planet) + CString(L" ") + DefaultFont::Get().GetRetroGlyphAsString();
					item.PhenomText += Helpers::GetPlanetName(c.Planet) + CString(L" R");
				}
				auto dt = L" (" + Helpers::FormatDateTime(station.Time, DateTimeFormatOptions::TimeOnly) + L")";
				item.PhenomGlyph += dt;
				item.PhenomText += dt;
			}
		}
	}
	return (m_FormatOptions & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs ? item.PhenomGlyph : item.PhenomText;
}

void CEphemerisView::CreateFonts() {
	if (m_Font)
		m_Font.DeleteObject();
	m_Font.CreatePointFont(m_FontSize, L"HamburgSymbols");
	if (m_StdFont)
		m_StdFont.DeleteObject();
	m_StdFont.CreatePointFont(m_FontSize, L"Consolas");
}

void CEphemerisView::UpdateViewUI() {
	UISetCheck(ID_VIEW_GLYPHS, (m_FormatOptions & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs);
	UISetCheck(ID_VIEW_SECONDS, (m_FormatOptions & FormatOptions::ShowSeconds) == FormatOptions::ShowSeconds);
	UIEnable(ID_FONT_BIGGER, m_FontSize < 180);
	UIEnable(ID_FONT_SMALLER, m_FontSize > 70);
	UISetCheck(ID_VIEW_GRIDLINES, (m_List.GetExtendedListViewStyle() & LVS_EX_GRIDLINES) != 0);
}

void CEphemerisView::AutoSizeColumns() {
	m_List.SetRedraw(FALSE);
	int count = m_List.GetHeader().GetItemCount();
	for (int i = 0; i < count; i++) {
		m_List.SetColumnWidth(i, LVSCW_AUTOSIZE);
		if (m_List.GetColumnWidth(i) < 100)
			m_List.SetColumnWidth(i, 100);
	}
	m_List.SetRedraw(TRUE);
}

LRESULT CEphemerisView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
		| LVS_OWNERDATA | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 0, 1);
	m_List.SetImageList(images, LVSIL_SMALL);

	ToolBarButtonInfo buttons[] = {
		{ ID_VIEW_GLYPHS, IDI_GLYPH, BTNS_CHECK, L"Glyphs" },
		{ ID_VIEW_SECONDS, IDI_CLOCK, BTNS_CHECK, L"Seconds" },
		{ 0 },
		{ ID_FONT_BIGGER, IDI_FONT_BIGGER },
		{ ID_FONT_SMALLER, IDI_FONT_SMALLER },
		{ ID_FONT_SIZE_DEFAULT, IDI_FONT_SIZE_DEFAULT },
		{ ID_VIEW_GRIDLINES, IDI_GRID, BTNS_CHECK },
	};

	CreateAndInitToolBar(buttons, _countof(buttons));
	CreateFonts();

	m_Planets = Helpers::GetStandardPlanets();
	m_Planets.push_back(PlanetType::Chiron);
	m_Planets.push_back(PlanetType::Lilith);
	//m_Planets.push_back(PlanetType::OscuApog);
	m_Planets.push_back(PlanetType::TrueNode);

	m_FormatOptions = FormatOptions::UseGlyphs | FormatOptions::ShowDegreeGlyph;

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Date", LVCFMT_LEFT, 120, ColumnType::Time);
	int i = 0;
	for (auto& p : m_Planets) {
		cm->AddColumn(Helpers::GetPlanetName(p), LVCFMT_LEFT, 100, ColumnType(int(ColumnType::Planet) + i++));
	}
	cm->AddColumn(L"Phenomena", LVCFMT_LEFT, 160, ColumnType::Phenom);
	cm->UpdateColumns();

	m_StartTime = DateTime::Today();
	m_StartTime = m_StartTime.AddDays(-m_StartTime.Day() + 1);
	m_Items.reserve(500);
	m_List.SetItemCount(1444);

	return 0;
}

LRESULT CEphemerisView::OnViewGlyphs(WORD, WORD, HWND, BOOL&) {
	m_FormatOptions ^= FormatOptions::UseGlyphs;
	UpdateList();
	AutoSizeColumns();
	m_List.RedrawWindow();

	return 0;
}

LRESULT CEphemerisView::OnViewSeconds(WORD, WORD, HWND, BOOL&) {
	m_FormatOptions ^= FormatOptions::ShowSeconds;
	UpdateList();
	AutoSizeColumns();
	m_List.RedrawWindow();

	return 0;
}

LRESULT CEphemerisView::OnChangeFontSize(WORD, WORD id, HWND, BOOL&) {
	if (id == ID_FONT_SIZE_DEFAULT)
		m_FontSize = 100;
	else
		m_FontSize += id == ID_FONT_BIGGER ? 8 : -8;
	auto images = m_List.GetImageList(LVSIL_SMALL);
	images.SetIconSize(1, m_FontSize / 6);
	m_List.SetImageList(images, LVSIL_SMALL);

	CreateFonts();
	AutoSizeColumns();
	m_List.RedrawWindow();
	UpdateViewUI();

	return 0;
}

LRESULT CEphemerisView::OnViewGridLines(WORD, WORD, HWND, BOOL&) {
	auto style = m_List.GetExtendedListViewStyle() ^ LVS_EX_GRIDLINES;
	m_List.SetExtendedListViewStyle(style, LVS_EX_GRIDLINES);

	return 0;
}

LRESULT CEphemerisView::OnEditCopy(WORD, WORD, HWND, BOOL&) {
	return LRESULT();
}

LRESULT CEphemerisView::OnNewChart(WORD, WORD, HWND, BOOL&) {
	auto const& item = m_Items[m_List.GetSelectionMark()];
	ChartData data;
	for (auto& p : item.Planets)
		data.AddPlanets({ p.Position });
	data.Houses(AstroCalculator::CalcHouses(item.Date, 32, 34, HouseSystem::Koch));
	GetFrame()->AddChartView(std::move(data), L"Chart 1");

	return 0;
}
