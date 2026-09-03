# iOS (iPad) build

Standalone-only iPad port of the plugin: the same C++ and the same React UI,
with every difference behind `#if JUCE_IOS` (C++) or
`window.__T3K_PLATFORM__ === 'ios'` / `pointerType === 'touch'` (UI). Desktop
behaviour is unchanged. AUv3 is out of scope; iPhone is untested.

Deployment target iOS 16. Landscape only.

## Build

Configure first, then build the UI, then configure again. `ui/package.json`
resolves `@juce-framework/webview` from `libs/juce`, which the first configure
is what creates, so building the UI first on a clean checkout fails with
`Cannot find module '@juce-framework/webview'`. That first configure embeds a
placeholder UI; the second picks up the real bundle. Same order as the root
README and the `iOS Simulator` CI job.

```sh
cmake --preset ios-simulator   # or ios-device: bootstrap, fetches JUCE into libs/
cd ui && npm ci && npm run build && cd ..

# Simulator
cmake --preset ios-simulator
cmake --build build-ios --config Release --target TONE3000_Standalone -- -sdk iphonesimulator

# Device
cmake --preset ios-device
cmake --build build-ios-device --config Release --target TONE3000_Standalone -- \
  -sdk iphoneos -allowProvisioningUpdates
```

The **Build Plugin** workflow (`.github/workflows/build.yml`) has an
`iOS Simulator` job that runs the same Simulator build on a macOS runner and
uploads the unsigned `.app` as an artifact.

Build **Release** on the Simulator. A Debug iOS build points the WebView at
`http://localhost:5173/`, so it shows a dead page and logs "navigation failed".

`-DT3K_IOS_BUNDLE_ID=<id>` signs under your own identity. Changing it on a
device that already holds the app gives a fresh, empty Documents folder, so
keep it stable once models are loaded. Add
`-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=<id>` if Xcode cannot pick your team.

**Reconfigure after every UI change.** `plugin/CMakeLists.txt` collects the
webview with `file(GLOB_RECURSE)`, which runs at configure time, and Vite's
asset filenames are content-hashed. Without a reconfigure the app keeps
serving the previously embedded bundle and looks like your change did nothing.
The requested asset name in the app's log tells you which bundle is running.

## Install and log

Each preset writes to its own build directory: `build-ios` for
`ios-simulator`, `build-ios-device` for `ios-device`. The artefact path under
each is the same.

```sh
# Simulator
xcrun simctl install <udid> build-ios/plugin/TONE3000_artefacts/Release/Standalone/TONE3000.app
xcrun simctl launch <udid> <bundle-id>

# Device
xcrun devicectl device install app --device <udid> \
  build-ios-device/plugin/TONE3000_artefacts/Release/Standalone/TONE3000.app

# The app's own log: console.* from the WebView is forwarded into it, which
# is the most useful debugging channel on both Simulator and device.
tail -f "$(xcrun simctl get_app_container <udid> <bundle-id> data)/Library/TONE3000/TONE3000.log"
```

Xcode's Devices and Simulators window installs either build too, if you
prefer it to the command line.

Simulator screenshots come out portrait while the app renders landscape.

## TestFlight and the App Store

A signed build is the `ios-device` preset plus your team. Everything below is
what App Store Connect checks on top of that, and none of it shows up in a
Simulator build.

- `plugin/PrivacyInfo.xcprivacy` declares the required-reason APIs the binary
  reaches through JUCE: user defaults (the WebView component), file timestamps
  and free disk space (both `juce_SharedCode_posix.h`), and system boot time
  (`systemUptime` timestamping touch events in
  `juce_UIViewComponentPeer_ios.mm`, plus `mach_absolute_time`). An upload whose
  binary calls one of those without declaring it is rejected with ITMS-91053,
  so the list is worth re-deriving whenever the JUCE version moves.
- The app icons are flattened to opaque at configure time by
  `script/flatten-icon-alpha.swift`, run through `xcrun swift`. juceaide writes
  them RGBA whatever the source is, and an alpha channel on the 1024 icon means
  ITMS-90717 and no icon in TestFlight. It uses ImageIO rather than a tool from
  a package manager because every machine that can build this target already
  has both, the GitHub macOS runner included.
- `ITSAppUsesNonExemptEncryption` is false in the Info.plist. The app's only
  encryption is standard HTTPS, and declaring it here answers the
  export-compliance question once instead of on every upload.
- `UIRequiresFullScreen` is true. A landscape-only iPad app must either list
  all four orientations or declare itself full-screen; without the key the
  upload is refused with ITMS-90474. It costs nothing at runtime on
  iPadOS 26 (see the multitasking note below).
