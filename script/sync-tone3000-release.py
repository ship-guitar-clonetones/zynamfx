#!/usr/bin/env python3
"""Merge a new stable TONE3000 release and advance the zynamfx v2 patch version."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess


TAG_RE = re.compile(r"v[0-9]+\.[0-9]+\.[0-9]+\Z")
VERSION_RE = re.compile(r"([0-9]+)\.([0-9]+)\.([0-9]+)\Z")
AUTO_RESOLVABLE_CONFLICTS = {"README.md", "VERSION"}
UPSTREAM_URL = "https://github.com/tone-3000/tone3000-plugin.git"


def git(*args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(["git", *args], check=check, text=True, capture_output=True)


def bump_version(value: str) -> str:
    match = VERSION_RE.fullmatch(value.strip())
    if match is None or int(match.group(1)) != 2:
        raise ValueError(f"Expected a zynamfx 2.x.y version, got {value!r}")
    major, minor, patch = map(int, match.groups())
    return f"{major}.{minor}.{patch + 1}"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8").strip()


def write_upstream_doc(path: Path, tag: str, sha: str) -> None:
    path.write_text(
        "# Upstream baseline\n\n"
        f"zynamfx Version 2 is based on the TONE3000 plugin **{tag}** release, "
        f"commit [`{sha}`](https://github.com/tone-3000/tone3000-plugin/commit/{sha}). "
        "The pinned release tag and commit are recorded in `.github/upstream-release` "
        "and `.github/upstream-sha`.\n\n"
        "The release workflow checks for newer stable TONE3000 plugin releases once each day. "
        "It merges a new upstream release only when it is a descendant of the recorded baseline "
        "and the merge is clean apart from the app version and project README. Unresolved source "
        "conflicts stop the release. Each successful sync advances zynamfx's patch version and "
        "records the upstream tag and commit here.\n\n"
        "zynamfx is an independent derivative and does not claim to be an official TONE3000 build. "
        "Upstream notices are preserved; see [LICENSE-SCOPE.md](LICENSE-SCOPE.md).\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True, help="stable upstream release tag, e.g. v0.0.13")
    args = parser.parse_args()
    if TAG_RE.fullmatch(args.tag) is None:
        parser.error("upstream release tag must have the form vX.Y.Z")

    repo = Path.cwd()
    git("remote", "add", "upstream", UPSTREAM_URL, check=False)
    fetch = git("fetch", "--no-tags", "upstream", f"refs/tags/{args.tag}:refs/tags/upstream/{args.tag}", check=False)
    if fetch.returncode:
        raise RuntimeError("Could not fetch the requested TONE3000 release tag")
    target = git("rev-parse", f"refs/tags/upstream/{args.tag}^{{commit}}").stdout.strip()
    if re.fullmatch(r"[0-9a-f]{40,64}", target) is None:
        raise RuntimeError("Fetched upstream tag did not resolve to a commit")

    marker = repo / ".github/upstream-sha"
    pinned = read(marker)
    if re.fullmatch(r"[0-9a-f]{40,64}", pinned) is None:
        raise RuntimeError("The recorded upstream commit is malformed")
    current_head = git("rev-parse", "HEAD").stdout.strip()
    current_version = read(repo / "VERSION")
    if target == pinned:
        print(json.dumps({
            "changed": False, "commit": current_head, "version": current_version,
            "upstream_tag": args.tag, "upstream_sha": target,
        }))
        return 0

    pinned_reachable = git("merge-base", "--is-ancestor", pinned, target, check=False).returncode == 0
    already_merged = git("merge-base", "--is-ancestor", target, current_head, check=False).returncode == 0
    if not pinned_reachable and not already_merged:
        raise RuntimeError("The new upstream tag is not a descendant of the recorded baseline")

    merge_started = False
    try:
        if not already_merged:
            merged = git("merge", "--no-commit", "--no-ff", target, check=False)
            merge_started = (repo / ".git/MERGE_HEAD").exists()
            if merged.returncode:
                conflicts = set(git("diff", "--name-only", "--diff-filter=U").stdout.splitlines())
                unexpected = conflicts - AUTO_RESOLVABLE_CONFLICTS
                if not conflicts or unexpected:
                    raise RuntimeError("Upstream merge has unresolved conflicts: " + ", ".join(sorted(conflicts)))
                for path in sorted(conflicts):
                    git("checkout", "--ours", "--", path)
                    git("add", "--", path)

        next_version = bump_version(current_version)
        (repo / "VERSION").write_text(next_version + "\n", encoding="utf-8")
        marker.write_text(target + "\n", encoding="utf-8")
        (repo / ".github/upstream-release").write_text(args.tag + "\n", encoding="utf-8")
        write_upstream_doc(repo / "UPSTREAM.md", args.tag, target)
        git("add", "VERSION", ".github/upstream-sha", ".github/upstream-release", "UPSTREAM.md")
        message = f"Sync TONE3000 {args.tag}; release zynamfx v{next_version}"
        git("commit", "-m", message)
    except Exception:
        if merge_started:
            git("merge", "--abort", check=False)
        raise

    print(json.dumps({
        "changed": True, "commit": git("rev-parse", "HEAD").stdout.strip(),
        "version": next_version, "upstream_tag": args.tag, "upstream_sha": target,
    }))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
