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

    # wraptool composes something like: sign /sha1 <bogus id> <path to dll>.
    # The path may contain spaces and may or may not be quoted; grab the
    # first drive-letter path through the end of the line.
    match = re.search(r'"?([A-Za-z]:[\\/].*?)"?\s*$', raw)
    if not match:
        print("aax-signtool shim: no file path found in arguments", flush=True)
        return 1
    target = match.group(1).strip('"')

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
