#pragma once

//
// Replaces the CAppModule / CMessageLoop / _tWinMain scaffolding in
// AstroStudio\AstroStudio.cpp. wxIMPLEMENT_APP generates the entry point and
// runs the message loop, so there is no global module object to maintain.
//
class AstroApp : public wxApp {
public:
	bool OnInit() override;
};

wxDECLARE_APP(AstroApp);
