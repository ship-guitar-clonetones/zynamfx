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
`VITE_T3K_UPDATE_NOTICE`) is read at CMake configure time
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
$UI --capture out [--ref refDir] [filter…]   # 2x PNG per scenario (+ mismatch table); fails on unnamed Tab stops
$UI --compare ref.png out.png [diff.png]     # one pair
$UI --selftest                               # unit tests: pure logic + the focus policy in a real window
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
                      AlphaTween (vblank-driven, via juce_animation),
                      DelayedCall, AsyncScope, Result, Help
                      (hint strings), Labels, Pitch, EqMath, KnobScale,
                      MeterScale, MidiCatalog, Alerts, Blur, Bitmap (photos
                      resampled once at device density), Wheel
  model/              juce::var → structs: ChainState, Tone, AudioDeviceState,
                      MidiMapState (VarReader); ToneQuery (the browser's
                      filters → the API query string)
  backend/            ui::Backend (the native surface the web bridge exposed)
                      and ProcessorBackend over TONE3000Processor
  services/           Services (one bundle per editor) and its members
                      (BrowserState: what the tone browser keeps between visits):
                      ChainStore, MeterStore, PresetStore, AudioDeviceStore,
                      MidiMapStore, UiPrefs, HintBus, Toast, Banners,
                      ParamBinding, AutoMeasure, SpectrumFeed, TunerFeed,
                      ModelLoads, LocalFiles, ImageLoader, ConnectionGate,
                      UpdateCheck, ToneLoadFlow; the TONE3000 stack:
                      HttpClient, OAuth (PKCE), LoopbackServer,
                      Tone3000Client, ToneSession / Tone3000Session
  widgets/            reusable controls that know nothing about services
                      (Knob, PillButton, Popover, ContextMenu, ModalLayer,
                      DbMeter, DotMeter, TextField, DragScroller, …);
                      widgets/form/ is the
                      settings form kit (FormItem layout, rows, controls)
  views/              screens, wiring widgets to services: PluginRoot,
                      PluginHeader, Faceplate, MainScreen, TunerView, …
    gallery/          ChainView, GalleryLane, ToneTile, AddTile, StereoPanRail
    block/            BlockDetail, BlockCard, BlockInfoPanel, BlockEqView
    browser/          ToneBrowser (the Select tone screen: search + FilterBar over the
                      card grid), FilterBar / FilterChip / FilterMenu, ToneCard,
                      Paginator, BrowserPrompt
    settings/         SettingsScreen, PluginSettingsPage, SystemSettingsPage,
                      MidiMapSection, …
    modals/           ConnectionModal, OAuthOverlay, UpdateNotice
  testbed/            UiTestbed: Main (--capture/--compare/--selftest), Host
                      (the root over the mocks, fitted like NativeEditor),
                      MockBackend, MockSession, Scenarios (drives), Drive
                      (Playwright-like helpers + the a11y audit), Compare,
                      SelfTests, fixtures/
script/gen-lucide-icons.mjs   regenerates core/LucideIcons.h from lucide-react
```

Every React component maps to one C++ component (the table in
`../docs/native-ui.md` §6). Each `.h` opens with a comment naming the React
file it ports and the CSS facts that fixed its numbers.

## Conventions

- **Design space.** Everything is laid out in the 1024 × 578 design box;
  `NativeEditor` applies one `AffineTransform`. Never scale by hand. The one
  view that holds its size on screen instead (the tone browser's body, so a
  bigger window shows more results) counter-scales by `Services::zoom`, the
  factor the shell publishes on every fit; a `Popover` adopts its anchor's
  scale, so menus opened from it are 1x too.
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
- **Focus and keys.** Nothing is focused until Tab or an explicit `focus()`
  / `grabKeyboardFocus()`; a click focuses only text fields, so the host's
  Space / Enter keep working after mouse work. Every button derives from
  `widgets/Clickable` (Tab-focusable, never by click, named for screen
  readers from its text or help lead); a new focusable control follows the
  same two `set…KeyboardFocus` calls and gives itself a name (`setTitle`,
  button text or a "Name: …" help hint). `PluginRoot::FocusPolicy` owns the
  rest (Tab from nothing, Escape / press elsewhere blur). Decorative
  components call `setAccessible(false)`; status that only paints elsewhere
  goes through `help::announce()`. See native-ui.md §5.8a.
- **Pixels.** New or changed visuals get a scenario in the React suite first
  (`ui/local/screenshots/scenarios.mjs`), exported with
  `export-fixtures.mjs`, and a matching drive in `Scenarios.cpp` if it needs
  one. The capture table is the acceptance test. Screens the web build no
  longer has (the tone browser) are declared `nativeOnly` there: exported
  for the testbed, skipped by the web capture, so they have no reference
  PNG and are reviewed by eye against the Figma mockups.
- **Logic.** Pure logic (parsers, state machines, math) is tested in
  `testbed/SelfTests.cpp`, one `juce::UnitTest` per file it covers.
- **Icons.** Lucide glyphs come from `script/gen-lucide-icons.mjs` and are
  never hand-edited; brand and gear artwork are in `CustomIcons` /
  `GearGlyphs` with their SVG source noted.
- **Style.** Two files per component, CamelCase, `t3k::ui`; `-Wshadow`
  clean; comments explain the *why* and cite the React/CSS they mirror.

## Sign-in

OAuth (PKCE) runs in the system browser. `Tone3000Session::login` starts
`LoopbackServer` on `127.0.0.1:<ephemeral>`, opens the authorize URL with
`redirect_uri=http://localhost:<port>/`, and dims the plugin (`OAuthOverlay`
gains a Cancel button after a few seconds, since the user may never come
back from the browser). The redirect lands on the loopback, is checked
against the PKCE `state`, exchanged for tokens (`Tone3000Client`, persisted
in `UiPrefs`, refreshed transparently with a single in-flight refresh and one
401 retry). A login started from the tone browser (`LoginIntent::browse`)
lands back in it. Closing the editor stops the listener; a stale callback is
ignored.

