#include "pch.h"
#include "App.h"
#include "MainFrame.h"

wxIMPLEMENT_APP(AstroApp);

bool AstroApp::OnInit() {
	if (!wxApp::OnInit())
		return false;

	SetAppName("AstroStudio");
	SetVendorName("Pavel Yosifovich");

	//
	// The whole of WTLHelper's dark mode support - InitDarkMode(),
	// SwitchToMode(), DarkModeSubclass, the per-control setDarkWndNotifySafe
	// calls and the setDarkScrollBar workaround in ChartView - collapses into
	// this. MSW is the one platform where wx does not follow the system
	// appearance unless asked (see the comment on wxApp::Appearance).
	//
	// The result is deliberately not treated as fatal: CannotChange just means
	// the app runs in the default light appearance.
	//
	SetAppearance(Appearance::System);

	auto frame = new MainFrame();
	frame->Show();

	return true;
}
