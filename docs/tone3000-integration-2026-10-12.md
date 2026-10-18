# TONE3000 Free-tier integration

This document describes the public zynamfx integration as of 2026-10-12. zynamfx is free and open source, has no paid product or upsell, and uses TONE3000's non-commercial Free tier.

## User flow

- The in-app **Browse TONE3000** action starts the hosted OAuth `select_tone` flow inside an in-editor browser panel (standalone, AU, VST3; the system WKWebView, no bundled engine). TONE3000 presents its own sign-in, full catalog search, filters, creator pages, audition and tone selection under its own branding. **Open in Browser** hands the same URL to the system browser, and the sign-in screen keeps the copy-link and device-code fallbacks.
- The app validates the OAuth state and callback, exchanges the returned code, and fetches metadata/models only for the tone the user selected. The API docs' Select flow example uses this same tone/model resolution step.
- Catalog browsing runs in-app with the same documented endpoints the upstream plugin uses: tone search, tags/makes/users taxonomy, the signed-in user's downloaded/favorited/created lists, favorite writes, and tone/model resolution. Every call runs under the user's own publishable key and their own token.
- Sign-in is the hosted OAuth consent, shown in an in-editor browser. The app validates state and the PKCE exchange, and the token set is stored locally for that user. Device authorization and the copyable sign-in link remain as fallbacks.
- A model download starts only to load the user's selected tone. The app does not scrape, mirror, bundle, or redistribute catalog models. Creator metadata and license information are preserved in tone state and the details view.
- Public binaries contain no API key, access token, refresh token or catalog model. A user supplies their own publishable client ID; the server-only secret key is rejected. Tokens remain local to that user's app settings and can be removed by signing out.

## Branding and code licenses

The product is named and identified as zynamfx, with independent graphics and plugin identifiers. The interface identifies the catalog provider with TONE3000 branding and “Powered by TONE3000”; README links to the service. The project claims no endorsement, certification or partnership. The source retains TONE3000's MIT notice and notices for NAM, JUCE, and other components; the combined zynamfx application is distributed under AGPL-3.0-only as described in `LICENSE-SCOPE.md`.

NAM's open-source license covers the model format and inference implementation. It does not grant rights to the TONE3000 API, catalog, creator uploads or individual models; API terms and creator-selected model licenses remain separate.

## Published basis and limits

Checked against the current [API Terms](https://www.tone3000.com/api/terms), [Select-flow documentation](https://www.tone3000.com/api#select), and [API design guidance](https://www.tone3000.com/api#design-requirements). The implementation follows the published non-commercial tier and does not claim individual provider approval. TONE3000 may change terms or revoke API access; re-check the official terms before each release. This README is a project compliance summary, not legal advice.

## Verification status

Automated tests cover request-path allowlisting, bounded list parameters, OAuth state/callback handling, and selected-tone loading. They cannot guarantee TONE3000's interpretation or uninterrupted service access. A live catalog test must use the maintainer's own account and must not be represented as provider approval.
