#include "pch.h"
#include "AnalysisDlg.h"
#include "AnalysisNames.h"
#include "AspectOptions.h"
#include "AppSettings.h"
#include "Helpers.h"
#include <algorithm>

namespace {
	// every planet that can move or be aimed at (the charts are seen from Earth)
	std::vector<Planet> const& AllBodies() {
		static std::vector<Planet> bodies = [] {
			std::vector<Planet> result;
			for (int i = 0; i < static_cast<int>(Planet::NumPlanets); i++)
				if (static_cast<Planet>(i) != Planet::Earth && static_cast<Planet>(i) != Planet::PartOfFortune)
					result.push_back(static_cast<Planet>(i));
			return result;
		}();
		return bodies;
	}

	constexpr AnalysisType Types[] = {
		AnalysisType::TransitsToNatal, AnalysisType::ProgressedToNatal, AnalysisType::SolarArcToNatal,
		AnalysisType::TransitsToProgressed, AnalysisType::ProgressedToProgressed,
	};

	enum Preset { Custom, Next30Days, Next12Months, ThisYear, Next5Years, Next10Years, BirthTo90 };
	PCWSTR const PresetNames[] = { L"(dates above)", L"Next 30 days", L"Next 12 months", L"This calendar year", L"Next 5 years", L"Next 10 years", L"Birth to age 90" };

	void FillPlanetList(CListViewCtrl& list, std::vector<Planet> const& checked) {
		list.SetExtendedListViewStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
		list.InsertColumn(0, L"", LVCFMT_LEFT, 100);
		for (auto planet : AllBodies()) {
			int item = list.AddItem(list.GetItemCount(), 0, Helpers::GetPlanetName(planet));
			list.SetItemData(item, static_cast<DWORD_PTR>(planet));
			list.SetCheckState(item, std::find(checked.begin(), checked.end(), planet) != checked.end());
		}
		// the column takes the whole width, and leaves room for the scroll bar
		CRect rc;
		list.GetClientRect(&rc);
		list.SetColumnWidth(0, rc.Width() - ::GetSystemMetrics(SM_CXVSCROLL));
	}

	std::vector<Planet> StandardBodies() {
		return Helpers::GetStandardPlanets();
	}
}

AnalysisSettings CAnalysisDlg::Defaults() {
	AnalysisSettings settings;
	settings.From = DateTime::Today();
	settings.To = settings.From.AddDays(365);
	settings.Movers = StandardBodies();
	settings.Targets = StandardBodies();
	settings.Aspects = AspectOptions::Current().Transit;
	// what the last analysis was made of, if there was one
	settings.FromText(AppSettings::Get().LastAnalysis());
	return settings;
}

std::vector<Planet> CAnalysisDlg::Checked(CListViewCtrl& list) {
	std::vector<Planet> planets;
	for (int i = 0; i < list.GetItemCount(); i++)
		if (list.GetCheckState(i))
			planets.push_back(static_cast<Planet>(list.GetItemData(i)));
	return planets;
}

