#include "pch.h"
#include "MidpointTreeView.h"
#include "AppSettings.h"
#include "MidpointSettings.h"
#include "DefaultFont.h"
#include <ToolbarHelper.h>
#include "DerivedCharts.h"

namespace {
	PCWSTR AngleWords(int angle) {
		return MidpointSettings::AngleWords(angle);
	}
}

CString CMidpointTreeView::PointName(ChartPoint const& point) {
	switch (point.Kind) {
		case PointKind::Ascendant: return L"Ascendant";
		case PointKind::Midheaven: return L"Midheaven";
		default: return Helpers::GetPlanetName(point.Body);
	}
}

CString CMidpointTreeView::Position(AstroPoint const& longitude) const {
	return Helpers::FormatLongitude(longitude, m_Glyphs ? FormatOptions::ShowDegreeGlyph | FormatOptions::UseGlyphs : FormatOptions::ShowDegreeGlyph);
}

CString CMidpointTreeView::Degrees(double value, int decimals) const {
	CString text;
	text.Format(L"%.*f%c", decimals, value, m_Glyphs ? 59 : 0xb0);
	return text;
}

CString CMidpointTreeView::Dial(double dial) const {
	// degrees and minutes on the dial (of 90 degrees, or of 45)
	int minutes = static_cast<int>(std::lround(dial * 60)) % (DialSize() * 60);
	CString text;
	text.Format(L"%d%c%02d'", minutes / 60, m_Glyphs ? 59 : 0xb0, minutes % 60);
	return text;
}

int CMidpointTreeView::DialSize() const {
	return 360 / Midpoints::DialDivisions(MidpointSettings::Current().TreeKind);
}

void CMidpointTreeView::SelectKind(ContactKind kind) {
	for (int i = 0; i < m_Kind.GetCount(); i++)
		if (static_cast<ContactKind>(m_Kind.GetItemData(i)) == kind)
			m_Kind.SetCurSel(i);
}

ContactOptions CMidpointTreeView::Options() const {
	return MidpointSettings::Current().TreeContacts();
}

void CMidpointTreeView::SyncSettings() {
	auto& settings = MidpointSettings::Current();
	if (m_Orb.m_hWnd) {
		m_Orb.SetCurSel(MidpointSettings::NearestOrb(settings.TreeOrb));
		SelectKind(settings.TreeKind);
	}
	Rebuild();
}

CString CMidpointTreeView::PointWords(ChartPoint const& point) const {
	auto name = PointName(point);
	if (!m_Overlay)
		return name;
	return CString(point.Set == 1 ? m_Overlay->Label.c_str() : m_Overlay->BaseLabel.c_str()) + L" " + name;
}

CString CMidpointTreeView::PointText(ChartPoint const& point) const {
	if (!m_Glyphs)
		return PointWords(point);
	// (a prime after the glyph is what tells the overlay's Sun from the chart's when both are in the tree)
	return MidpointSettings::PointGlyph(point) + (m_Overlay && point.Set == 1 ? L"'" : L"");
}

LRESULT CMidpointTreeView::OnGlyphs(WORD, WORD, HWND, BOOL&) {
	m_Glyphs = !m_Glyphs;
	m_Toolbar.CheckButton(ID_VIEW_GLYPHS, m_Glyphs);
	AppSettings::Get().MidpointTreeGlyphs(m_Glyphs ? 1 : 0);
	ApplyTreeFont();
	Rebuild();
	return 0;
}

void CMidpointTreeView::ApplyTreeFont() {
	if (!m_Tree.m_hWnd)
		return;
	CFontHandle normal = m_HasTextFont ? CFontHandle(m_TextFont.m_hFont) : CFontHandle(m_UiFont.m_hFont);
	if (!m_Glyphs || normal.IsNull()) {
		if (!normal.IsNull())
			m_Tree.SetFont(normal);
		return;
	}
	// the same size as the text, in the glyph font
	LOGFONT lf;
	normal.GetLogFont(lf);
	wcscpy_s(lf.lfFaceName, L"HamburgSymbols");
	if (m_SymbolFont)
		m_SymbolFont.DeleteObject();
	m_SymbolFont.CreateFontIndirect(&lf);
	m_Tree.SetFont(m_SymbolFont);
}

