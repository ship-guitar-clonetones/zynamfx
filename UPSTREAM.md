# Upstream baseline

zynamfx Version 2 is based on the TONE3000 plugin **v0.0.12** release, commit [`ab5e5ae4388753184687d67816815aa481f2cea9`](https://github.com/tone-3000/tone3000-plugin/commit/ab5e5ae4388753184687d67816815aa481f2cea9). The pinned release tag and commit are recorded in `.github/upstream-release` and `.github/upstream-sha`.

The release workflow checks for newer stable TONE3000 plugin releases once each day. It merges a new upstream release only when it is a descendant of the recorded baseline and the merge is clean apart from the app version and project README. Unresolved source conflicts stop the release. Each successful sync advances zynamfx's patch version and records the upstream tag and commit here.

zynamfx is an independent derivative and does not claim to be an official TONE3000 build. Upstream notices are preserved; see [LICENSE-SCOPE.md](LICENSE-SCOPE.md).
