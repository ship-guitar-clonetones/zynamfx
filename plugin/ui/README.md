# Native UI (`plugin/ui`)

The plugin's editor, in JUCE/C++ (`namespace t3k::ui`). It is a
pixel-for-pixel, feature-for-feature port of the React app in `ui/`, which
stays in the tree as the QA reference. The design and the decisions behind
it are in [`../docs/native-ui.md`](../docs/native-ui.md); this file is the
working guide: how to build it, how to test it, where things go.

## Build

The default plugin build (`-DT3K_NATIVE_UI=ON`) compiles this tree into every
format; nothing extra to install. `-DT3K_NATIVE_UI=OFF` builds the legacy
webview editor instead (see [`../../ui/README.md`](../../ui/README.md)).

Configuration (`VITE_T3K_PUBLISHABLE_KEY`, `VITE_T3K_API_DOMAIN`,
`VITE_T3K_UPDATE_NOTICE`, `VITE_T3K_PREVIEW`) is read at CMake configure time
from `ui/.env` and `ui/.env.local`, with the configure environment overriding
both, into the generated `T3kConfig.h` (`NativeUi.cmake`). One file
configures both UIs.

### The testbed

`testbed/` is a standalone JUCE app that renders `PluginRoot` over a
fixture-driven mock backend and mock TONE3000 session. Its own project keeps
UI iteration from rebuilding the DSP:

```sh
cmake -S plugin/ui/testbed -B build-ui -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ui -j
UI=build-ui/UiTestbed_artefacts/Debug/UiTestbed.app/Contents/MacOS/UiTestbed   # macOS path

$UI --scenario main-mono                     # interactive window on one scenario
$UI --capture out [--ref refDir] [filter…]   # 2x PNG per scenario (+ mismatch table)
$UI --compare ref.png out.png [diff.png]     # one pair
$UI --selftest                               # unit tests for the pure logic
```

`-DT3K_BUILD_UI_TESTBED=ON` on the plugin build adds the same target and
registers `--selftest` with ctest (CI runs it next to the DSP suite).

Scenario ids come from `testbed/fixtures/scenarios.json`, exported from the
React screenshot suite by `node plugin/ui/testbed/export-fixtures.mjs`. The
suite's Playwright drive steps (clicks before the shot) are mirrored by hand
in `testbed/Scenarios.cpp`; the capture table flags ids whose drive is
missing. Reference PNGs are produced by the React suite
(`node ui/local/screenshots/capture.mjs`, local-only tooling) into
`ui/local/ui-states/img`; a full run looks like

```sh
$UI --capture /tmp/t3k --ref ui/local/ui-states/img
```

and every scenario should sit at the glyph-rasterisation floor: about 1-2 %
global mismatch (CoreText vs. Skia anti-aliasing and JPEG resampling) and a
worst 64px tile under about 20 % (the header logo's anti-aliased edges run
11 %; photo tiles up to 17 %). The tile figure is the one that catches a
shifted control: one line of misplaced text is a rounding error globally but
lights up its tile. Anything above the floor is a layout bug: crop the two
PNGs and compare ink rows/columns rather than eyeballing.

## Layout

