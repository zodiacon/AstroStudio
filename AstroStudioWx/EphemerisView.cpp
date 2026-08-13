#include "pch.h"
#include "EphemerisView.h"

#include "AstroFont.h"
#include "ChartData.h"
#include "Resources.h"
#include "resource.h"

#include <wx/toolbar.h>

namespace {
	// Column 0 is the date, the planet columns follow, and Phenomena is last.
	constexpr int DateColumn = 0;
	constexpr int FirstPlanetColumn = 1;

	constexpr int MinFontSize = 7;
	constexpr int MaxFontSize = 18;
	constexpr int DefaultFontSize = 10;

	constexpr long RowCount = 2000;
}

//
// ------------------------------------------------------------------- List
//

EphemerisView::List::List(EphemerisView* owner)
	: wxListCtrl(owner, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL | wxLC_HRULES * 0),
	m_Owner(owner) {
}

wxString EphemerisView::List::OnGetItemText(long item, long column) const {
	return m_Owner->CellText(item, column);
}

//
// Replaces OnSubItemPrePaint. Same rules: element background per sign, a darker
// shade when retrograde, a lightened row for today, and the glyph font whenever
// glyphs are switched on.
//
wxItemAttr* EphemerisView::List::OnGetItemColumnAttr(long item, long column) const {
	m_Owner->EnsureRow(item);
	if (item >= static_cast<long>(m_Owner->m_Items.size()))
		return nullptr;

	m_Attr = wxItemAttr();
	auto const& row = m_Owner->m_Items[item];
	auto const& colours = m_Owner->m_Colours;
	auto useGlyphs = (m_Owner->m_FormatOptions & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs;
	auto today = row.Date == DateTime::Today(true);

	if (column >= FirstPlanetColumn &&
		column < FirstPlanetColumn + static_cast<long>(row.Planets.size())) {
		auto const& planet = row.Planets[column - FirstPlanetColumn];
		wxColour back;

		if (colours.PaintSigns)
			back = colours.Element[static_cast<int>(planet.Position.Longitude.Sign()) % 4];

		if (colours.PaintRetro && planet.Position.Speed < 0)
			back = colours.PaintSigns ? AstroHelpers::Darken(back, 10) : colours.RetroBack;

		if (today && back.IsOk())
			back = AstroHelpers::Lighten(back, 25);

		if (back.IsOk()) {
			m_Attr.SetBackgroundColour(back);
			m_Attr.SetTextColour(*wxBLACK);
		}
		m_Attr.SetFont(useGlyphs ? m_Owner->m_GlyphFont : m_Owner->m_StdFont);
	}
	else {
		if (today && column == DateColumn) {
			m_Attr.SetBackgroundColour(colours.TodayTime);
			m_Attr.SetTextColour(*wxBLACK);
		}
		m_Attr.SetFont(column == DateColumn ? m_Owner->m_StdFont
			: (useGlyphs ? m_Owner->m_GlyphFont : m_Owner->m_StdFont));
	}

	return &m_Attr;
}

//
// ---------------------------------------------------------- EphemerisView
//

EphemerisView::EphemerisView(wxWindow* parent, IMainFrame* frame)
	: wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxTAB_TRAVERSAL | wxCLIP_CHILDREN),
	m_Frame(frame) {

	m_Planets = AstroHelpers::GetStandardPlanets();
	m_Planets.push_back(Planet::Chiron);
	m_Planets.push_back(Planet::Lilith);
	m_Planets.push_back(Planet::TrueNode);

	ApplyTheme();
	CreateFonts();

	auto sizer = new wxBoxSizer(wxVERTICAL);
	BuildToolBar(sizer);

	m_List = new List(this);
	sizer->Add(m_List, wxSizerFlags(1).Expand());
	SetSizer(sizer);

	m_List->AppendColumn("Date", wxLIST_FORMAT_LEFT, FromDIP(130));
	for (auto planet : m_Planets)
		m_List->AppendColumn(AstroHelpers::GetPlanetName(planet), wxLIST_FORMAT_LEFT, FromDIP(110));
	m_List->AppendColumn("Phenomena", wxLIST_FORMAT_LEFT, FromDIP(420));

	m_StartTime = DateTime::Today().AddDays(-30);
	m_Items.reserve(RowCount);
	m_List->SetItemCount(RowCount);

	m_List->Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &EphemerisView::OnRightClick, this);

	Bind(wxEVT_MENU, &EphemerisView::OnToggleGlyphs, this, ID_VIEW_GLYPHS);
	Bind(wxEVT_MENU, &EphemerisView::OnToggleSeconds, this, ID_VIEW_SECONDS);
	Bind(wxEVT_MENU, &EphemerisView::OnToggleGridLines, this, ID_VIEW_GRIDLINES);
	Bind(wxEVT_MENU, &EphemerisView::OnChangeFontSize, this, ID_FONT_BIGGER);
	Bind(wxEVT_MENU, &EphemerisView::OnChangeFontSize, this, ID_FONT_SMALLER);
	Bind(wxEVT_MENU, &EphemerisView::OnChangeFontSize, this, ID_FONT_SIZE_DEFAULT);
	Bind(wxEVT_MENU, &EphemerisView::OnNewChart, this, ID_NEW_CHART);

	static constexpr int toolbarItems[] = {
		ID_VIEW_GLYPHS, ID_VIEW_SECONDS, ID_VIEW_GRIDLINES,
		ID_FONT_BIGGER, ID_FONT_SMALLER,
	};
	for (auto id : toolbarItems)
		Bind(wxEVT_UPDATE_UI, &EphemerisView::OnUpdateUI, this, id);

	Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent& e) {
		ApplyTheme();
		m_List->Refresh();
		e.Skip();
		});
}

