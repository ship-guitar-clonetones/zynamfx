#!/bin/bash
# Sign the AAX bundle with PACE Eden cloud signing (macOS).
#
# Pro Tools refuses to load an AAX plugin that is not signed by PACE's
# wraptool, and wraptool needs an authorized iLok signing license. In CI
# there is no physical iLok, so this uses PACE's cloud signing: iloktool
# opens a headless iLok Cloud session, wraptool signs against it
# (--allowsigningservice), and the session is closed afterwards.
#
# wraptool applies BOTH signatures in one pass:
#   - the PACE/Avid signature (authorized by the cloud session + WCGUID)
#   - the Apple Developer ID signature (--signid), with the codesign options
#     notarization requires (--dsigharden = hardened runtime + timestamp)
# so the bundle must NOT be codesigned again afterwards; create-pkg.sh
# detects the existing Developer ID signature on the AAX and leaves it alone.
# Run this AFTER the universal lipo merge and BEFORE create-pkg.sh.
#
# Required environment:
#   PACE_ILOK_ACCOUNT   iLok User ID approved for AAX cloud signing
#   PACE_ILOK_PASSWORD  password for that account
#   PACE_WCGUID         wrap configuration GUID (shown in your PACE Central /
#                       Eden account for the Avid AAX signing certificate)
#   SIGN_ID_APP         'Developer ID Application: Name (TEAMID)' identity,
#                       already imported into an unlocked keychain
#
# Tool discovery (wraptool / iloktool are Eden SDK binaries from PACE Connect
# and are NOT redistributable, so they are never committed to this repo):
#   PACE_TOOLS_URL      optional URL to a .zip containing the macOS `wraptool`
#                       (and optionally `iloktool`), hosted privately (S3
#                       presigned URL, private GitHub release asset, ...)
#   PACE_TOOLS_TOKEN    optional bearer token for PACE_TOOLS_URL (needed for
#                       private GitHub release assets)
#   ...otherwise both tools are looked up on PATH and in the local Eden
#   install (/Applications/PACEAntiPiracy/Eden/Fusion/Current/bin).
#
# The PACE License Support drivers (public download from installers.ilok.com)
# are installed automatically if missing; that install needs sudo.
#
# Usage: ./script/sign-aax-macos.sh path/to/TONE3000.aaxplugin

set -euo pipefail

AAX_BUNDLE="${1:?usage: sign-aax-macos.sh path/to/plugin.aaxplugin}"

if [[ "$(uname)" != "Darwin" ]]; then
  echo "This script must run on macOS." >&2
  exit 1
fi
if [[ ! -d "$AAX_BUNDLE" ]]; then
  echo "AAX bundle not found: $AAX_BUNDLE" >&2
  exit 1
fi
: "${PACE_ILOK_ACCOUNT:?PACE_ILOK_ACCOUNT not set}"
: "${PACE_ILOK_PASSWORD:?PACE_ILOK_PASSWORD not set}"
: "${PACE_WCGUID:?PACE_WCGUID not set}"
: "${SIGN_ID_APP:?SIGN_ID_APP not set}"

WORK_DIR="$(mktemp -d /tmp/pace-tools.XXXXXX)"
cleanup_workdir() { rm -rf "$WORK_DIR"; }

# 1. PACE License Support (drivers/services iloktool and wraptool talk to).
#    Public installer; skip when a PACE package is already on the machine.
if ! pkgutil --pkgs 2>/dev/null | grep -qi 'paceap'; then
  echo "Installing PACE License Support..."
  curl -fsSL "https://installers.ilok.com/iloklicensemanager/LicenseSupportInstallerMac.zip" \
    -o "$WORK_DIR/LicenseSupport.zip"
  unzip -q "$WORK_DIR/LicenseSupport.zip" -d "$WORK_DIR/license-support"
  DMG=$(find "$WORK_DIR/license-support" -name '*.dmg' | head -n 1)
  MOUNT_DIR=$(hdiutil attach "$DMG" -nobrowse | awk -F'\t' '/\/Volumes\// {print $NF}' | head -n 1)
  PKG=$(find "$MOUNT_DIR" -maxdepth 1 -name '*.pkg' | head -n 1)
  sudo installer -pkg "$PKG" -target /
  hdiutil detach "$MOUNT_DIR" -quiet || true
