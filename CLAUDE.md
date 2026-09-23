# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

AstroStudio is a native Windows desktop astrology application built with C++20, WTL (Windows Template Library), and Direct2D/DirectWrite. It calculates and draws natal/chart data (planets, houses, aspects) using the Swiss Ephemeris library.

## Build

This is a Visual Studio solution (`AstroStudio.sln`), not a CMake/cross-platform project. Build with MSBuild or Visual Studio 2022+ (toolset v145, targets Win32/x64/ARM64, `stdcpp20`).

```
msbuild AstroStudio.sln /p:Configuration=Debug /p:Platform=x64
```

There are no automated tests, lint configs, or CI workflows in this repo — verify changes by building and running the app (`x64\Debug\AstroStudio.exe`).

### Submodules

`WTLHelper` is a git submodule (https://github.com/zodiacon/WTLHelper). Run `git submodule update --init` after cloning if it's empty. Note: `.gitmodules` also lists `modules/CairoGfx`, but the `CairoGfx` directory at the repo root is actually committed directly to this repo (not a submodule) — the `.gitmodules` entry for it is stale.

## Solution structure

The `.sln` builds four projects: `AstroStudio` (the app), `AstroCore` (calculation engine), `sweph` (Swiss Ephemeris, vendored C library), and `WTLHelper` (submodule, shared WTL UI helper library used across the author's other projects). The `CairoGfx` and `QChart` directories exist in the repo but are **not** part of the solution — they're unused/legacy alternative rendering backends; don't assume they're wired into the build.

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
  - `GraphicChartView` (`CGraphicChartView`) — renders the circular chart wheel via `D2DChartDrawing` onto its own `ID2D1HwndRenderTarget`.
  - `D2DChartDrawing` — Direct2D/DirectWrite drawing logic for the chart wheel (zodiac belt, houses, aspect lines) in a 1000x1000 logical space, parameterized by `ChartDrawingParameters` (colors, widths, toggles). `Draw` does not call `BeginDraw`/`EndDraw`; the owning view does.
  - `AspectGridWnd` / `AspectGridDrawing` — the aspect grid tab, drawn the same way (its own render target; `AspectGridDrawing::Draw` also leaves `BeginDraw`/`EndDraw` to the caller).
  - `D2DResources` — process-wide singleton (`D2DResources::Get().Ensure()`) holding everything device-independent: the D2D and DirectWrite factories, the embedded glyph-font collection and the text formats. Only render targets are per window (`CreateWindowRenderTarget`, fixed at 96 DPI so a DIP is a pixel). The project targets Windows 7 in `pch.h`, which hides the DirectWrite in-memory font loader; `D2DResources.h` raises `NTDDI_VERSION` around its d2d1/dwrite_3 includes only. There is no GDI+ anywhere in the app — don't reintroduce it.
  - `EphemerisView` — ephemeris/table-style view.
  - `Interfaces.h` — cross-view contracts: `IMainFrame` (what views can call on the frame) and `IView` (what the frame can call on a tab page: `PageActivated`, `ProcessCommand`). New views should implement `IView`; new frame-level operations should be added to `IMainFrame`.
  - `ViewBase.h` (`CViewBase<T, TBase>`) — CRTP base every tab view derives from; wires up `IView::PageActivated`, idle-driven toolbar UI updates (`CAutoUpdateUI`), and self-deletion on `WM_DESTROY`/`OnFinalMessage`. New tab views should derive from this rather than reimplementing the plumbing.
  - `NetworkHelper` / `WinHttp` — WinHTTP-based lookups (e.g. external IP, geocoding-ish info) used to fill in `ChartInfo` (location/timezone) for "chart for now" style birth data.
  - `AstroFont` / `DefaultFont` — embeds/loads `res\HamburgSymbols.ttf` (the zodiac/planet glyph font) so it renders without being installed system-wide — see commit history for why (avoids requiring a global font install).
    - **`Helpers::LoadAstroFont` loading the font twice is not redundant.** `AddFontMemResourceEx` registers the face with GDI, which is what `CFont`/`CreatePointFont` lookups by name need; DirectWrite cannot see that registration, so `D2DResources` builds its own font collection from the same embedded resource (in-memory font loader + font set builder). Don't "simplify" either away.
    - The glyph tables in `DefaultFont.cpp` must cover their whole enum. `Planet` runs to Vesta (21) while HamburgSymbols originally stopped at Chiron (16), so the last five read past the end. A `static_assert` now ties the planet table's length to `Planet::NumPlanets`, so adding a body fails to compile until it has a glyph. Codes come from the "INDEX #" column of `HamburgSymbols.pdf`.
  - `PlanetSpacer` — layout helper to avoid overlapping planet glyphs when several planets cluster together on the wheel.

## Conventions

- Views communicate with the main frame only through `IMainFrame`/`IView`, not concrete types — keep new cross-view interactions going through those interfaces.
- `AstroCore` must stay free of WTL/Direct2D/UI includes; calculation logic belongs there, drawing/UI logic belongs in `AstroStudio`.
- Dark mode is handled via `WTLHelper`'s `WTLHelper::InitDarkMode()` / `ThemeHelper`; the app toggles it at runtime (`ID_OPTIONS_DARKMODE`) — UI code should get colors from the theme helpers rather than hardcoding them where dark-mode correctness matters.