## The tone browser (Select tone)

The whole screen needs a session: signed out it shows only the sign-in
prompt. Signed in, `ToneBrowser` pins a search box and a `FilterBar` above
the scrolling card grid and asks `ToneSession::searchTones` for one page at a
time. The ← row zooms with the window; the body under it does not: it is
laid out in screen pixels under the zoom, the search box and filter row keep
their 1x height and widen with the column, the cards keep their 1x height
and widen to fill two columns, and go three-up once three fit at the default
width (`browser-zoom-wide`, `browser-zoom-three-up`, `browser-zoom-menu`; a
scenario's `zoom` sizes the testbed window).

- `BrowserState` (`services/`, one per editor) is what the screen keeps
  between visits: the `ToneQuery`, whether the filter row is unfolded, the
  page and the page's results. The browser is mounted only while open, so
  coming back renders the last page at once with no fetch; it refreshes on
  the next search, filter change or page turn. Closing the editor forgets it.
- `ToneQuery` (model) is the one place the filters live: text, sort, gear,
  format, tags / makes / creators, calibrated, verified, profile. It builds
  the API path itself (`requestPath`): `/tones/search` with the query string,
  or `/tones/{downloaded,favorited,created}` with the title search and gear
  alone when a profile filter is set. The plugin's NAM architecture always rides along (the API
  ignores it for IR, and omitting it falls back to a legacy A1-only default).
  The default sort (best match with text, else trending) is stored as no
  pick, so Trending never reads as a filter. Unit-tested in `SelfTests.cpp`.
- `FilterBar` edits the state's query through one horizontally scrolling row
  of chips (`FilterChip`): the filters button, then (once unfolded, pushing
  the rest right) Sort, Format, Tags, Makes, Creators, Calibrated, a
  divider, then verified, the profile chip (the user's avatar alone until a
  profile is picked) and the gear chips. A chip holding a value shows it
  with an × that clears it (Sort: back to the default); the filters button
  carries a dot while any of the unfolded ones are set. A profile filter
  parks the catalog-only controls (dimmed, hint says why) without losing
  their values; the search box stays live, matching titles within the
  stream; IR gear (cabinet, space) or the IR
  format parks Calibrated the same way, and the flag stays out of the
  request (`ToneQuery::calibratedInForce`).
- `FilterMenu` is the dropdown (`Popover`). Single-pick menus (Sort, Format,
  Profile) show the current value in white; multi-pick ones add a check
  column. Rows may lead with an icon (Favorites' bookmark) or a creator
  avatar. Taxonomy menus add a search field and look their rows up from
  `ToneSession::listTaxonomy` (debounced, a newer lookup cancels the one in
  flight). Any pick closes the menu.
- The search box submits on Enter (or its ×, or Escape, which clear it);
  every filter change fetches page 1 at once. An older page arriving after
  a newer one is dropped.
- The web-facing bits stay in the web UI: nothing here is shared with `ui/`
  except the fixtures the testbed renders.
