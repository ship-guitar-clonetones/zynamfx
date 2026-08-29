"""signtool shim for PACE wraptool + Azure Artifact Signing (Trusted Signing).

wraptool's built-in Authenticode step assumes a local certificate (.p12 file
or cert-store sign id), which cloud signing services like Azure Artifact
Signing don't provide. wraptool does support delegating to a custom signtool
(--signtool), so this shim discards the arguments wraptool composed (they
reference the bogus '--signid 1' placeholder), keeps only the path of the
binary wraptool wants signed, and re-invokes the real signtool.exe with the
Artifact Signing dlib arguments instead.

Approach documented by KoalaDSP's "Artifact Signing for plugin developers"
guide (github.com/koaladsp/KoalaDocs).

Required environment (set by sign-aax-windows.ps1):
  SIGNTOOL_PATH  absolute path to signtool.exe (Windows SDK BuildTools >=
                 10.0.22621.755; older signtools don't support /dlib)
  ACS_DLIB       absolute path to Azure.CodeSigning.Dlib.dll (x64)
  ACS_JSON       absolute path to the Artifact Signing metadata.json
  AZURE_TENANT_ID / AZURE_CLIENT_ID / AZURE_CLIENT_SECRET
                 service-principal credentials the dlib authenticates with
"""

import os
import re
import subprocess
import sys


def main() -> int:
    raw = " ".join(sys.argv[1:])
    print(f"aax-signtool shim: arguments from wraptool: {raw}", flush=True)

    # wraptool composes: sign /sha1 <id> [/t <timestamp-url>] <path to dll>.
    # Do NOT regex the joined string for "[drive]:[/\]"..... that matches the
    # "p:/" inside "http://..." and turns the URL+path into a bogus target
    # (seen in CI as "signing p://timestamp.sectigo.com C:\..."). Take the
    # last argv that looks like a real Windows filesystem path instead.
    target = None
    for arg in reversed(sys.argv[1:]):
        candidate = arg.strip().strip('"')
        if re.match(r"^[A-Za-z]:[\\/]", candidate) and "://" not in candidate:
            target = candidate
            break
    if not target:
        print("aax-signtool shim: no file path found in arguments", flush=True)
        return 1

    missing = [v for v in ("SIGNTOOL_PATH", "ACS_DLIB", "ACS_JSON") if not os.environ.get(v)]
    if missing:
        print(f"aax-signtool shim: missing environment: {', '.join(missing)}", flush=True)
        return 1

    cmd = [
        os.environ["SIGNTOOL_PATH"],
        "sign",
        "/v",
        "/debug",
        "/fd", "SHA256",
        "/tr", "http://timestamp.acs.microsoft.com",
        "/td", "SHA256",
        "/dlib", os.environ["ACS_DLIB"],
        "/dmdf", os.environ["ACS_JSON"],
        target,
    ]
    print(f"aax-signtool shim: signing {target}", flush=True)
    return subprocess.run(cmd).returncode


if __name__ == "__main__":
    sys.exit(main())
