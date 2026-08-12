# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

AstroStudio is a native Windows desktop astrology application built with C++20, WTL (Windows Template Library), and GDI+. It calculates and draws natal/chart data (planets, houses, aspects) using the Swiss Ephemeris library.

## Build

This is a Visual Studio solution (`AstroStudio.sln`), not a CMake/cross-platform project. Build with MSBuild or Visual Studio 2022+ (toolset v145, targets Win32/x64/ARM64, `stdcpp20`).

```
msbuild AstroStudio.sln /p:Configuration=Debug /p:Platform=x64
```

There are no automated tests, lint configs, or CI workflows in this repo — verify changes by building and running the app (`x64\Debug\AstroStudio.exe`).

The `AstroStudioWx` project additionally needs wxWidgets from vcpkg:

```
vcpkg install wxwidgets:x64-windows-static
```

The **static** triplet is required, not preferred — every project here builds with the static CRT (`/MT`, `/MTd`), and the default `x64-windows` triplet is `/MD`. `AstroStudioWx\wxwidgets-vcpkg.props` locates it via `VCPKG_ROOT` (defaulting to `C:\vcpkg`) and fails with a readable message if it's missing. That project is x64-only; x86 solution builds skip it.

### Submodules

`WTLHelper` is a git submodule (https://github.com/zodiacon/WTLHelper). Run `git submodule update --init` after cloning if it's empty. Note: `.gitmodules` also lists `modules/CairoGfx`, but the `CairoGfx` directory at the repo root is actually committed directly to this repo (not a submodule) — the `.gitmodules` entry for it is stale.

## Solution structure

The `.sln` builds five projects: `AstroStudio` (the WTL app), `AstroStudioWx` (the in-progress wxWidgets port of the same app — see below), `AstroCore` (calculation engine), `sweph` (Swiss Ephemeris, vendored C library), and `WTLHelper` (submodule, shared WTL UI helper library used across the author's other projects). The `CairoGfx` and `QChart` directories exist in the repo but are **not** part of the solution — they're unused/legacy alternative rendering backends; don't assume they're wired into the build.

- **`sweph/`** — Vendored Swiss Ephemeris C sources (`swe*.c/h`). Treat as third-party; avoid modifying unless fixing a vendoring issue.
- **`AstroCore/`** — Platform-agnostic astrology calculation engine, no UI/WTL dependencies.
  - `DateTime` — Julian day based date/time type (Gregorian/Julian calendar aware).
  - `AstroPoint` — zodiac position type; `Planet`, `ZodiacSign` enums.
  - `AstroCalculator` — thin wrapper over `sweph` for planet positions, house cusps, ingresses, stations.
  - `ChartData` — the aggregate for one chart: planets, houses, harmonic, `ChartInfo` (person/event birth data). `CalcHouses`/`CalcPlanets` populate it via an `AstroCalculator`.
  - `Aspects.h` — `AspectCalculator` computes aspects (conjunction, trine, square, ...) between planet positions from `AspectSettings` (orbs).
- **`AstroStudio/`** — The WTL application.
  - `MainFrm` (`IMainFrame`) — main frame window; owns a `CNativeCustomTabView` hosting one tab per open chart. Creates new chart views via `AddChartView`.
  - `ChartView` (`CChartView`) — one chart's tab content; splits between `ChartDetailsView` (text/list data) and `GraphicChartView`.
  - `GraphicChartView` (`CGraphicChartView`) — renders the circular chart wheel via `ChartDrawing` onto a `Gdiplus::Bitmap`.
  - `ChartDrawing` — pure GDI+ drawing logic for the chart wheel (zodiac belt, houses, aspect lines), parameterized by `ChartDrawingParameters` (colors, widths, toggles).
  - `EphemerisView` — ephemeris/table-style view.
  - `Interfaces.h` — cross-view contracts: `IMainFrame` (what views can call on the frame) and `IView` (what the frame can call on a tab page: `PageActivated`, `ProcessCommand`). New views should implement `IView`; new frame-level operations should be added to `IMainFrame`.
  - `ViewBase.h` (`CViewBase<T, TBase>`) — CRTP base every tab view derives from; wires up `IView::PageActivated`, idle-driven toolbar UI updates (`CAutoUpdateUI`), and self-deletion on `WM_DESTROY`/`OnFinalMessage`. New tab views should derive from this rather than reimplementing the plumbing.
  - `NetworkHelper` / `WinHttp` — WinHTTP-based lookups (e.g. external IP, geocoding-ish info) used to fill in `ChartInfo` (location/timezone) for "chart for now" style birth data.
  - `AstroFont` / `DefaultFont` — embeds/loads `res\HamburgSymbols.ttf` (the zodiac/planet glyph font) via a private font collection so it renders without being installed system-wide — see commit history for why (avoids requiring a global font install).
  - `PlanetSpacer` — layout helper to avoid overlapping planet glyphs when several planets cluster together on the wheel.
- **`AstroStudioWx/`** — In-progress wxWidgets 3.3 port of the application, built as a second executable alongside the WTL one so both run during the migration. It links `AstroCore`/`sweph` but shares no code with `AstroStudio/` yet — those files are ATL/WTL-bound (`CString` returns, `Gdiplus::FontFamily`) and get ported phase by phase. x64 only; see `wxwidgets-vcpkg.props` for why, and for the vcpkg autolink conflict that sheet pins down.
  - `App` — `wxIMPLEMENT_APP`; replaces the `CAppModule`/`CMessageLoop`/`_tWinMain` scaffolding and the `WTLHelper::InitDarkMode` call.
  - `MainFrame` — replaces `CMainFrame`: menu bar, tool bar, status bar, and a `wxAuiNotebook` in place of `CNativeCustomTabView`. UI enable/check state comes from `wxEVT_UPDATE_UI` instead of `CAutoUpdateUI` + `CIdleHandler`.
  - `Interfaces.h` — `IView` (a mixin recovered from notebook pages with `dynamic_cast`) and `IMainFrame`, down from six methods to two. Also the `Recalc` enum, which replaces the `WM_RECALC` custom message. The header explains each omission.
  - `ChartView` — replaces `CChartView`: a `wxSplitterWindow` (wheel left, detail notebook right) with Details / Aspect Grid / Aspect List pages.
  - `GraphicChartView` / `AspectGridView` — hosts for the two drawings. `wxAutoBufferedPaintDC` replaces the manual `Gdiplus::Bitmap` backbuffer and `WM_ERASEBKGND` handler.
  - `ChartDrawing` / `AspectGridDrawing` — ports of the GDI+ originals onto `wxGraphicsContext`, keeping the same 1000-unit logical space and geometry. Three colours the WTL code hard-coded (`Color::Black`, `Color::Gray`, `Color::Blue`) are parameters here, so the wheel can be themed without editing drawing code.
  - **Text cannot go through `wxGraphicsContext`.** GDI+ ignores private fonts entirely — both `AddFontMemResourceEx` and file-based `FR_PRIVATE` — and silently substitutes Microsoft Sans Serif, so HamburgSymbols renders as plain Latin letters. Its only route to a private face is a `Gdiplus::PrivateFontCollection` (which is why `AstroStudio\Helpers.cpp` keeps one *in addition to* its `AddFontMemResourceEx` call), and wx exposes no way to supply one. So shapes are drawn through the graphics context and glyphs are collected into `AstroHelpers::GlyphRun`s and drawn afterwards with `wxDC::DrawText`, which is GDI and resolves the face. Do not "simplify" this back into the graphics context.
  - `PlanetSpacer` — the one UI-layer file that ports verbatim; it is pure geometry.
  - `AspectGridView` — replaces `CAspectGridWnd` **and** its `CScrollContainer` host, plus the `UpdateAspectGridScrollSize`/`UpdateAspectGridScrollBarTheme` plumbing in `CChartView`. One `wxScrolledWindow` does all of it.
  - `AstroHelpers` — the portable subset of `Helpers`, ported per-phase. The rest of `Helpers` is ATL/GDI+-bound (`CString`, `Gdiplus::FontFamily`, `COLORREF`) and cannot be shared.
  - `Resources` — icon loading via `wxIconBundle`/`wxBitmapBundle`, which picks the right size per monitor DPI. Icons are shared with the WTL build's `res\*.ico` via *named* resources in `AstroStudioWx.rc` (both `wxICON()` and `wxIconBundle` look icons up by name, not number).
  - `PlaceholderView` — temporary stub page; replaced by `CChartView` (phase 2) and `CEphemerisView` (phase 5).
  - **Command IDs are renumbered, not carried over.** wx asserts `id < 32767` for menu items, and Visual Studio's resource editor allocates from 32771 up, so every `ID_*` in `AstroStudio\resource.h` is illegal in wx. `AstroStudioWx\resource.h` documents the mapping.

## Conventions

- Views communicate with the main frame only through `IMainFrame`/`IView`, not concrete types — keep new cross-view interactions going through those interfaces.
- `AstroCore` must stay free of WTL/GDI+/UI includes; calculation logic belongs there, drawing/UI logic belongs in `AstroStudio`.
- Dark mode is handled via `WTLHelper`'s `WTLHelper::InitDarkMode()` / `ThemeHelper`; the app toggles it at runtime (`ID_OPTIONS_DARKMODE`) — UI code should get colors from the theme helpers rather than hardcoding them where dark-mode correctness matters.