else
  echo "PACE License Support already installed."
fi

# 2. Eden tools (wraptool + iloktool).
if [[ -n "${PACE_TOOLS_URL:-}" ]]; then
  echo "Fetching Eden tools..."
  AUTH_ARGS=()
  if [[ -n "${PACE_TOOLS_TOKEN:-}" ]]; then
    # Accept header makes private GitHub release-asset API URLs download the
    # binary instead of the asset's JSON metadata; harmless elsewhere.
    AUTH_ARGS=( -H "Authorization: Bearer ${PACE_TOOLS_TOKEN}" \
                -H "Accept: application/octet-stream" )
  fi
  curl -fsSL ${AUTH_ARGS[@]+"${AUTH_ARGS[@]}"} "$PACE_TOOLS_URL" -o "$WORK_DIR/eden-tools.zip"
  unzip -q "$WORK_DIR/eden-tools.zip" -d "$WORK_DIR/eden"
fi

find_tool() {
  local name="$1"
  local found=""
  # Downloaded bundle wins, then PATH, then a local Eden GUI install.
  found=$(find "$WORK_DIR/eden" -type f -name "$name" 2>/dev/null | head -n 1 || true)
  if [[ -z "$found" ]]; then
    found=$(command -v "$name" || true)
  fi
  if [[ -z "$found" && -x "/Applications/PACEAntiPiracy/Eden/Fusion/Current/bin/$name" ]]; then
    found="/Applications/PACEAntiPiracy/Eden/Fusion/Current/bin/$name"
  fi
  echo "$found"
}

WRAPTOOL=$(find_tool wraptool)
ILOKTOOL=$(find_tool iloktool)
if [[ -z "$WRAPTOOL" || -z "$ILOKTOOL" ]]; then
  echo "wraptool/iloktool not found (wraptool='$WRAPTOOL' iloktool='$ILOKTOOL')." >&2
  echo "Provide PACE_TOOLS_URL or install the Eden tools locally." >&2
  cleanup_workdir
  exit 1
fi
chmod +x "$WRAPTOOL" "$ILOKTOOL" 2>/dev/null || true
echo "wraptool: $WRAPTOOL"
echo "iloktool: $ILOKTOOL"

# 3. Open the iLok Cloud session; always close it again, even on failure
#    (a stale open session can lock the license until it times out).
close_session() {
  "$ILOKTOOL" cloud --close -v || true
  cleanup_workdir
}
trap close_session EXIT

echo "Opening iLok Cloud session..."
"$ILOKTOOL" cloud --open \
  --account "$PACE_ILOK_ACCOUNT" \
  --password "$PACE_ILOK_PASSWORD" \
  -v

# 4. Sign. --dsigharden sets the codesign options Apple notarization
#    requires; --dsig1-compat off drops the legacy v1 signature (only needed
#    by very old Pro Tools versions).
echo "Signing $AAX_BUNDLE with wraptool..."
"$WRAPTOOL" sign --verbose \
  --account "$PACE_ILOK_ACCOUNT" \
  --password "$PACE_ILOK_PASSWORD" \
  --wcguid "$PACE_WCGUID" \
  --signid "$SIGN_ID_APP" \
  --dsigharden \
  --dsig1-compat off \
  --allowsigningservice \
  --in "$AAX_BUNDLE" \
  --out "$AAX_BUNDLE"

echo "Verifying PACE signature..."
"$WRAPTOOL" verify --verbose --in "$AAX_BUNDLE"

echo "Verifying Apple signature..."
codesign --verify --deep --strict --verbose=2 "$AAX_BUNDLE"

echo "AAX signed: $AAX_BUNDLE"
