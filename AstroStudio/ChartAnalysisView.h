#pragma once

#include "AnalysisReport.h"
#include "DpiAware.h"
#include <functional>

// The structure of the chart (a tab of the chart view): its aspect patterns, the balance of elements, modalities and hemispheres,
// the dignities, receptions and dispositors (ChartAnalysis), as a tree of sections. The rulers (traditional or modern) and the
// weighting are chosen at the top. Selecting an item that names planets marks them on the wheel (through Highlight).
class CChartAnalysisView :
	public CWindowImpl<CChartAnalysisView>,
	public CDpiAware<CChartAnalysisView> {
public:
	DECLARE_WND_CLASS(L"AstroStudioChartAnalysis")

	// Works the analysis out for the chart, with the aspects by these settings, and shows it; null (or no planets): nothing.
	// The chart must outlive its use here.
	void SetChartData(ChartData const* chart, AspectSettings const& settings);
	void ApplyTextFont();
	void OnDpiChanged(UINT dpi);
	// called with the planets of the item selected (empty when there are none, or the selection goes)
	std::function<void(std::vector<Planet> const&)> Highlight;

	// the analysis as it is now, in words: for the report, Export and Print
	std::vector<AnalysisReport::Section> const& Sections() const {
		return m_Sections;
	}
	Helpers::TableSource Table() const {
		return AnalysisReport::ToTable(m_Sections);
	}

	BEGIN_MSG_MAP(CChartAnalysisView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CDpiAware<CChartAnalysisView>)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_SHOWWINDOW, OnShowWindow)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColorStatic)
		MESSAGE_HANDLER(WM_CTLCOLORBTN, OnCtlColorStatic)
		COMMAND_HANDLER(IDC_CA_RULERS, CBN_SELCHANGE, OnChoice)
		COMMAND_HANDLER(IDC_CA_WEIGHTED, BN_CLICKED, OnChoice)
		NOTIFY_HANDLER(IDC_CA_TREE, TVN_SELCHANGED, OnSelChanged)
	END_MSG_MAP()

private:
	enum { IDC_CA_RULERS = 7101, IDC_CA_WEIGHTED, IDC_CA_TREE };

	LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSetFocus(UINT, WPARAM, LPARAM, BOOL&);
	// the wheel marks the selected item's planets only while the tab shows
	LRESULT OnShowWindow(UINT, WPARAM, LPARAM, BOOL&);
	void HighlightSelected();
	LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnCtlColorStatic(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnChoice(WORD, WORD, HWND, BOOL&);
	LRESULT OnSelChanged(int, LPNMHDR, BOOL&);
	void ApplyUiFont();
	void Layout();
	void Rebuild();
	ChartAnalysisOptions Options() const;
	// where an item is in the tree, by index at every level (so the same item can be selected again after the tree is made anew)
	std::vector<int> PathOf(HTREEITEM item) const;
	HTREEITEM ItemAt(std::vector<int> const& path) const;

	ChartData const* m_Data{ nullptr };
	AspectSettings m_Settings;
	std::vector<AnalysisReport::Section> m_Sections;
	std::vector<std::vector<Planet>> m_ItemPlanets;		// by the tree items' lParam
	CStatic m_RulersLabel;
	CComboBox m_Rulers;
	CButton m_Weighted;
	CTreeViewCtrl m_Tree;
	CFont m_UiFont, m_TextFont;
	bool m_HasTextFont{ false };
	bool m_Rebuilding{ false };
};