void CMidpointTreeView::SetOverlay(ChartOverlay const* overlay) {
	bool changed = (overlay != nullptr) != (m_Overlay != nullptr);
	m_Overlay = overlay;
	if (m_Pairs.m_hWnd) {
		m_Pairs.EnableWindow(overlay != nullptr);
		m_Pairs.ShowWindow(overlay ? SW_SHOW : SW_HIDE);
		m_PairsLabel.ShowWindow(overlay ? SW_SHOW : SW_HIDE);
	}
	if (changed)
		Layout();
	Rebuild();
}

void CMidpointTreeView::SetChartData(ChartData const* chart) {
	m_Data = chart;
	Rebuild();
}

void CMidpointTreeView::Rebuild() {
	m_Entries.clear();
	if (m_Tree.m_hWnd == nullptr)
		return;
	m_Tree.SetRedraw(FALSE);
	m_Tree.DeleteAllItems();
	if (m_Data && !m_Data->AllPlanets().empty()) {
		auto& settings = MidpointSettings::Current();
		auto points = Midpoints::Points(*m_Data, settings.Points(true));
		std::vector<MidpointData> midpoints;
		std::vector<ChartPoint> branches = points;		// the points the branches are of
		if (m_Overlay) {
			bool hasAngles = m_Overlay->Data.Houses().Asc.Value != 0 || m_Overlay->Data.Houses().MC.Value != 0;
			auto other = Midpoints::Points(m_Overlay->Data, settings.Points(hasAngles), 1);
			switch (m_Pairs.m_hWnd ? m_Pairs.GetCurSel() : 0) {
				case 1:		// the overlay's own midpoints, with the chart's points on them
					midpoints = Midpoints::Calculate(other);
					break;
				case 2:		// between the two, with the points of both
					midpoints = Midpoints::CalcBetween(other, points);
					branches.insert(branches.end(), other.begin(), other.end());
					break;
				default:	// the chart's midpoints, with the overlay's points on them: the transits to the natal midpoints
					midpoints = Midpoints::Calculate(points);
					branches = other;
					break;
			}
			if (m_Pairs.m_hWnd && m_Pairs.GetCurSel() == 1)
				branches = points;
		}
		else
			midpoints = Midpoints::Calculate(points);
		auto options = Options();
		auto tree = Midpoints::Tree(midpoints, branches, options);

		for (auto const& branch : tree) {
			CString text;
			if (m_Glyphs)
				text.Format(L"%s   %s   (%s)", (PCWSTR)PointText(branch.Point), (PCWSTR)Position(branch.Point.Longitude), (PCWSTR)Dial(branch.Dial));
			else
				text.Format(L"%s   %s   (on the dial: %s)", (PCWSTR)PointText(branch.Point), (PCWSTR)Position(branch.Point.Longitude), (PCWSTR)Dial(branch.Dial));
			auto root = m_Tree.InsertItem(text, TVI_ROOT, TVI_LAST);
			for (auto const& contact : branch.Contacts) {
				auto const& midpoint = midpoints[contact.Midpoint];
				CString angle = m_Glyphs ? DefaultFont::Get().GetAspectGlyphAsString(MidpointSettings::AspectOfAngle(contact.Angle)) : CString(AngleWords(contact.Angle));
				CString line;
				line.Format(L"%s / %s  =  %s   %s   %s", (PCWSTR)PointText(midpoint.A), (PCWSTR)PointText(midpoint.B),
					(PCWSTR)Position(midpoint.Longitude), (PCWSTR)angle, (PCWSTR)Degrees(contact.Orb, 2));
				m_Tree.InsertItem(line, root, TVI_LAST);
				// (what Export and Print get is always in words, with the dial written as it is with names)
				CString dial;
				int minutes = static_cast<int>(std::lround(branch.Dial * 60)) % (DialSize() * 60);
				dial.Format(L"%d%c%02d'", minutes / 60, 0xb0, minutes % 60);
				m_Entries.push_back({ PointWords(branch.Point), Helpers::FormatLongitude(branch.Point.Longitude, FormatOptions::ShowSeconds | FormatOptions::ShowDegreeGlyph),
					dial, PointWords(midpoint.A) + L"/" + PointWords(midpoint.B),
					Helpers::FormatLongitude(midpoint.Longitude, FormatOptions::ShowSeconds | FormatOptions::ShowDegreeGlyph), AngleWords(contact.Angle), contact.Orb });
			}
			m_Tree.Expand(root);
		}
		if (tree.empty()) {
			CString none;
			if (m_Glyphs)
				none = L"-";		// (the glyph font has no letters)
			else
				none.Format(L"No midpoint within %.1f%c of a point", options.Orb, 0xb0);
			m_Tree.InsertItem(none, TVI_ROOT, TVI_LAST);
		}
		if (HTREEITEM first = m_Tree.GetRootItem())
			m_Tree.SelectSetFirstVisible(first);		// (from the top, whatever the tree was scrolled to)
	}
	m_Tree.SetRedraw(TRUE);
}

