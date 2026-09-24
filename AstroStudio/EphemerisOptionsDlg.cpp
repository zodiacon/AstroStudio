#include "pch.h"
#include "EphemerisOptionsDlg.h"
#include "Helpers.h"
#include "TimeZones.h"
#include <cmath>

namespace {
	// every body that can have a column, in the order they are listed
	const Planet AllBodies[] = {
		Planet::Sun, Planet::Moon, Planet::Mercury, Planet::Venus, Planet::Mars, Planet::Jupiter, Planet::Saturn,
		Planet::Uranus, Planet::Neptune, Planet::Pluto, Planet::MeanNode, Planet::TrueNode, Planet::Lilith, Planet::OscuApog,
		Planet::Chiron, Planet::Pholus, Planet::Ceres, Planet::Pallas, Planet::Juno, Planet::Vesta,
	};

	PCWSTR const MonthNames[] = {
		L"January", L"February", L"March", L"April", L"May", L"June",
		L"July", L"August", L"September", L"October", L"November", L"December",
	};
}

void CEphemerisOptionsDlg::SetSettings(EphemerisSettings const& settings) {
	m_Settings = settings;
}

EphemerisSettings const& CEphemerisOptionsDlg::GetSettings() const {
	return m_Settings;
}

bool CEphemerisOptionsDlg::GetYear(long& year) const {
	CString text;
	GetDlgItemText(IDC_YEAR, text);
	text.Trim();
	if (text.IsEmpty())
		return false;

	PWSTR end;
	year = wcstol(text, &end, 10);
	return *end == 0 && year >= MinChartYear && year <= MaxChartYear;
}

void CEphemerisOptionsDlg::UpdateDays() {
	int month = m_Month.GetCurSel() + 1;
	long year = 2000;
	if (!GetYear(year))
		year = 2000;

	// 29 February needs a leap year in whichever calendar the date falls in
	bool gregorian = DateTime::AfterPapalReform(year, month, 1);
	int count = DateTime::DaysInMonth(month, DateTime::IsLeap(year, gregorian));

	int selected = m_Day.GetCurSel();
	while (m_Day.GetCount() > count)
		m_Day.DeleteString(m_Day.GetCount() - 1);
	while (m_Day.GetCount() < count) {
		CString text;
		text.Format(L"%d", m_Day.GetCount() + 1);
		m_Day.AddString(text);
	}
	m_Day.SetCurSel(std::min(std::max(selected, 0), count - 1));
}

void CEphemerisOptionsDlg::ShowDate(DateTime const& date) {
	SetDlgItemInt(IDC_YEAR, date.Year(), TRUE);
	m_Month.SetCurSel(date.Month() - 1);
	UpdateDays();
	m_Day.SetCurSel(date.Day() - 1);
}

LRESULT CEphemerisOptionsDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	CenterWindow(GetParent());

	m_Day = GetDlgItem(IDC_DAY);
	m_Month = GetDlgItem(IDC_MONTH);
	for (auto name : MonthNames)
		m_Month.AddString(name);
	for (int day = 1; day <= 31; day++) {
		CString text;
		text.Format(L"%d", day);
		m_Day.AddString(text);
	}
	GetDlgItem(IDC_YEAR).SendMessage(EM_LIMITTEXT, 5);		// "-3000"
	ShowDate(m_Settings.Start);

	CString step;
	step.Format(L"%d", (int)std::lround(m_Settings.Step));
	SetDlgItemText(IDC_EPH_STEP, step);

	m_Bodies = GetDlgItem(IDC_EPH_BODIES);
	m_Bodies.SetExtendedListViewStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
	m_Bodies.InsertColumn(0, L"", LVCFMT_LEFT, 100);
	for (auto planet : AllBodies) {
		int item = m_Bodies.AddItem(m_Bodies.GetItemCount(), 0, Helpers::GetPlanetName(planet));
		m_Bodies.SetItemData(item, (DWORD_PTR)planet);
		bool shown = std::find(m_Settings.Planets.begin(), m_Settings.Planets.end(), planet) != m_Settings.Planets.end();
		m_Bodies.SetCheckState(item, shown);
	}
	// the column takes the whole width, and leaves room for the scroll bar
	CRect rc;
	m_Bodies.GetClientRect(&rc);
	m_Bodies.SetColumnWidth(0, rc.Width() - ::GetSystemMetrics(SM_CXVSCROLL));

	CheckDlgButton(IDC_EPH_ECLIPSES, m_Settings.Eclipses);
	CheckDlgButton(IDC_EPH_VOID, m_Settings.VoidOfCourse);
	UpdateExtras();
	return TRUE;
}

