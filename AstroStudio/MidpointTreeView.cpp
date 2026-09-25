#include "pch.h"
#include "MidpointTreeView.h"
#include "AppSettings.h"
#include "DerivedCharts.h"

namespace {
	// the orbs to choose from, in degrees
	const double Orbs[] = { 0.5, 1, 1.5, 2, 3 };

	PCWSTR AngleWords(int angle) {
		switch (angle) {
			case 0: return L"on";
			case 45: return L"semi-square";
			case 90: return L"square";
			case 135: return L"sesquiquadrate";
			case 180: return L"opposite";
		}
		return L"";
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
	return Helpers::FormatLongitude(longitude, FormatOptions::ShowDegreeGlyph);
}

CString CMidpointTreeView::Dial(double dial) const {
	// degrees and minutes on the dial of 90 degrees
	int minutes = static_cast<int>(std::lround(dial * 60)) % (90 * 60);
	CString text;
	text.Format(L"%d%c%02d'", minutes / 60, 0xb0, minutes % 60);
	return text;
}

ContactOptions CMidpointTreeView::Options() const {
	ContactOptions options;
	int orb = m_Orb.m_hWnd ? m_Orb.GetCurSel() : -1;
	options.Orb = orb >= 0 && orb < _countof(Orbs) ? Orbs[orb] : 1.5;
	options.Kind = m_Kind.m_hWnd && m_Kind.GetCurSel() == 1 ? ContactKind::Axis : ContactKind::Dial90;
	return options;
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
		MidpointOptions midpointOptions;
		midpointOptions.Angles = true;
		auto points = Midpoints::Points(*m_Data, midpointOptions);
		auto midpoints = Midpoints::Calculate(points);
		auto options = Options();
		auto tree = Midpoints::Tree(midpoints, points, options);

		for (auto const& branch : tree) {
			CString text;
			text.Format(L"%s   %s   (on the dial: %s)", (PCWSTR)PointName(branch.Point), (PCWSTR)Position(branch.Point.Longitude), (PCWSTR)Dial(branch.Dial));
			auto root = m_Tree.InsertItem(text, TVI_ROOT, TVI_LAST);
			for (auto const& contact : branch.Contacts) {
				auto const& midpoint = midpoints[contact.Midpoint];
				CString line;
				line.Format(L"%s / %s  =  %s   %s   %.2f%c", (PCWSTR)PointName(midpoint.A), (PCWSTR)PointName(midpoint.B),
					(PCWSTR)Position(midpoint.Longitude), AngleWords(contact.Angle), contact.Orb, 0xb0);
				m_Tree.InsertItem(line, root, TVI_LAST);
				CString orb;
				orb.Format(L"%.2f%c", contact.Orb, 0xb0);
				m_Entries.push_back({ PointName(branch.Point), Helpers::FormatLongitude(branch.Point.Longitude, FormatOptions::ShowSeconds | FormatOptions::ShowDegreeGlyph),
					Dial(branch.Dial), PointName(midpoint.A) + L"/" + PointName(midpoint.B),
					Helpers::FormatLongitude(midpoint.Longitude, FormatOptions::ShowSeconds | FormatOptions::ShowDegreeGlyph), AngleWords(contact.Angle), contact.Orb });
			}
			m_Tree.Expand(root);
		}
		if (tree.empty()) {
			CString none;
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
	if (Helpers::UserTextFont(m_TextFont) && m_Tree.m_hWnd)
		m_Tree.SetFont(m_TextFont);
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
	m_Tree.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | TVS_HASLINES | TVS_HASBUTTONS |
		TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP, 0, IDC_MT_TREE);
	if (!m_UiFont.IsNull())
		for (CWindow* control : { (CWindow*)&m_OrbLabel, (CWindow*)&m_Orb, (CWindow*)&m_KindLabel, (CWindow*)&m_Kind, (CWindow*)&m_Tree })
			control->SetFont(m_UiFont);

	auto& settings = AppSettings::Get();
	for (double orb : Orbs) {
		CString text;
		text.Format(L"%.1f%c", orb, 0xb0);
		m_Orb.AddString(text);
	}
	int selected = 2;		// 1.5 degrees
	for (int i = 0; i < _countof(Orbs); i++)
		if (std::lround(Orbs[i] * 10) == settings.MidpointTreeOrb())
			selected = i;
	m_Orb.SetCurSel(selected);
	CString dialChoice;
	dialChoice.Format(L"90%c dial (on, semi-square, square...)", 0xb0);
	m_Kind.AddString(dialChoice);
	m_Kind.AddString(L"Axis (on and opposite)");
	m_Kind.SetCurSel(settings.MidpointTreeAxis() ? 1 : 0);
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
	m_Tree.MoveWindow(0, StripHeight, rc.Width(), std::max<int>(0, rc.Height() - StripHeight));
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
	auto& settings = AppSettings::Get();
	int orb = m_Orb.GetCurSel();
	if (orb >= 0 && orb < _countof(Orbs))
		settings.MidpointTreeOrb(static_cast<int>(std::lround(Orbs[orb] * 10)));
	settings.MidpointTreeAxis(m_Kind.GetCurSel() == 1 ? 1 : 0);
	Rebuild();
	return 0;
}