Helpers::TableSource CMidpointTreeView::Table() const {
	Helpers::TableSource table;
	table.Headers = { L"Point", L"Position", L"Dial", L"Midpoint of", L"Midpoint", L"Contact", L"Orb" };
	table.Rows = static_cast<int>(m_Entries.size());
	table.Cell = [this](int row, int column) -> CString {
		auto& e = m_Entries[row];
		CString text;
		switch (column) {
			case 0: return e.Point;
			case 1: return e.PointPosition;
			case 2: return e.Dial;
			case 3: return e.Midpoint;
			case 4: return e.MidpointPosition;
			case 5: return e.Angle;
			case 6: text.Format(L"%.2f%c", e.Orb, 0xb0); return text;
		}
		return text;
	};
	return table;
}

void CMidpointTreeView::ApplyTextFont() {
	m_HasTextFont = Helpers::UserTextFont(m_TextFont);
	ApplyTreeFont();
}

LRESULT CMidpointTreeView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	// the strip's controls in the font the system uses for its windows (a new control has the old System font)
	NONCLIENTMETRICS metrics{ sizeof(metrics) };
	if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
		m_UiFont.CreateFontIndirect(&metrics.lfMessageFont);

	m_OrbLabel.Create(m_hWnd, rcDefault, L"Orb:", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE);
	m_Orb.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 0, IDC_MT_ORB);
	m_KindLabel.Create(m_hWnd, rcDefault, L"Contacts:", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE);
	m_Kind.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 0, IDC_MT_KIND);
	m_PairsLabel.Create(m_hWnd, rcDefault, L"Midpoints of:", WS_CHILD | SS_LEFT | SS_CENTERIMAGE);
	m_Pairs.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 0, IDC_MT_PAIRS);
	m_Tree.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | TVS_HASLINES | TVS_HASBUTTONS |
		TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP, 0, IDC_MT_TREE);
	// the Glyphs button: symbols or names
	ToolBarButtonInfo buttons[] = { { ID_VIEW_GLYPHS, IDI_GLYPH, BTNS_CHECK, L"Glyphs" } };
	m_Glyphs = AppSettings::Get().MidpointTreeGlyphs() != 0;
	m_Toolbar = ToolbarHelper::CreateAndInitToolBar(m_hWnd, buttons, _countof(buttons), 16);
	m_Toolbar.CheckButton(ID_VIEW_GLYPHS, m_Glyphs);
	if (!m_UiFont.IsNull())
		for (CWindow* control : { (CWindow*)&m_OrbLabel, (CWindow*)&m_Orb, (CWindow*)&m_KindLabel, (CWindow*)&m_Kind, (CWindow*)&m_PairsLabel, (CWindow*)&m_Pairs, (CWindow*)&m_Tree })
			control->SetFont(m_UiFont);

	auto& settings = MidpointSettings::Current();
	for (int orb : MidpointSettings::Orbs) {
		CString text;
		text.Format(L"%g%c", orb / 100.0, 0xb0);
		m_Orb.AddString(text);
	}
	m_Orb.SetCurSel(MidpointSettings::NearestOrb(settings.TreeOrb));
	CString dialChoice;
	dialChoice.Format(L"90%c dial (on, semi-square, square...)", 0xb0);
	m_Kind.SetItemData(m_Kind.AddString(dialChoice), static_cast<DWORD_PTR>(ContactKind::Dial90));
	dialChoice.Format(L"45%c dial (all of those together)", 0xb0);
	m_Kind.SetItemData(m_Kind.AddString(dialChoice), static_cast<DWORD_PTR>(ContactKind::Dial45));
	m_Kind.SetItemData(m_Kind.AddString(L"Axis (on and opposite)"), static_cast<DWORD_PTR>(ContactKind::Axis));
	SelectKind(settings.TreeKind);
	m_Pairs.AddString(L"the chart (with the overlay's points on them)");
	m_Pairs.AddString(L"the overlay (with the chart's points on them)");
	m_Pairs.AddString(L"the chart and the overlay (with both)");
	m_Pairs.SetCurSel(0);
	SetOverlay(m_Overlay);		// (shows or hides the choice)
	ApplyTextFont();
	Layout();
	Rebuild();
	return 0;
}