```
plugin/ui/
  NativeEditor.*      AudioProcessorEditor: owns UiPrefs, HttpClient, Tone3000Session,
                      Services, PluginRoot; scales the 1024-wide design space
  NativeUi.cmake      t3k_add_native_ui(<target>), T3kConfig.h, embedded assets
  assets/             Roboto Mono, brand SVGs (UiBinaryData)
  core/               no JUCE components: Theme, Fonts, Icons (+ generated
                      LucideIcons.h, CustomIcons, GearGlyphs), Design, Paint,
                      TextFlow / RichText (CSS-style text layout), Tween /
                      AlphaTween / DelayedCall, AsyncScope, Result, Help
                      (hint strings), Labels, Pitch, EqMath, KnobScale,
                      MeterScale, MidiCatalog, Alerts, Blur, Bitmap (photos
                      resampled once at device density), Wheel
  model/              juce::var → structs: ChainState, Tone, AudioDeviceState,
                      MidiMapState (VarReader)
  backend/            ui::Backend (the native surface the web bridge exposed)
                      and ProcessorBackend over TONE3000Processor
  services/           Services (one bundle per editor) and its members:
                      ChainStore, MeterStore, PresetStore, AudioDeviceStore,
                      MidiMapStore, UiPrefs, HintBus, Toast, Banners,
                      ParamBinding, AutoMeasure, SpectrumFeed, TunerFeed,
                      ModelLoads, LocalFiles, ImageLoader, ConnectionGate,
                      UpdateCheck, ToneLoadFlow; the TONE3000 stack:
                      HttpClient, OAuth (PKCE), LoopbackServer,
                      Tone3000Client, ToneSession / Tone3000Session
  widgets/            reusable controls that know nothing about services
                      (Knob, PillButton, Popover, ContextMenu, ModalLayer,
                      DbMeter, DotMeter, TextField, …); widgets/form/ is the
                      settings form kit (FormItem layout, rows, controls)
  views/              screens, wiring widgets to services: PluginRoot,
                      PluginHeader, Faceplate, MainScreen, TunerView, …
    gallery/          ChainView, GalleryLane, ToneTile, AddTile, StereoPanRail
    block/            BlockDetail, BlockCard, BlockInfoPanel, BlockEqView
    browser/          ToneBrowser, ToneCard, StreamTabs, GearFilterRow, Paginator
    settings/         SettingsScreen, PluginSettingsPage, SystemSettingsPage,
                      MidiMapSection, …
    modals/           ConnectionModal, OAuthOverlay, UpdateNotice
  testbed/            UiTestbed: Main (--capture/--compare/--selftest),
                      MockBackend, MockSession, Scenarios (drives), Drive
                      (Playwright-like helpers), Compare, SelfTests, fixtures/
script/gen-lucide-icons.mjs   regenerates core/LucideIcons.h from lucide-react
```

Every React component maps to one C++ component (the table in
`../docs/native-ui.md` §6). Each `.h` opens with a comment naming the React
file it ports and the CSS facts that fixed its numbers.

## Conventions

- **Design space.** Everything is laid out in the 1024 × 578 design box;
  `NativeEditor` applies one `AffineTransform`. Never scale by hand.
- **Ownership.** `NativeEditor` → `Services` → `PluginRoot` → views. Views
  hold references to the services they use and register as listeners in
  their constructor, deregister in their destructor. No singletons, no
  globals beyond the constexpr tables in `core/`.
- **Data flow.** Stores are the only callers of `Backend`; views subscribe
  to stores, read plain structs, call store actions. Optimistic edits live
  in the store, reconciled on the next revision.
- **Async.** Anything that lands later goes through `AsyncScope::wrap` (or
  `juce::Component::SafePointer`) so a closed editor never gets a callback.
  HTTP runs on `HttpClient`'s pool, images on the same pool, the OAuth
  listener on its own thread; all deliver on the message thread.
- **Text.** Line boxes follow CSS: `Fonts::normalLineHeight`,
  `Fonts::cssBaseline`, `TextFlow` for wrapping/clamping/ellipsis, `RichText`
  for mixed runs and links. Fractional layout positions are kept and snapped
  where Blink snaps them (`FormItem::subpixelTop`, `TextFlow::draw`).
- **Pixels.** New or changed visuals get a scenario in the React suite first
  (`ui/local/screenshots/scenarios.mjs`), exported with
  `export-fixtures.mjs`, and a matching drive in `Scenarios.cpp` if it needs
  one. The capture table is the acceptance test.
- **Logic.** Pure logic (parsers, state machines, math) is tested in
  `testbed/SelfTests.cpp`, one `juce::UnitTest` per file it covers.
- **Icons.** Lucide glyphs come from `script/gen-lucide-icons.mjs` and are
  never hand-edited; brand and gear artwork are in `CustomIcons` /
  `GearGlyphs` with their SVG source noted.
- **Style.** Two files per component, CamelCase, `t3k::ui`; `-Wshadow`
  clean; comments explain the *why* and cite the React/CSS they mirror.

## Sign-in and Select

OAuth (PKCE) runs in the system browser. `Tone3000Session::leave` starts
`LoopbackServer` on `127.0.0.1:<ephemeral>`, opens the authorize URL with
`redirect_uri=http://localhost:<port>/`, and dims the plugin (`OAuthOverlay`
gains a Cancel button after a few seconds, since the user may never come
back from the browser). The redirect lands on the loopback, is checked
against the PKCE `state`, exchanged for tokens (`Tone3000Client`, persisted
in `UiPrefs`, refreshed transparently with a single in-flight refresh and one
401 retry), and, for Select, the picked `tone_id` is resolved into the chain
exactly as the web did. Closing the editor stops the listener; a stale
callback is ignored.
