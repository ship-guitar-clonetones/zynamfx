#!/usr/bin/env python3
"""Small deterministic tests for the upstream release version policy."""
import importlib.util
from pathlib import Path
import unittest


MODULE_PATH = Path(__file__).with_name("sync-tone3000-release.py")
SPEC = importlib.util.spec_from_file_location("sync_tone3000_release", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class VersionBumpTests(unittest.TestCase):
    def test_increments_patch(self):
        self.assertEqual(MODULE.bump_version("2.0.0\n"), "2.0.1")
        self.assertEqual(MODULE.bump_version("2.8.19"), "2.8.20")

    def test_rejects_non_v2_or_invalid_versions(self):
        for version in ("1.9.9", "3.0.0", "2.1", "2.x.0", ""):
            with self.subTest(version=version), self.assertRaises(ValueError):
                MODULE.bump_version(version)

    def test_tag_format_is_stable_semver(self):
        for tag in ("v0.0.13", "v1.2.3"):
            self.assertIsNotNone(MODULE.TAG_RE.fullmatch(tag))
        for tag in ("main", "v1.2", "v1.2.3-rc1", "0.1.0"):
            self.assertIsNone(MODULE.TAG_RE.fullmatch(tag))


if __name__ == "__main__":
    unittest.main()