void CMidpointTreeView::Layout() {
	if (m_Tree.m_hWnd == nullptr)
		return;
	CRect rc;
	GetClientRect(&rc);
	int x = 8, y = 4, h = 24;
	m_OrbLabel.MoveWindow(x, y, 32, h);
	m_Orb.MoveWindow(x + 34, y, 64, 200);
	m_KindLabel.MoveWindow(x + 112, y, 62, h);
	m_Kind.MoveWindow(x + 176, y, 210, 200);
	if (m_Toolbar.m_hWnd) {
		CSize size;
		m_Toolbar.GetMaxSize(&size);
		m_Toolbar.MoveWindow(x + 394, y, size.cx + 4, h);
	}
	// (with an overlay a second row: whose midpoints)
	m_PairsLabel.MoveWindow(x, y + 28, 80, h);
	m_Pairs.MoveWindow(x + 84, y + 28, 302, 200);
	m_Tree.MoveWindow(0, StripHeight(), rc.Width(), std::max<int>(0, rc.Height() - StripHeight()));
}

LRESULT CMidpointTreeView::OnEraseBkgnd(UINT, WPARAM wParam, LPARAM, BOOL&) {
	// the strip above the tree in the colour of a window's face (dark in dark mode, where the program supplies these colours)
	CRect rc;
	GetClientRect(&rc);
	::FillRect(reinterpret_cast<HDC>(wParam), &rc, ::GetSysColorBrush(COLOR_3DFACE));
	return 1;
}

LRESULT CMidpointTreeView::OnCtlColorStatic(UINT, WPARAM wParam, LPARAM, BOOL&) {
	auto dc = reinterpret_cast<HDC>(wParam);
	::SetBkColor(dc, ::GetSysColor(COLOR_3DFACE));
	::SetTextColor(dc, ::GetSysColor(COLOR_WINDOWTEXT));
	return reinterpret_cast<LRESULT>(::GetSysColorBrush(COLOR_3DFACE));
}

LRESULT CMidpointTreeView::OnSize(UINT, WPARAM, LPARAM, BOOL&) {
	Layout();
	return 0;
}

LRESULT CMidpointTreeView::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
	if (m_Tree.m_hWnd)
		m_Tree.SetFocus();
	return 0;
}

LRESULT CMidpointTreeView::OnChoice(WORD, WORD, HWND, BOOL&) {
	auto& settings = MidpointSettings::Current();
	int orb = m_Orb.GetCurSel();
	if (orb >= 0 && orb < _countof(MidpointSettings::Orbs))
		settings.TreeOrb = MidpointSettings::Orbs[orb];
	if (m_Kind.GetCurSel() >= 0)
		settings.TreeKind = static_cast<ContactKind>(m_Kind.GetItemData(m_Kind.GetCurSel()));
	MidpointSettings::StoreInSettings();
	Rebuild();
	return 0;
}
