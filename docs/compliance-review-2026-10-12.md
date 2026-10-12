# Compliance remediation — 2026-10-12

Scope: private local-only macOS standalone testing. This records engineering
changes against published requirements; it is not a legal certification or
evidence of approval by any provider.

## Implemented

- zynamfx retains the MIT TONE3000/NAM source notices and credits.
- The app uses SignedOutSession instead of the online OAuth/catalog client.
  Add/swap opens local files; login/API-key controls are hidden.
- HttpClient rejects all requests, images reject remote URLs and the processor
  rejects non-file model URLs, including old-preset restore/cache misses.
- No official-product factory-preset directory is silently imported.
- No downloaded catalog model library or user settings are packaged.
- The combined app selects JUCE's GNU AGPLv3 route. COPYING, license scope,
  upstream/component/font notices and source-delivery instructions are bundled.
  Settings provides access to the notices and states there is no warranty.
- Matching corresponding source is conveyed with the test app, including exact
  JUCE/NAM/AudioDSPTools dependencies and applied patches. The archive includes
  a file-hash manifest. No private Git access is required by its recipient.
- Source archives support configuring with vendored JUCE/GoogleTest without Git.
- Inherited automatic/public release workflows are removed.
- No provider logos are altered or partnership/endorsement claimed.
- Original graphics use a visual reference rather than Line 6 assets.

## Why local-only

The MIT source license permits modification/rebranding with notices. API terms
are separate: the former brand-free online browser conflicted with provider
branding, imagery and free-tier endpoint requirements. Keeping the NAM engine
does not require access to that API. Re-enabling an online integration requires
a new review and the required provider/creator presentation and permitted flows.

## Remaining boundaries

Recipients must preserve AGPL corresponding-source and component obligations
if redistributing the app. Local model files and hosted third-party plugins
remain subject to their own licenses. No commercial JUCE license, provider
approval, trademark clearance or counsel opinion has been obtained. Do not
describe these checks as a guarantee of 100% legal compliance.

## Sources checked

- https://github.com/tone-3000/tone3000-plugin/blob/main/LICENSE
- https://www.tone3000.com/api/terms (effective July 7, 2026)
- https://www.tone3000.com/api#design-requirements
- https://juce.com/legal/juce-9-licence/
- https://github.com/juce-framework/JUCE/blob/9.0.3/LICENSE.md
- Exact checked-out component licenses and the full AGPL/GPL texts from SPDX.

## Build notes

Local-only account-menu sizing is implemented inside its Menu subclass, where
Popover's protected border constant is accessible. Edits to C++ constructors
must target the constructor body, not an initializer-list brace.
GNU text retrieval was blocked by HTTP 403; official SPDX license-list-data
copies were verified by license name/version and bundled instead.

## Execution validation

The macOS standalone Release build passed. The UI suite passed 658 assertions across 123 groups, including five assertions proving that a real HTTP request is rejected without connecting to a loopback server. Local-only main and Settings fixtures were captured for inspection. The inherited DSP test target was not built, following the build skill’s documented hosted-effects linker limitation.
