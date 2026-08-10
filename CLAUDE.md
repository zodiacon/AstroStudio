# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

AstroStudio is a native Windows desktop astrology application built with C++20, WTL (Windows Template Library), and GDI+. It calculates and draws natal/chart data (planets, houses, aspects) using the Swiss Ephemeris library.

## Build

This is a Visual Studio solution (`AstroStudio.sln`), not a CMake/cross-platform project. Build with MSBuild or Visual Studio 2022+ (toolset v145, targets Win32/x64/ARM64, `stdcpp20`).

```
msbuild AstroStudio.sln /p:Configuration=Debug /p:Platform=x64
```

There are no automated tests, lint configs, or CI workflows in this repo — verify changes by building and running the app (`AstroStudio\x64\Debug\AstroStudio.exe`).

### Submodules

`WTLHelper` is a git submodule (https://github.com/zodiacon/WTLHelper). Run `git submodule update --init` after cloning if it's empty. Note: `.gitmodules` also lists `modules/CairoGfx`, but the `CairoGfx` directory at the repo root is actually committed directly to this repo (not a submodule) — the `.gitmodules` entry for it is stale.

## Solution structure

The `.sln` only builds four projects: `AstroStudio` (the app), `AstroCore` (calculation engine), `sweph` (Swiss Ephemeris, vendored C library), and `WTLHelper` (submodule, shared WTL UI helper library used across the author's other projects). The `CairoGfx` and `QChart` directories exist in the repo but are **not** part of the solution — they're unused/legacy alternative rendering backends; don't assume they're wired into the build.

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

## Conventions

- Views communicate with the main frame only through `IMainFrame`/`IView`, not concrete types — keep new cross-view interactions going through those interfaces.
- `AstroCore` must stay free of WTL/GDI+/UI includes; calculation logic belongs there, drawing/UI logic belongs in `AstroStudio`.
- Dark mode is handled via `WTLHelper`'s `WTLHelper::InitDarkMode()` / `ThemeHelper`; the app toggles it at runtime (`ID_OPTIONS_DARKMODE`) — UI code should get colors from the theme helpers rather than hardcoding them where dark-mode correctness matters.