double CEphemerisOptionsDlg::TypedStep() const {
	CString text;
	GetDlgItemText(IDC_EPH_STEP, text);
	text.Trim();
	if (text.IsEmpty() || text.SpanIncluding(L"0123456789").GetLength() != text.GetLength())
		return 0;
	double step = _wtof(text);
	return step >= 1 && step <= EphemerisSettings::MaxStep ? step : 0;
}

void CEphemerisOptionsDlg::UpdateExtras() {
	double step = TypedStep();
	::EnableWindow(GetDlgItem(IDC_EPH_ECLIPSES), step > 0 && step <= EphemerisSettings::MaxEclipseStep);
	::EnableWindow(GetDlgItem(IDC_EPH_VOID), step > 0 && step <= EphemerisSettings::MaxVoidStep);
}

void CEphemerisOptionsDlg::CheckBodies(bool all) {
	auto standard = Helpers::GetStandardPlanets();
	for (int i = 0; i < m_Bodies.GetItemCount(); i++) {
		auto planet = (Planet)m_Bodies.GetItemData(i);
		m_Bodies.SetCheckState(i, all || std::find(standard.begin(), standard.end(), planet) != standard.end());
	}
}

bool CEphemerisOptionsDlg::Fail(UINT control, PCWSTR message) {
	AtlMessageBox(m_hWnd, message, L"Astro Studio", MB_ICONWARNING);
	GotoDlgCtrl(GetDlgItem(control));
	return false;
}

LRESULT CEphemerisOptionsDlg::OnOK(WORD, WORD, HWND, BOOL&) {
	EphemerisSettings settings = m_Settings;

	long year;
	if (!GetYear(year)) {
		Fail(IDC_YEAR, L"The year must be a number from -3000 to 3000 (0 is 1 BC).");
		return 0;
	}
	long month = m_Month.GetCurSel() + 1, day = m_Day.GetCurSel() + 1;
	if (year == 1582 && month == 10 && day > 4 && day < 15) {
		Fail(IDC_DAY, L"The days from 5 to 14 October 1582 do not exist: the calendar reform skipped them.");
		return 0;
	}
	// dates before 15 October 1582 are in the Julian calendar
	settings.Start = DateTime(year, month, (double)day, 0, 0, 0, DateTime::AfterPapalReform(year, month, day));

	settings.Step = TypedStep();
	if (settings.Step == 0) {
		Fail(IDC_EPH_STEP, L"The step is a whole number of days, from 1 to 366.");
		return 0;
	}
	settings.Planets.clear();
	for (int i = 0; i < m_Bodies.GetItemCount(); i++)
		if (m_Bodies.GetCheckState(i))
			settings.Planets.push_back((Planet)m_Bodies.GetItemData(i));
	if (settings.Planets.empty()) {
		Fail(IDC_EPH_BODIES, L"Choose at least one body.");
		return 0;
	}
	settings.Eclipses = IsDlgButtonChecked(IDC_EPH_ECLIPSES) == BST_CHECKED && settings.Step <= EphemerisSettings::MaxEclipseStep;
	settings.VoidOfCourse = IsDlgButtonChecked(IDC_EPH_VOID) == BST_CHECKED && settings.Step <= EphemerisSettings::MaxVoidStep;

	m_Settings = settings;
	EndDialog(IDOK);
	return 0;
}

LRESULT CEphemerisOptionsDlg::OnCancel(WORD, WORD, HWND, BOOL&) {
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CEphemerisOptionsDlg::OnToday(WORD, WORD, HWND, BOOL&) {
	ShowDate(DateTime::Today());
	return 0;
}

LRESULT CEphemerisOptionsDlg::OnMonthOrYearChanged(WORD, WORD, HWND, BOOL&) {
	UpdateDays();
	return 0;
}

LRESULT CEphemerisOptionsDlg::OnStandard(WORD, WORD, HWND, BOOL&) {
	CheckBodies(false);
	return 0;
}

LRESULT CEphemerisOptionsDlg::OnAll(WORD, WORD, HWND, BOOL&) {
	CheckBodies(true);
	return 0;
}

LRESULT CEphemerisOptionsDlg::OnStepChanged(WORD, WORD, HWND, BOOL&) {
	UpdateExtras();
	return 0;
}
