#pragma once

#include "Interfaces.h"

#include <wx/listctrl.h>	// wxListCtrl, and wxItemAttr via listbase.h
#include <wx/dateevt.h>		// wxDateEvent
#include <wx/spinctrl.h>	// wxSpinCtrl, wxSpinEvent
#include <functional>

class wxDatePickerCtrl;
class wxTimePickerCtrl;

//
// Port of CChartDetailsView (AstroStudio\ChartDetailsView.h) and the
// IDD_CHARTDETAILS dialog resource.
//
// Notable differences from the WTL original:
//
//  * It is a wxPanel built from sizers, not a CDialogImpl over a dialog
//    template. wx cannot load a Win32 RC dialog, and the .rc used absolute
//    pixel coordinates, so the layout is rebuilt rather than transcribed -
//    which is where DPI and font independence come from.
//
//  * The five edit + msctls_updown32 pairs (IDC_LATDEG/IDC_LATDEGUD and
//    friends) collapse into single wxSpinCtrls.
//
//  * SysDateTimePick32 becomes wxDatePickerCtrl + wxTimePickerCtrl.
//
//  * Recalculation is a callback rather than SendMessage(WM_RECALC) to a
//    notify window.
//
class ChartDetailsView : public wxPanel {
public:
	using RecalcHandler = std::function<void(Recalc)>;

	ChartDetailsView(wxWindow* parent, RecalcHandler onRecalc);

	void SetChartData(ChartData* data);

	// Pushes the model back into the controls and refreshes the lists.
	// Equivalent to CChartDetailsView::UpdateControls.
	void UpdateControls(Recalc type = Recalc::All);

private:
	//
	// The two virtual report lists. One class serves both; which data it reads
	// is decided by Kind. Replaces CVirtualListView + the LVS_OWNERDATA
	// custom-draw handlers, since wx asks for text, colour and font through
	// overridables instead of NM_CUSTOMDRAW.
	//
	class DetailsList : public wxListCtrl {
	public:
		enum class Kind { Planets, Houses };

		DetailsList(ChartDetailsView* owner, Kind kind, wxWindow* parent);

	protected:
		wxString OnGetItemText(long item, long column) const override;
		wxItemAttr* OnGetItemColumnAttr(long item, long column) const override;

	private:
		ChartDetailsView* m_Owner;
		Kind m_Kind;
		mutable wxItemAttr m_Attr;	// returned by pointer, so it has to outlive the call
	};

	friend class DetailsList;

	void BuildControls();
	void UpdateLocationControls();
	void ApplyLocationFromControls();
	void Recalculate(Recalc what);

	void OnHouseSystemChanged(wxCommandEvent& e);
	void OnDateTimeChanged(wxDateEvent& e);
	void OnHarmonicChanged(wxSpinEvent& e);
	void OnNow(wxCommandEvent& e);
	void OnApply(wxCommandEvent& e);
	void OnLocationChanged(wxCommandEvent& e);

	wxString PlanetCellText(long row, long col) const;
	wxString HouseCellText(long row, long col) const;

	ChartData* m_Data{};
	RecalcHandler m_OnRecalc;
	std::vector<PlanetPosition> m_Planets;

	wxTextCtrl* m_Name{};
	wxTextCtrl* m_Location{};
	wxButton* m_Lookup{};
	wxButton* m_Here{};
	wxButton* m_Now{};
	wxButton* m_Apply{};
	wxSpinCtrl* m_LatDeg{};
	wxSpinCtrl* m_LatMin{};
	wxSpinCtrl* m_LonDeg{};
	wxSpinCtrl* m_LonMin{};
	wxRadioButton* m_North{};
	wxRadioButton* m_South{};
	wxRadioButton* m_East{};
	wxRadioButton* m_West{};
	wxChoice* m_HouseSystem{};
	wxDatePickerCtrl* m_Date{};
	wxTimePickerCtrl* m_Time{};
	wxSpinCtrl* m_Harmonic{};
	DetailsList* m_PlanetList{};
	DetailsList* m_HouseList{};

	wxFont m_GlyphFont;

	// What the House System combo is currently showing, so UpdateControls can
	// skip re-asserting it. See the note in UpdateControls.
	HouseSystem m_ShownHouseSystem{ static_cast<HouseSystem>(0) };

	//
	// Guards the whole of UpdateControls, not just the location fields.
	//
	// The WTL version only guarded location (m_UpdatingLocationControls);
	// setting the date/time pickers programmatically also raises a change
	// notification, so OnDateChanged -> recalc -> UpdateControls -> SetSystemTime
	// could re-enter. It terminated only because the value stopped changing.
	// One guard over all programmatic updates removes the whole class of
	// problem.
	//
	bool m_Updating{ false };
};