void EphemerisView::BuildToolBar(wxSizer* sizer) {
	// A frame-owned CreateToolBar() is not available here - this is a notebook
	// page, not a frame - so the toolbar is an ordinary child in the sizer.
	m_ToolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxTB_FLAT | wxTB_HORIZONTAL | wxTB_NODIVIDER);

	m_ToolBar->AddCheckTool(ID_VIEW_GLYPHS, "Glyphs", Resources::Icon(ICON_GLYPH),
		wxNullBitmap, "Show glyphs");
	m_ToolBar->AddCheckTool(ID_VIEW_SECONDS, "Seconds", Resources::Icon(ICON_CLOCK),
		wxNullBitmap, "Show seconds");
	m_ToolBar->AddSeparator();
	m_ToolBar->AddTool(ID_FONT_BIGGER, "Bigger", Resources::Icon(ICON_FONT_BIGGER), "Larger font");
	m_ToolBar->AddTool(ID_FONT_SMALLER, "Smaller", Resources::Icon(ICON_FONT_SMALLER), "Smaller font");
	m_ToolBar->AddTool(ID_FONT_SIZE_DEFAULT, "Default", Resources::Icon(ICON_FONT_DEFAULT), "Default font size");
	m_ToolBar->AddSeparator();
	m_ToolBar->AddCheckTool(ID_VIEW_GRIDLINES, "Grid", Resources::Icon(ICON_GRID),
		wxNullBitmap, "Grid lines");
	m_ToolBar->Realize();

	sizer->Add(m_ToolBar, wxSizerFlags().Expand());
}

void EphemerisView::ApplyTheme() {
	// The WTL build kept two hard-coded ColorOptions structs and swapped them on
	// WTLHelper::IsDarkMode(); same idea, but asked of the system.
	if (wxSystemSettings::GetAppearance().IsDark()) {
		m_Colours.RetroBack = wxColour(30, 30, 30);
		m_Colours.Element[0] = AstroHelpers::Darken(wxColour(255, 128, 0), 40);
		m_Colours.Element[1] = AstroHelpers::Darken(wxColour(224, 224, 0), 40);
		m_Colours.Element[2] = AstroHelpers::Darken(wxColour(0, 255, 128), 40);
		m_Colours.Element[3] = AstroHelpers::Darken(wxColour(0, 192, 255), 50);
		m_Colours.TodayTime = wxColour(120, 120, 0);
	}
	else {
		m_Colours.RetroBack = wxColour(220, 220, 220);
		m_Colours.Element[0] = wxColour(255, 128, 0);
		m_Colours.Element[1] = wxColour(224, 224, 0);
		m_Colours.Element[2] = wxColour(0, 255, 128);
		m_Colours.Element[3] = wxColour(0, 192, 255);
		m_Colours.TodayTime = wxColour(220, 220, 0);
	}
}