LRESULT CAnalysisDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	CenterWindow(GetParent());

	m_ChartBox = GetDlgItem(IDC_AN_CHART);
	for (auto const& chart : *m_Charts)
		m_ChartBox.AddString(chart.Name);
	m_ChartBox.SetCurSel(std::clamp(m_Chart, 0, std::max(0, (int)m_Charts->size() - 1)));

	// the analyses to run: any number of them, together
	m_Types = GetDlgItem(IDC_AN_TYPE);
	m_Types.SetExtendedListViewStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
	m_Types.InsertColumn(0, L"", LVCFMT_LEFT, 100);
	auto ticked = m_Settings.TypeList();
	for (auto type : Types) {
		int item = m_Types.AddItem(m_Types.GetItemCount(), 0, AnalysisNames::TypeName(type));
		m_Types.SetItemData(item, static_cast<DWORD_PTR>(type));
		m_Types.SetCheckState(item, std::find(ticked.begin(), ticked.end(), type) != ticked.end());
	}
	CRect typesRect;
	m_Types.GetClientRect(&typesRect);
	m_Types.SetColumnWidth(0, typesRect.Width() - ::GetSystemMetrics(SM_CXVSCROLL));

	m_PresetBox = GetDlgItem(IDC_AN_PRESET);
	for (auto name : PresetNames)
		m_PresetBox.AddString(name);
	m_PresetBox.SetCurSel(Custom);

	m_From.Init(m_hWnd, IDC_AN_FROM_DAY, IDC_AN_FROM_MONTH, IDC_AN_FROM_YEAR);
	m_To.Init(m_hWnd, IDC_AN_TO_DAY, IDC_AN_TO_MONTH, IDC_AN_TO_YEAR);
	m_From.Set(m_Settings.From);
	// the range ends where its last day does
	m_To.Set(m_Settings.To.AddDays(-1));

	m_Movers = GetDlgItem(IDC_AN_MOVERS);
	FillPlanetList(m_Movers, m_Settings.Movers);
	m_Targets = GetDlgItem(IDC_AN_TARGETS);
	FillPlanetList(m_Targets, m_Settings.Targets);
	CheckDlgButton(IDC_AN_ANGLES, m_Settings.NatalAngles);

	m_Aspects = GetDlgItem(IDC_AN_ASPECTLIST);
	m_Aspects.SetExtendedListViewStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
	m_Aspects.InsertColumn(0, L"", LVCFMT_LEFT, 100);
	for (int i = 0; i < AspectSettings::AspectTypeCount; i++) {
		int item = m_Aspects.AddItem(i, 0, Helpers::GetAspectName(static_cast<AspectType>(i)));
		m_Aspects.SetItemData(item, i);
		// (the transit set can be "major aspects only", which switches the minor ones off without unticking them)
		m_Aspects.SetCheckState(item, m_Settings.Aspects.AspectEnabled[i] && (!m_Settings.Aspects.MajorOnly || i <= static_cast<int>(AspectType::Opposition)));
	}
	CRect rc;
	m_Aspects.GetClientRect(&rc);
	m_Aspects.SetColumnWidth(0, rc.Width() - ::GetSystemMetrics(SM_CXVSCROLL));

	CheckDlgButton(IDC_AN_ASPECTS, m_Settings.AspectEvents);
	CheckDlgButton(IDC_AN_HOUSES, m_Settings.HouseIngresses);
	CheckDlgButton(IDC_AN_SIGNS, m_Settings.SignIngresses);
	CheckDlgButton(IDC_AN_STATIONS, m_Settings.Stations);

	m_Loading = false;
	UpdateType();
	return TRUE;
}

std::vector<AnalysisType> CAnalysisDlg::SelectedTypes() const {
	std::vector<AnalysisType> types;
	for (int i = 0; i < m_Types.GetItemCount(); i++)
		if (m_Types.GetCheckState(i))
			types.push_back(static_cast<AnalysisType>(m_Types.GetItemData(i)));
	return types;
}

void CAnalysisDlg::UpdateType() {
	// house ingresses are through the birth chart's houses, and the angles are the birth chart's: only when one of the
	// analyses compares with it
	bool natal = false;
	for (auto type : SelectedTypes()) {
		AnalysisSettings probe;
		probe.Type = type;
		natal |= probe.NatalTargets();
	}
	GetDlgItem(IDC_AN_HOUSES).EnableWindow(natal);
	GetDlgItem(IDC_AN_ANGLES).EnableWindow(natal);
	UpdateNote();
}

void CAnalysisDlg::UpdateNote() {
	if (m_Loading)
		return;
	DateTime from, to;
	UINT control;
	PCWSTR problem;
	CString note;
	if (m_From.Get(from, control, problem) && m_To.Get(to, control, problem)) {
		bool moonChecked = false;
		for (auto planet : Checked(m_Movers))
			moonChecked |= planet == Planet::Moon;
		bool dropped = false;
		for (auto type : SelectedTypes()) {
			AnalysisSettings probe;
			probe.Type = type;
			probe.From = from;
			probe.To = to.AddDays(1);
			dropped |= probe.MoonDropped();
		}
		if (moonChecked && dropped)
			note = L"The Moon is left out of transits over more than 2 months.";
	}
	SetDlgItemText(IDC_AN_NOTE, note);
}

bool CAnalysisDlg::Fail(UINT control, PCWSTR message) {
	AtlMessageBox(m_hWnd, message, L"Analysis", MB_ICONWARNING);
	if (control)
		GetDlgItem(control).SetFocus();
	return false;
}

bool CAnalysisDlg::ReadDates(DateTime& from, DateTime& to) {
	UINT control;
	PCWSTR problem;
	if (!m_From.Get(from, control, problem))
		return Fail(control, problem);
	if (!m_To.Get(to, control, problem))
		return Fail(control, problem);
	to = to.AddDays(1);		// through the last day
	if (!(to.Julian() > from.Julian()))
		return Fail(IDC_AN_TO_DAY, L"The range must end on or after the day it starts on.");
	return true;
}

