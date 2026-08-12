#pragma once

//
// Resource / command IDs for the wxWidgets build of AstroStudio.
//
// ==========================================================================
// ID range audit
// ==========================================================================
//
// wxWidgets constrains menu/tool command IDs in TWO independent ways, and the
// WTL ids satisfy only the first:
//
//  1. They must avoid 4999-5999 (wxID_LOWEST..wxID_HIGHEST), which wx reserves
//     for its own standard ids. The WTL ids are all clear of this.
//
//  2. They must satisfy 0 <= id < 32767 (asserted in wxMenuItemBase, see
//     src/common/menucmn.cpp). This is not arbitrary: a command id travels in
//     the low word of WM_COMMAND's wParam, so anything at or above 32767 is
//     ambiguous once interpreted as a signed 16-bit value.
//
// Visual Studio's resource editor allocates generated command ids from 32771
// upwards (_APS_NEXT_COMMAND_VALUE), so *every* ID_* command in
// AstroStudio\resource.h - 32772 through 32787 - violates rule 2 by a few
// dozen. They cannot be carried over and are renumbered below.
//
// The ids that CAN be carried over verbatim are the low ones: ID_VIEW_RETRO
// through ID_VIEW_GRIDLINES at 101-106. They are kept at their original values
// purely so diffs against the WTL views stay readable during the port; they
// are equally valid anywhere in the range.
//
// Everything renumbered lands in 6000-6199: above wxID_HIGHEST, far below
// 32767, and contiguous so it is obvious at a glance that the block is ours.
//
// Also note: AstroStudio\resource.h:52-53 defines IDC_HERE2 and IDC_APPLY with
// the same value (1017). IDC_HERE2 looks like a leftover; not carried over.
//

//
// Commands that map onto a wx standard ID. wx supplies the label, the
// accelerator and the platform-correct placement for these.
//
//   ID_APP_EXIT           -> wxID_EXIT
//   ID_APP_ABOUT          -> wxID_ABOUT
//   ID_FILE_OPEN          -> wxID_OPEN
//   ID_FILE_SAVE          -> wxID_SAVE
//   ID_FILE_SAVE_AS       -> wxID_SAVEAS
//   ID_FILE_PRINT         -> wxID_PRINT
//   ID_FILE_PRINT_PREVIEW -> wxID_PREVIEW
//   ID_FILE_PRINT_SETUP   -> wxID_PRINT_SETUP
//   ID_EDIT_UNDO          -> wxID_UNDO
//   ID_EDIT_CUT           -> wxID_CUT
//   ID_EDIT_COPY          -> wxID_COPY
//   ID_EDIT_PASTE         -> wxID_PASTE
//

// Frame commands. Renumbered from 32772-32790; see rule 2 above.
#define ID_WINDOW_CLOSE                 6000
#define ID_WINDOW_CLOSE_ALL             6001
#define ID_OPTIONS_ALWAYSONTOP          6002
#define ID_TOOL_EPHEMERIS               6003
#define ID_OPTIONS_FONT                 6004
#define ID_NEW_CHART                    6005
#define ID_NEW_CHARTFORNOW              6006
#define ID_OPTIONS_DARKMODE             6007
#define ID_VIEW_STATUS_BAR              6008

//
// View-level commands. Not used by the frame; listed here so the ephemeris and
// chart views have their ids settled when they are ported in later phases.
// The 101-106 group keeps its original WTL values (legal under both rules);
// ID_VIEW_GLYPHS was 32777 and had to move.
//
#define ID_VIEW_RETRO                   101
#define ID_VIEW_SECONDS                 102
#define ID_FONT_BIGGER                  103
#define ID_FONT_SMALLER                 104
#define ID_FONT_SIZE_DEFAULT            105
#define ID_VIEW_GRIDLINES               106
#define ID_VIEW_GLYPHS                  6020

//
// Dynamic "Window" menu entries, one per open tab. Replaces WTL's
// ID_WINDOW_TABFIRST/ID_WINDOW_TABLAST range from atlres.h.
//
#define ID_WINDOW_TABFIRST              6100
#define ID_WINDOW_TABLAST               6199

//
// Named icon resources (see AstroStudioWx.rc). These are strings rather than
// numbers because both wxICON() and wxIconBundle look icons up by name.
//
#define ICON_APP        "appicon"
#define ICON_EPHEMERIS  "ephemeris"
#define ICON_CHART      "chart"
#define ICON_CHARTNOW   "chartnow"