- `plugin/icon/icon.png` is 512x512 and juceaide never enlarges a source, so the
  App Store icon is currently that 512 artwork centred on a blank 1024 field.
  Exporting the icon at 1024 fixes it; the configure step warns until then.
- `T3K_IOS_BUILD_NUMBER` is CFBundleVersion, and it defaults to 1. App Store
  Connect refuses a build number it has already seen for the same marketing
  version, so a second upload of one version needs
  `-DT3K_IOS_BUILD_NUMBER=<n>`. Without the setting at all, JUCE uses the
  marketing version as the build number and the second upload always bounces.
- The **Deploy to TestFlight** workflow
  (`.github/workflows/deploy-testflight.yml`) does the upload: publishing a
  GitHub Release builds, signs and uploads, and `workflow_dispatch` runs the
  same pipeline against any ref. Signing and upload both use an App Store
  Connect API key from the `builds` environment, so no keychain or stored
  profile is involved, and the build number is the workflow run number. A
  repository without those credentials skips the job instead of failing.
- App Store Connect requires uploads built against a current iOS SDK. A runner
  pinned to an older Xcode builds and signs fine and is then refused at upload,
  which reads as a signing problem and is not one.

## Touch rules

| gesture | result |
| ------- | ------ |
| tap a tile | open the block |
| swipe over a tile | scroll the chain lane |
| hold 250 ms, then drag | reorder |
| hold, release without moving | tile menu at that point |
| `...` on a tile | the same menu, visibly |
| hold on the Spread / Align group | the advanced deck (desktop: right-click) |
| press a control | its help in the info bar; release clears it |
| drag a knob | adjust, with the value in a bubble above it |
| double tap a knob | reset to default |
| swipe in from the left edge | back, on BLOCK and SELECT TONE |
| swipe down | dismiss the Tuner and Settings |

No gesture is the only route to anything: every action above also has a
visible control, per the HIG.

Every touch target meets 44 pt through one rule in `index.css` under
`html.t3k-ios`: an invisible `::after` at `max(100%, 44px)`, centred and out
of flow, so no layout changes.

## Touch verification

Everything below was driven on the iPad Simulator against a Release build.
Local `.nam` models only: the catalogue needs a sign-in the port cannot
complete (see Known gaps).

| Area | Verdict |
| ---- | ------- |
| Tap a chain tile; power / `...` / swap / trash on it | fixed and passing. A 44 pt hit expander was landing on the tile wrapper, which dnd-kit marks `role="button"`, and swallowing every tap |
| Preset prev / next | steps and wraps |
| Preset name popover | opens under the pill, above the keyboard it raises |
| Save preset | popover and its field stay clear of the keyboard; saves |
| New | clears the chain, greys out once at the default |
| Preset reorder on touch | swipe scrolls the list; hold the grip, then drag, moves the row |
| Tuner | opens; closes by `X` and by swipe down |
| Undo / redo | covers reorder (both ways), remove and paste |
| Mono / stereo toggle | switches; two lanes, pan rail, ALIGN and Balance appear |
| Stereo two-lane layout | tiles scale with the same three-across rule as mono |
| Spread / Align, hold for the advanced deck | both decks open on a touch and hold |
| Per-block EQ | faders and curve dots both drag; the response redraws |
| Block swap / remove | swap opens SELECT TONE for that block; remove takes it out |
| Block info / share | **not tested**: both controls exist only for a catalogue tone |

## Platform notes worth knowing

- **Picker results must be read through security-scoped URLs.** A file chosen
  outside the app container is unreadable through its raw path. A test with
  the file *inside* the container passes and proves nothing.
- **WebKit replays a mouse event pair after every touch**, aimed at the
  element just tapped and landing after `pointerup`. Anything that clears
  state on release has to ignore that replay.
- **`pointercancel` is reported at 0,0.** The page opts into panning, so
  WKWebView takes swipes over and ends them with a cancel carrying no useful
  position, and no `pointerup`. Swipe gestures use touch events instead.
- **A control that takes pointer capture retargets its release**, so a release
  that must be seen regardless is watched on `window` in the capture phase.
- **`env(safe-area-inset-*)` is 0 on all sides** here: the WKWebView is
  already inset (1366x999 in a 1024 pt screen), so the faceplate clears the
  home indicator without the page doing anything.
