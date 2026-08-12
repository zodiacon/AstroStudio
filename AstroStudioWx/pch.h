#pragma once

// Kept from the WTL pch.h: AstroStudio code uses std::min/std::max (e.g. the
// chart sizing in GraphicChartView), which the windows.h macros break.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <memory>
#include <vector>
#include <string>
#include <algorithm>
