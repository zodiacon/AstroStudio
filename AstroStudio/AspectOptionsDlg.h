#pragma once

#include <DialogHelper.h>
#include "resource.h"
#include "AspectOptions.h"

// The "Aspects" dialog: which aspects count and with what orb, which planets take part and with what extra orb - for the
// aspects within a chart and, separately, for transits - and Load / Save of such a set of settings to a file.
class CAspectOptionsDlg :
	public CDialogImpl<CAspectOptionsDlg>,
	public CDialogHelper<CAspectOptionsDlg> {
public:
	enum { IDD = IDD_ASPECTOPTIONS };

	void SetOptions(AspectOptions const& options);
	// valid after DoModal returned IDOK
	AspectOptions const& GetOptions() const;

	BEGIN_MSG_MAP(CAspectOptionsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_ASP_LOAD, OnLoad)
		COMMAND_ID_HANDLER(IDC_ASP_SAVE, OnSave)
		COMMAND_ID_HANDLER(IDC_ASP_DEFAULTS, OnDefaults)
		COMMAND_HANDLER(IDC_ASP_SET, CBN_SELCHANGE, OnSetChanged)
		COMMAND_HANDLER(IDC_ASP_MAJORORB, EN_CHANGE, OnGeneralChanged)
		COMMAND_HANDLER(IDC_ASP_MINORORB, EN_CHANGE, OnGeneralChanged)
		COMMAND_HANDLER(IDC_ASP_MAJORONLY, BN_CLICKED, OnGeneralChanged)
		COMMAND_HANDLER(IDC_ASP_ORB, EN_CHANGE, OnAspectOrbChanged)
		COMMAND_HANDLER(IDC_ASP_EXTRA, EN_CHANGE, OnPlanetExtraChanged)
		NOTIFY_HANDLER(IDC_ASP_ASPECTS, LVN_ITEMCHANGED, OnAspectItemChanged)
		NOTIFY_HANDLER(IDC_ASP_PLANETS, LVN_ITEMCHANGED, OnPlanetItemChanged)
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLoad(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDefaults(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSetChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnGeneralChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAspectOrbChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPlanetExtraChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAspectItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnPlanetItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

	// the set being edited: the chart's aspects or the transits'
	AspectSettings& Current() {
		return m_Set == 0 ? m_Options.Chart : m_Options.Transit;
	}
	// puts the current set into the controls
	void ShowSet();
	void FillAspects();
	void FillPlanets();
	// the orb column of the aspect list, after the general orbs or one aspect's own changed
	void RefreshAspectOrbs();
	void RefreshPlanetExtra(int item);
	static bool ParseOrb(CString text, float& orb);
	// the general orbs typed in, if they are fit to use
	bool ReadGeneral(AspectSettings& settings, UINT& badControl);
	bool Fail(UINT control, PCWSTR message);

	AspectOptions m_Options;
	int m_Set{ 0 };
	bool m_Loading{ false };		// the controls are being filled in: what they report isn't the user's doing
	CListViewCtrl m_Aspects, m_Planets;
	CComboBox m_SetCombo;
};
