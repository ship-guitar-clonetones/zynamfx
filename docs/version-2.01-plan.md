# Version 2.01 plan — Hosted FX search

Status: proposed, research only. Requested 2026-10-13. No application implementation, build, installation, version bump or release is included in this change. The current VERSION file is 2.0.0; “2.01” is the requested planning label, with release numbering to be resolved when preparing the release.

## Feasibility and source evidence

Feasible as a focused native JUCE UI change. Research inspected main commit 2fc152b.

- `plugin/ui/views/gallery/AddTile.cpp`: `chooseInstalledEffect()` obtains a bundle list, passes vendor/name/format/path entries to EffectMenu, and defers loading through MessageManager::callAsync. Preserve that deferred callback and insert-slot target; synchronous loading during a row click can destroy the menu while its callback is running.
- `plugin/ui/widgets/EffectMenu.h` and `.cpp`: custom Popover with vendor and effect columns, owned viewports, and a frosted vendor background after selection. Entries already contain all metadata needed for local filtering. There is currently no search field.
- `plugin/src/ProcessorHostedEffects.cpp`: `listHostedEffectBundles()` enumerates system AU/VST3 bundle directories and reads bundle metadata without instantiating plug-ins. `listHostedEffects()` performs scanning; it must not be used for typing/filtering. Loading/validation happens only after selection.
- `plugin/ui/widgets/Popover.cpp`: inherited keyboard handling uses Escape to dismiss and Up/Down/Tab to move focus. A TextEditor needs explicit interaction with these shortcuts.

This conclusion is from source inspection, not a prototype, GUI check or performance measurement.

## Proposed user experience

1. Add a persistent search field at the top of the Add effect / Hosted FX chooser, above both columns. Placeholder: “Search effects or vendors…”; provide an accessible clear control.
2. Empty query keeps the current vendor → effect flow and blurred vendor background.
3. Non-empty query switches the area beneath the field to one scrollable global results list across all vendors. Show effect name, vendor and AU/VST3 on every result so duplicates are unambiguous. Search remains visible and unblurred.
4. Match case-insensitively against displayed effect name, vendor and format. Trim whitespace and split into whitespace-separated tokens; every token must occur somewhere in the combined fields. Example: “fabfilter au”. No fuzzy matching or network calls in this first iteration.
5. Sort deterministically by name, vendor, format, then path. Keep AU and VST3 entries separately selectable; retain the exact original bundle path as identity.
6. Display “No matching effects” for zero matches. Clearing restores vendor browsing, including the previous vendor when available. Reset the search on reopening; no preset/settings persistence is needed.
7. Focus the search field when opened. Typing edits the query; Down transfers focus to the first result, Enter on a focused result loads it, Tab/Shift+Tab traverse visible controls. Escape dismisses the chooser consistently with existing behavior. Verify host keyboard capture and focus restoration.

## Implementation sequence for later authorization

- [ ] Add search editor, clear button, results viewport and explicit query/vendor view state to EffectMenu. Keep changes local where possible; adjust AddTile only if an after-open focus call is necessary.
- [ ] Extract a small deterministic matching/sorting function and test it with synthetic entries; retain immutable source entries and stable path identity.
- [ ] Filter only the existing in-memory entries on the JUCE message thread. Rebuild visible rows safely, reset result scroll after query edits, and avoid destroying a row from inside its own activation callback. Retain the existing deferred plug-in load.
- [ ] Reserve header space in resized(), keep the field outside the frost layer, and apply height/width clamping on every mode/vendor change. Current pickVendor() resets the height to 420 plus padding, which can undo centreOn() clamping; fix that within the layout work.
- [ ] Add accessible labels and focus behavior scoped to this chooser; inspect inherited Popover traversal before changing shared keyboard handling.
- [ ] Update docs/hosted-effects.md with the final interaction after implementation.

## Validation and release acceptance

- [ ] Focused UI tests: empty/whitespace input, mixed case, partial name, vendor, format, multi-token AND behavior, zero matches, duplicate names/formats/vendors, deterministic order and clear/restore behavior.
- [ ] Verify search/filter/clear operations never invoke scanning, plug-in instantiation or audio-chain changes; only explicit selection calls the existing loader once with the correct path and insert target.
- [ ] Test scrolling and response with a large synthetic library; measure before deciding whether debounce or virtualized rows are necessary.
- [ ] Manually inspect small/resized windows and UI scaling: header stays visible, results fit, blur excludes search, and switching modes never places the panel outside the overlay.
- [ ] Manually check keyboard, focus restoration, outside-click dismissal, repeated open/close, and loading one installed AU and one installed VST3. Exercise load failure without damaging the existing chain.
- [ ] Run the repository's affected UI checks and current supported macOS build/validation workflow. Test standalone and each distributed plug-in format in a host before claiming those formats verified. No GUI/audio claim follows from headless tests alone.

## Scope and limits

Search covers only bundles already returned by the chooser: currently nonrecursive system /Library/Audio/Plug-Ins/Components and /Library/Audio/Plug-Ins/VST3 listings. User-library folders, nested bundles, authoritative manufacturer discovery and multi-effect shell bundles require separate discovery work. Bundle names/vendor aliases may differ from plug-in marketing names. A search result does not certify that a bundle is a supported effect; existing load-time checks remain necessary. No DSP, latency, state serialization, TONE3000 catalog search or public API changes are required for this proposal.