void EphemerisView::CreateFonts() {
	m_GlyphFont = wxFont(wxFontInfo(m_FontSize).FaceName(AstroHelpers::GlyphFontName()));
	m_StdFont = wxFont(wxFontInfo(m_FontSize).FaceName("Consolas"));
	AstroHelpers::LoadAstroFont();
}

void EphemerisView::AutoSizeColumns() {
	m_List->Freeze();
	for (int i = 0; i < m_List->GetColumnCount(); i++) {
		m_List->SetColumnWidth(i, wxLIST_AUTOSIZE);
		if (m_List->GetColumnWidth(i) < FromDIP(100))
			m_List->SetColumnWidth(i, FromDIP(100));
	}
	m_List->Thaw();
}

//
// Rows are computed on demand, exactly as the WTL GetColumnText did inline.
// row + 1 is filled too because GetRowPhenom compares a row against the next.
//
void EphemerisView::EnsureRow(long row) const {
	while (static_cast<long>(m_Items.size()) <= row + 1) {
		RowData data;
		data.Date = m_StartTime.AddDays(m_Increment * static_cast<int>(m_Items.size()));
		for (auto planet : m_Planets) {
			PlanetData pd;
			pd.Planet = planet;
			pd.Position = m_Calc.CalcPlanet(planet, data.Date);
			data.Planets.push_back(pd);
		}
		m_Items.push_back(std::move(data));
	}
}