- **The app data container's UUID rotates on every reinstall and every app
  update.** Any absolute path the plugin persisted then names a directory
  that no longer exists, and the only path it persists is a local model's
  stash URL, in the tone JSON that rides presets, the saved app state and
  undo snapshots. `resolveLocalModelFile` re-roots the stored (content-hashed)
  file name under the current stash folder; a path that still exists is used
  as-is, which is every desktop case. Presets and project state were never
  affected: they embed the model bytes.
- **Bluetooth headphones cap the whole session at 16 or 24 kHz.** We hit this
  on an iPad with AirPods: `prepareToPlay: sampleRate=24000` and a sample-rate
  warning in Settings with nothing saying why. JUCE opens the iOS
  session as `PlayAndRecord` with `AllowBluetoothHFP`
  (`juce_Audio_ios.cpp`, `setAudioSessionCategory`), so a headset with a
  microphone wins the route and iOS refuses the requested 48 kHz. Two answers
  ship together, both in `IosAudioRoute` (the Haptics / AudioPermissions
  shim pattern, header-only no-op off iOS):
  - `configureSession()` makes one `setCategory:mode:options:` call: the
    category and options JUCE asked for, minus `AllowBluetoothHFP`, with
    Measurement mode (the raw input path). `AllowBluetoothA2DP` stays, so
    Bluetooth output-only listening still works and only the low-rate
    headset *mic* route goes away. Mode and options go in one call on
    purpose: on iPadOS 26 a bare `setMode:` clears category options.
    Measured on an iPad Pro: from Default mode `0x69` became `0x1` (A2DP,
    AirPlay and DefaultToSpeaker all gone); with the mode already
    Measurement, a repeated `setMode:` turned `0x69` into `0x61`
    (DefaultToSpeaker gone). It is not a JUCE text patch: JUCE sets the
    category when it *opens* a device and never on its own route-change
    `restart()` path, so re-applying it on every device-manager change is
    enough and the JUCE tree stays untouched. On the same iPad Pro, with
    AirPods Pro connected and reading `AVAudioSession` from the app log:
    the first open with HFP allowed came up at 24 kHz; after the call the
    device reopened at 48 kHz on the built-in mic. With a USB interface
    unplugged mid-session, the route went to the built-in mic at 48 kHz,
    then to built-in mic plus AirPods A2DP output at 48 kHz, and back to
    the interface when it was plugged in again, with options `0x69` and
    Measurement mode held through every change.
  - `isBluetoothRoute()` feeds `bluetoothRoute` in the settings state, and
    the UI turns that (or any session under 44.1 kHz) into one plain tip in
    Settings > System Settings, next to Sample Rate: use wired headphones,
    the iPad speaker, or a USB audio interface. The generic "runs lightest
    at 48 kHz" note is suppressed while it shows, so there is one
    explanation instead of two.
- `xcrun simctl privacy grant microphone` does not suppress the prompt;
  `AVAudioSession` still asks once.
- **`UIRequiresFullScreen` no longer opts an app out of multitasking** on
  iPadOS 26: a second app dragged from the Dock windows itself over this one
  regardless. The app is not resized by it (the other app floats), so the
  layout is unaffected. The key is set anyway because App Store validation
  still requires it for a landscape-only iPad app (ITMS-90474) — it changes
  runtime behaviour only on older iPadOS, where it disables Split View.
- The `NAM` static library must be force-loaded on iOS as well as macOS.
  `$<PLATFORM_ID:...>` reports `iOS`, not `Darwin`, when cross-compiling, so
  without both the linker strips the model-architecture registrations and
  loads fail with "No config parser registered for ...".

## Known gaps

- Load Folder is a multi-select on iOS: a security-scoped *directory* cannot
  be enumerated, so the picker returns files instead.
- The double-tap knob reset is proved in a browser against the same bundle,
  not on a device: two taps cannot be driven inside 300 ms through the
  Simulator automation bridge.
- Dragging a `.nam` from Files onto a tile is untested. The receiving code is
  the same HTML5 drop path the desktop uses, and the app does window alongside
  Files, but the drag could not be driven from the automation.
- No haptics: the iPad has no Taptic Engine, so
  `UIImpactFeedbackGenerator` does nothing there and the tile lift and drop
  are silent.
- AUv3 is not built. Only the Standalone app exists on iOS.

## Desktop CI evidence

Nothing on this branch reaches a desktop build. The C++ side is one
`withUserScript` call inside `#if JUCE_IOS`, so every other platform's
injected script is byte-identical to before; everything else is TypeScript
and CSS gated on `IS_IOS` / `html.t3k-ios`, which is false and absent in
every desktop build. macOS Release was rebuilt locally on this branch as the
regression check, and the shared `ui` bundle builds and lints clean.