LRESULT CAnalysisDlg::OnTypesChanged(int, LPNMHDR, BOOL&) {
	if (!m_Loading)
		UpdateType();
	return 0;
}

LRESULT CAnalysisDlg::OnChartChanged(WORD, WORD, HWND, BOOL&) {
	m_Chart = m_ChartBox.GetCurSel();
	return 0;
}

LRESULT CAnalysisDlg::OnPreset(WORD, WORD, HWND, BOOL&) {
	int preset = m_PresetBox.GetCurSel();
	auto today = DateTime::Today();
	DateTime from = today, to = today;		// the last day
	switch (preset) {
		case Next30Days: to = today.AddDays(29); break;
		case Next12Months: to = today.AddDays(364); break;
		case ThisYear: {
			long year = today.Year();
			from = DateTime(year, 1, 1.0, 0, 0, 0, true);
			to = DateTime(year, 12, 31.0, 0, 0, 0, true);
			break;
		}
		case Next5Years: to = today.AddDays(5 * 365); break;
		case Next10Years: to = today.AddDays(10 * 365); break;
		case BirthTo90:
			if (m_Chart >= 0 && m_Chart < (int)m_Charts->size()) {
				auto birth = (*m_Charts)[m_Chart].Data.Info().Time;
				from = DateTime(birth.Year(), birth.Month(), (double)birth.Day(), 0, 0, 0, DateTime::AfterPapalReform(birth.Year(), birth.Month(), birth.Day()));
				to = from.AddDays(90 * 365.25);
			}
			break;
		default:
			return 0;
	}
	m_From.Set(from);
	m_To.Set(to);
	UpdateNote();
	return 0;
}

LRESULT CAnalysisDlg::OnFromChanged(WORD, WORD, HWND, BOOL&) {
	m_From.Update();
	DatesEdited();
	return 0;
}

LRESULT CAnalysisDlg::OnToChanged(WORD, WORD, HWND, BOOL&) {
	m_To.Update();
	DatesEdited();
	return 0;
}

void CAnalysisDlg::DatesEdited() {
	if (!m_Loading)
		m_PresetBox.SetCurSel(Custom);
	UpdateNote();
}

LRESULT CAnalysisDlg::OnDatesEdited(WORD, WORD, HWND, BOOL&) {
	DatesEdited();
	return 0;
}

LRESULT CAnalysisDlg::OnCheckAll(WORD, WORD id, HWND, BOOL&) {
	auto standard = StandardBodies();
	auto setAll = [](CListViewCtrl& list, auto&& wanted) {
		for (int i = 0; i < list.GetItemCount(); i++)
			list.SetCheckState(i, wanted(static_cast<int>(list.GetItemData(i))));
	};
	switch (id) {
		case IDC_AN_MOVERS_ALL: setAll(m_Movers, [](int) { return true; }); break;
		case IDC_AN_TARGETS_ALL: setAll(m_Targets, [](int) { return true; }); break;
		case IDC_AN_MOVERS_STD:
		case IDC_AN_TARGETS_STD:
			setAll(id == IDC_AN_MOVERS_STD ? m_Movers : m_Targets, [&](int value) {
				return std::find(standard.begin(), standard.end(), static_cast<Planet>(value)) != standard.end();
			});
			break;
		case IDC_AN_ASPECTS_ALL: setAll(m_Aspects, [](int) { return true; }); break;
		case IDC_AN_ASPECTS_MAJOR: setAll(m_Aspects, [](int value) { return value <= static_cast<int>(AspectType::Opposition); }); break;
	}
	UpdateNote();
	return 0;
}

LRESULT CAnalysisDlg::OnEventKindChanged(WORD, WORD, HWND, BOOL&) {
	return 0;
}

LRESULT CAnalysisDlg::OnMoversChanged(int, LPNMHDR, BOOL&) {
	UpdateNote();
	return 0;
}

