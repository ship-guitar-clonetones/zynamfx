# Free GUI resizing

The editor window accepts independent width and height changes. The fixed-layout interface scales independently on each axis to fill the editor bounds, so changing the window aspect ratio also changes the content aspect ratio. Minimum dimensions keep the existing layout legible; the selected dimensions are saved with plug-in state and restored when the editor reopens.

The local build helper produces the standalone app and AU component only. It does not enable VST3, AAX, LV2, or CLAP formats. The install option preserves previous Desktop app and user AU bundles as timestamped backups.

## Build record — 2026-10-12

The public AU subtype is `Zyfx`, with manufacturer code `ZYUS`. JUCE requires exactly one uppercase letter in an AU plug-in code; keeping the initial letter uppercase also matches its GarageBand compatibility guidance. The GitHub release workflow reads the built AU metadata and validates the generated component code with `auval`.