wxString EphemerisView::CellText(long row, long column) const {
	EnsureRow(row);
	if (row >= static_cast<long>(m_Items.size()))
		return wxString();

	auto& item = m_Items[row];
	auto useGlyphs = (m_FormatOptions & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs;

	if (column == DateColumn)
		return AstroHelpers::FormatDateTime(item.Date);

	auto index = static_cast<int>(column) - FirstPlanetColumn;
	if (index < 0 || index >= static_cast<int>(item.Planets.size()))
		return GetRowPhenom(row);	// the trailing Phenomena column

	auto& pp = item.Planets[index];
	pp.Position.Longitude.Flags |=
		(pp.Position.Speed < 0 ? AstroPointFlags::Retro : AstroPointFlags::None);

	auto text = AstroHelpers::FormatLongitude(pp.Position.Longitude, m_FormatOptions);
	if (useGlyphs)
		text = DefaultFont::Get().GetPlanetGlyphAsString(pp.Planet) + " " + text;
	return text;
}

wxString EphemerisView::GetRowPhenom(long row) const {
	EnsureRow(row);
	auto& item = m_Items[row];

	if (!item.PhenomCalculated) {
		auto const& next = m_Items[row + 1];
		item.PhenomCalculated = true;

		auto separate = [](wxString& s) { if (!s.empty()) s += " | "; };

		for (size_t i = 0; i < item.Planets.size(); i++) {
			auto const& c = next.Planets[i];
			auto const& p = item.Planets[i];

			//
			// sign ingress
			//
			if (c.Position.Longitude.Sign() != p.Position.Longitude.Sign()) {
				separate(item.PhenomGlyph);
				separate(item.PhenomText);
				auto ingress = m_Calc.CalcPlanetIngress(c.Planet, item.Date, c.Position.Speed < 0);
				item.PhenomGlyph += DefaultFont::Get().GetPlanetGlyphAsString(c.Planet) + " "
					+ DefaultFont::Get().GetSignGlyphAsString(c.Position.Longitude.Sign());
				item.PhenomText += AstroHelpers::GetPlanetName(c.Planet) + " to "
					+ AstroHelpers::GetZodiacSignName(c.Position.Longitude.Sign()).Left(3);
				auto when = " (" + AstroHelpers::FormatDateTime(ingress.Time,
					DateTimeFormatOptions::TimeOnly) + ")";
				item.PhenomGlyph += when;
				item.PhenomText += when;
			}

			//
			// Retro / Direct station
			//
			auto direct = c.Position.Speed > 0 && p.Position.Speed < 0;
			auto retro = c.Position.Speed < 0 && p.Position.Speed > 0;
			if (direct || retro) {
				auto station = m_Calc.CalcPlanetStation(c.Planet, item.Date);
				separate(item.PhenomGlyph);
				separate(item.PhenomText);
				item.PhenomGlyph += DefaultFont::Get().GetPlanetGlyphAsString(c.Planet) + " "
					+ (direct ? DefaultFont::Get().GetDirectGlyphAsString()
							  : DefaultFont::Get().GetRetroGlyphAsString());
				item.PhenomText += AstroHelpers::GetPlanetName(c.Planet)
					+ (direct ? " D" : " R");
				auto when = " (" + AstroHelpers::FormatDateTime(station.Time,
					DateTimeFormatOptions::TimeOnly) + ")";
				item.PhenomGlyph += when;
				item.PhenomText += when;
			}
		}
	}

	return (m_FormatOptions & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs
		? item.PhenomGlyph : item.PhenomText;
}

void EphemerisView::OnToggleGlyphs(wxCommandEvent&) {
	m_FormatOptions ^= FormatOptions::UseGlyphs;
	AutoSizeColumns();
	m_List->Refresh();
}

void EphemerisView::OnToggleSeconds(wxCommandEvent&) {
	m_FormatOptions ^= FormatOptions::ShowSeconds;
	AutoSizeColumns();
	m_List->Refresh();
}

void EphemerisView::OnChangeFontSize(wxCommandEvent& e) {
	if (e.GetId() == ID_FONT_SIZE_DEFAULT)
		m_FontSize = DefaultFontSize;
	else
		m_FontSize += e.GetId() == ID_FONT_BIGGER ? 1 : -1;

	m_FontSize = std::clamp(m_FontSize, MinFontSize, MaxFontSize);
	CreateFonts();
	AutoSizeColumns();
	m_List->Refresh();
}

void EphemerisView::OnToggleGridLines(wxCommandEvent&) {
	// LVS_EX_GRIDLINES has no wx equivalent; wxLC_HRULES/wxLC_VRULES are window
	// styles, so they are toggled rather than set as an extended style.
	m_GridLines = !m_GridLines;
	auto style = m_List->GetWindowStyleFlag() & ~(wxLC_HRULES | wxLC_VRULES);
	if (m_GridLines)
		style |= wxLC_HRULES | wxLC_VRULES;
	m_List->SetWindowStyleFlag(style);
	m_List->Refresh();
}

void EphemerisView::OnRightClick(wxListEvent& e) {
	// Replaces LoadMenu(IDR_CONTEXT) + IMainFrame::TrackPopupMenu.
	wxMenu menu;
	menu.Append(ID_NEW_CHART, "&Chart");
	PopupMenu(&menu);
	e.Skip();
}

void EphemerisView::OnNewChart(wxCommandEvent&) {
	auto selected = m_List->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected < 0 || selected >= static_cast<long>(m_Items.size()))
		return;

	auto const& item = m_Items[selected];

	ChartData data;
	auto& info = data.Info();
	info = m_Frame->DefaultChartInfo();
	info.Time = item.Date;
	for (auto const& p : item.Planets)
		data.AddPlanets({ p.Position });

	m_Frame->AddChartView(std::move(data),
		AstroHelpers::FormatDateTime(item.Date).Trim());
}

void EphemerisView::OnUpdateUI(wxUpdateUIEvent& e) {
	switch (e.GetId()) {
		case ID_VIEW_GLYPHS:
			e.Check((m_FormatOptions & FormatOptions::UseGlyphs) == FormatOptions::UseGlyphs);
			break;
		case ID_VIEW_SECONDS:
			e.Check((m_FormatOptions & FormatOptions::ShowSeconds) == FormatOptions::ShowSeconds);
			break;
		case ID_VIEW_GRIDLINES:
			e.Check(m_GridLines);
			break;
		case ID_FONT_BIGGER:
			e.Enable(m_FontSize < MaxFontSize);
			break;
		case ID_FONT_SMALLER:
			e.Enable(m_FontSize > MinFontSize);
			break;
	}
}