double CAnalysisDlg::EstimateSeconds(AnalysisSettings const& settings) {
	// measured in a Debug build: every planet as mover and target over a number of years, per year, per mover and per target;
	// the analyses add up
	double seconds = 0;
	for (auto type : settings.TypeList()) {
		AnalysisSettings one = settings;
		one.Type = type;
		double factor = 0.0033;
		switch (type) {
			case AnalysisType::TransitsToNatal: factor = 0.0033; break;
			case AnalysisType::TransitsToProgressed: factor = 0.0064; break;
			case AnalysisType::ProgressedToNatal: factor = 0.00012; break;
			case AnalysisType::SolarArcToNatal: factor = 0.00006; break;
			case AnalysisType::ProgressedToProgressed: factor = 0.00024; break;
		}
		double years = (settings.To.Julian() - settings.From.Julian()) / 365.25;
		auto references = settings.Targets.size() + (settings.NatalAngles && one.NatalTargets() ? 2 : 0) + 2;
		seconds += years * one.EffectiveMovers().size() * references * factor;
	}
	return seconds;
}

LRESULT CAnalysisDlg::OnOK(WORD, WORD, HWND, BOOL&) {
	if (m_Charts->empty())
		return EndDialog(IDCANCEL), 0;

	AnalysisSettings settings;
	settings.Types = SelectedTypes();
	if (settings.Types.empty())
		return Fail(IDC_AN_TYPE, L"Choose at least one analysis."), 0;
	settings.Type = settings.Types[0];
	if (!ReadDates(settings.From, settings.To))
		return 0;
	settings.Movers = Checked(m_Movers);
	settings.Targets = Checked(m_Targets);
	bool natal = false;
	for (auto type : settings.Types) {
		AnalysisSettings probe;
		probe.Type = type;
		natal |= probe.NatalTargets();
	}
	settings.NatalAngles = IsDlgButtonChecked(IDC_AN_ANGLES) == BST_CHECKED && natal;
	settings.AspectEvents = IsDlgButtonChecked(IDC_AN_ASPECTS) == BST_CHECKED;
	settings.HouseIngresses = IsDlgButtonChecked(IDC_AN_HOUSES) == BST_CHECKED && natal;
	settings.SignIngresses = IsDlgButtonChecked(IDC_AN_SIGNS) == BST_CHECKED;
	settings.Stations = IsDlgButtonChecked(IDC_AN_STATIONS) == BST_CHECKED;

	// the orbs are those of Options > Aspects (transit / overlay); the list says which aspects
	settings.Aspects = AspectOptions::Current().Transit;
	settings.Aspects.AspectEnabled.fill(false);
	settings.Aspects.MajorOnly = false;		// the list says which aspects
	for (int i = 0; i < m_Aspects.GetItemCount(); i++)
		settings.Aspects.AspectEnabled[m_Aspects.GetItemData(i)] = m_Aspects.GetCheckState(i) != 0;

	bool anyMover = false;		// (the Moon alone gives nothing to a long range of transits)
	for (auto type : settings.Types) {
		AnalysisSettings one = settings;
		one.Type = type;
		anyMover |= !one.EffectiveMovers().empty();
	}
	if (!anyMover)
		return Fail(IDC_AN_MOVERS, L"Choose at least one planet to move."), 0;
	if (!settings.AspectEvents && !settings.HouseIngresses && !settings.SignIngresses && !settings.Stations)
		return Fail(IDC_AN_ASPECTS, L"Choose at least one kind of event to list."), 0;
	if (settings.AspectEvents) {
		bool anyAspect = std::any_of(settings.Aspects.AspectEnabled.begin(), settings.Aspects.AspectEnabled.end(), [](bool on) { return on; });
		if (!anyAspect)
			return Fail(IDC_AN_ASPECTLIST, L"Choose at least one aspect, or turn the aspects off."), 0;
		if (settings.Targets.empty() && !settings.NatalAngles)
			return Fail(IDC_AN_TARGETS, L"Choose at least one planet for the aspects to be made to, or turn the aspects off."), 0;
	}

	if (double seconds = EstimateSeconds(settings); seconds > 6) {
		CString message;
		message.Format(L"This analysis may take about %d seconds, and the program waits until it is done. Continue?", (int)std::lround(seconds));
		if (AtlMessageBox(m_hWnd, (PCWSTR)message, L"Analysis", MB_YESNO | MB_ICONQUESTION) != IDYES)
			return 0;
	}

	m_Settings = settings;
	m_Chart = m_ChartBox.GetCurSel();
	AppSettings::Get().LastAnalysis(settings.ToText());
	EndDialog(IDOK);
	return 0;
}

LRESULT CAnalysisDlg::OnCancel(WORD, WORD, HWND, BOOL&) {
	EndDialog(IDCANCEL);
	return 0;
}
