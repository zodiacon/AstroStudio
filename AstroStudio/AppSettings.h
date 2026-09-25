#pragma once

#include <Settings.h>
#include "TimeStep.h"

// What the program remembers between runs, in HKCU\Software\AstroStudio\Settings (or in AstroStudio.ini next to the
// program, if there is one - see Settings::Load). Loaded at startup and saved when the main window closes; the parts of the
// program change them as the user does, and read them when they are created.
struct AppSettings : Settings {
	static constexpr PCWSTR RegistryKey = L"Software\\AstroStudio\\Settings";

	BEGIN_SETTINGS(AppSettings)
		SETTING(MainWindowPlacement, WINDOWPLACEMENT{}, SettingType::Binary);
		SETTING(DarkMode, -1, SettingType::Int32);		// 1 dark, 0 light, -1 (never chosen): as the system is
		SETTING(ViewStatusBar, 1, SettingType::Bool);
		SETTING(AlwaysOnTop, 0, SettingType::Bool);
		SETTING(TextFont, LOGFONT{}, SettingType::Binary);		// the font for text that is not astrological symbols (empty face: Consolas); lfHeight in tenths of a point
		SETTING(ShowPartOfFortune, 0, SettingType::Bool);		// the charts have the Part of Fortune among their planets (and on the wheel)
		SETTING(LastHouseSystem, (int)'K', SettingType::Int32);		// the last one used for a chart (Koch to begin with)

		// the chart view's Step, Auto and Live controls
		SETTING(ChartStepCount, 1, SettingType::Int32);
		SETTING(ChartStepUnit, (int)StepUnit::Day, SettingType::Int32);
		SETTING(ChartStepInterval, 1000, SettingType::Int32);		// milliseconds

		// the ephemeris (its start date is not kept: it starts a month before today)
		SETTING(EphemerisGlyphs, 1, SettingType::Bool);
		SETTING_STRING(LastAnalysis, L"");		// what the last analysis was made of (AnalysisSettings::ToText), for the next one to start from
		SETTING(AnalysisFontSize, 90, SettingType::Int32);		// the analysis view's text size, in tenths of a point
		SETTING(AnalysisGlyphs, 1, SettingType::Bool);		// the analysis view shows glyphs rather than names
		SETTING(EphemerisSeconds, 0, SettingType::Bool);
		SETTING(EphemerisGridLines, 0, SettingType::Bool);
		SETTING(EphemerisFontSize, 100, SettingType::Int32);
		SETTING(EphemerisStep, 1, SettingType::Int32);		// days between rows
		SETTING(EphemerisEclipses, 0, SettingType::Bool);
		SETTING(EphemerisVoid, 0, SettingType::Bool);
		SETTING_STRING(AspectSets, L"");		// the aspect settings (AspectOptions), as INI text; empty: the defaults
		SETTING_STRING(WheelOptions, L"");		// what the chart wheel draws (WheelOptions), as text; empty: everything
		SETTING_STRING(ChartColors, L"");		// the user's colours for the chart wheel (ChartColors), as text; empty: the defaults
		SETTING_STRING(PrintMargins, L"");		// the print margins in thousandths of an inch (left, top, right, bottom); empty: 0.75 inch all round
		SETTING_STRING(EphemerisBodies, L"");		// the numbers of the bodies (Planet), comma separated; empty: the usual ones
	END_SETTINGS

	DEF_SETTING(MainWindowPlacement, WINDOWPLACEMENT)
	DEF_SETTING(DarkMode, int)
	DEF_SETTING(ViewStatusBar, int)
	DEF_SETTING(AlwaysOnTop, int)
	DEF_SETTING(TextFont, LOGFONT)
	DEF_SETTING(LastHouseSystem, int)
	DEF_SETTING(ShowPartOfFortune, int)
	DEF_SETTING(ChartStepCount, int)
	DEF_SETTING(ChartStepUnit, int)
	DEF_SETTING(ChartStepInterval, int)
	DEF_SETTING(EphemerisGlyphs, int)
	DEF_SETTING(AnalysisFontSize, int)
	DEF_SETTING_STRING(LastAnalysis)
	DEF_SETTING(AnalysisGlyphs, int)
	DEF_SETTING(EphemerisSeconds, int)
	DEF_SETTING(EphemerisGridLines, int)
	DEF_SETTING(EphemerisFontSize, int)
	DEF_SETTING(EphemerisStep, int)
	DEF_SETTING(EphemerisEclipses, int)
	DEF_SETTING(EphemerisVoid, int)
	DEF_SETTING_STRING(AspectSets)
	DEF_SETTING_STRING(EphemerisBodies)
	DEF_SETTING_STRING(ChartColors)
	DEF_SETTING_STRING(WheelOptions)
	DEF_SETTING_STRING(PrintMargins)
};
