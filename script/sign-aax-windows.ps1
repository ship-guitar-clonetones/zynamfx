# Sign the AAX bundle with PACE Eden cloud signing (Windows).
#
# Pro Tools refuses to load an AAX plugin that is not signed by PACE's
# wraptool, and wraptool needs an authorized iLok signing license. In CI
# there is no physical iLok, so this uses PACE's cloud signing: iloktool
# opens a headless iLok Cloud session, wraptool signs against it
# (--allowsigningservice), and the session is closed afterwards.
#
# On Windows the AAX signature has two halves: the PACE/Avid signature and a
# regular Authenticode signature on the inner DLL. This repo's Authenticode
# signing runs through Azure Artifact Signing (Trusted Signing), which has no
# local certificate for wraptool to use, so wraptool is pointed at a shim
# (--signtool script/pace/aax-signtool.bat) that re-invokes the real
# signtool.exe with the Artifact Signing dlib. Run this AFTER the build and
# BEFORE the Inno Setup installer is compiled.
#
# Required environment:
#   PACE_ILOK_ACCOUNT       iLok User ID approved for AAX cloud signing
#   PACE_ILOK_PASSWORD      password for that account
#   PACE_WCGUID             wrap configuration GUID (PACE Central / Eden)
#   PACE_TOOLS_URL          URL to a .zip containing wraptool.exe and
#                           iloktool.exe (Eden SDK binaries from PACE Connect;
#                           NOT redistributable, host them privately)
#   PACE_TOOLS_TOKEN        optional bearer token for PACE_TOOLS_URL
#   PACE_ILM_RESPONSE_ISS   base64 of an InstallShield response file recorded
#                           once with: "License Support Win64.exe" -r -f1"C:\r.iss"
#                           (needed to install PACE License Support silently;
#                           see the CircleCI guide "How to install iLok
#                           License Support Installer silently")
#   AZURE_TENANT_ID         service principal the Artifact Signing dlib
#   AZURE_CLIENT_ID           authenticates with (same values the
#   AZURE_CLIENT_SECRET       azure/artifact-signing-action steps use)
#   AZURE_SIGNING_ENDPOINT  e.g. https://wus2.codesigning.azure.net
#   AZURE_SIGNING_ACCOUNT   Artifact Signing account name
#   AZURE_SIGNING_PROFILE   certificate profile name
#
# Usage: pwsh -File script/sign-aax-windows.ps1 path\to\TONE3000.aaxplugin

param(
    [Parameter(Mandatory = $true)][string]$AaxBundle
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

if (-not (Test-Path $AaxBundle)) {
    throw "AAX bundle not found: $AaxBundle"
}
foreach ($name in @(
        'PACE_ILOK_ACCOUNT', 'PACE_ILOK_PASSWORD', 'PACE_WCGUID', 'PACE_TOOLS_URL',
        'AZURE_TENANT_ID', 'AZURE_CLIENT_ID', 'AZURE_CLIENT_SECRET',
        'AZURE_SIGNING_ENDPOINT', 'AZURE_SIGNING_ACCOUNT', 'AZURE_SIGNING_PROFILE')) {
    if (-not (Get-Item "env:$name" -ErrorAction SilentlyContinue).Value) {
        throw "Required environment variable not set: $name"
    }
}

$baseTemp = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$workDir = Join-Path $baseTemp 'pace-sign'
New-Item -ItemType Directory -Force -Path $workDir | Out-Null
if ($workDir -match ' ') {
    Write-Warning "Work dir contains spaces ($workDir); wraptool --signtool may misbehave."
}

# 1. PACE License Support (drivers/services iloktool and wraptool talk to).
#    Public installer, but InstallShield-based: silent install needs a
#    response file recorded once on any Windows machine with a GUI.
$paceInstalled = Test-Path (Join-Path $env:CommonProgramFiles 'PACE')
if (-not $paceInstalled) {
    if (-not $env:PACE_ILM_RESPONSE_ISS) {
        throw 'PACE License Support is not installed and PACE_ILM_RESPONSE_ISS is not set.'
    }
    Write-Host 'Installing PACE License Support...'
    $lsZip = Join-Path $workDir 'LicenseSupport.zip'
    Invoke-WebRequest -Uri 'https://installers.ilok.com/iloklicensemanager/LicenseSupportInstallerWin64.zip' -OutFile $lsZip
    $lsDir = Join-Path $workDir 'license-support'
    Expand-Archive -Path $lsZip -DestinationPath $lsDir -Force
    $installer = Get-ChildItem -Path $lsDir -Recurse -Filter '*.exe' |
        Where-Object { $_.Name -like '*License Support*' } | Select-Object -First 1
    if (-not $installer) {
        throw "License Support installer exe not found under $lsDir"
    }
    $issPath = Join-Path $workDir 'license-support.iss'
    [IO.File]::WriteAllBytes($issPath, [Convert]::FromBase64String($env:PACE_ILM_RESPONSE_ISS))

    Start-Process -Wait -FilePath $installer.FullName -ArgumentList '-s', "-f1`"$issPath`""
    # InstallShield's -s can return before the actual install finishes; wait
    # for the PACE services directory to appear before continuing.
    $deadline = (Get-Date).AddSeconds(180)
    while (-not (Test-Path (Join-Path $env:CommonProgramFiles 'PACE'))) {
        if ((Get-Date) -gt $deadline) {
            throw 'PACE License Support install did not complete within 180s.'
        }
        Start-Sleep -Seconds 5
    }
}
else {
    Write-Host 'PACE License Support already installed.'
}

# 2. Eden tools (wraptool.exe + iloktool.exe) from private hosting.
Write-Host 'Fetching Eden tools...'
$toolsZip = Join-Path $workDir 'eden-tools.zip'
$headers = @{}
if ($env:PACE_TOOLS_TOKEN) {
    # Accept header makes private GitHub release-asset API URLs download the
    # binary instead of the asset's JSON metadata; harmless elsewhere.
    $headers['Authorization'] = "Bearer $($env:PACE_TOOLS_TOKEN)"
    $headers['Accept'] = 'application/octet-stream'
}
Invoke-WebRequest -Uri $env:PACE_TOOLS_URL -Headers $headers -OutFile $toolsZip
$toolsDir = Join-Path $workDir 'eden'
Expand-Archive -Path $toolsZip -DestinationPath $toolsDir -Force

# wraptool ships in the Eden SDK (so it must be in the zip); iloktool is part
# of the License Support install (like /usr/local/bin/iloktool on macOS), so
# fall back to searching the PACE install locations when the zip doesn't
# carry it.
function Find-Tool([string]$name) {
    $hit = Get-ChildItem -Path $toolsDir -Recurse -Filter $name -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $hit) {
        foreach ($root in @(
                (Join-Path $env:CommonProgramFiles 'PACE'),
                (Join-Path $env:ProgramFiles 'PACE Anti-Piracy'))) {
            if (Test-Path $root) {
                $hit = Get-ChildItem -Path $root -Recurse -Filter $name -ErrorAction SilentlyContinue |
                    Select-Object -First 1
                if ($hit) { break }
            }
        }
    }
    if (-not $hit) { throw "$name not found in the Eden tools zip or the PACE install directories." }
    return $hit.FullName
}
$wraptool = Find-Tool 'wraptool.exe'
$iloktool = Find-Tool 'iloktool.exe'
Write-Host "wraptool: $wraptool"
Write-Host "iloktool: $iloktool"

# 3. signtool.exe + Azure Artifact Signing dlib, driven through the shim.
#    NuGet-based install per the Microsoft docs; the SDK BuildTools package is
#    required because only signtool >= 10.0.22621.755 supports /dlib.
Write-Host 'Installing signtool + Artifact Signing client...'
$nuget = Join-Path $workDir 'nuget.exe'
Invoke-WebRequest -Uri 'https://dist.nuget.org/win-x86-commandline/latest/nuget.exe' -OutFile $nuget
$pkgDir = Join-Path $workDir 'packages'
& $nuget install Microsoft.Windows.SDK.BuildTools -x -NonInteractive -OutputDirectory $pkgDir | Out-Null
# Package was renamed with the Trusted Signing -> Artifact Signing rebrand;
# try the new id first, fall back to the old one.
& $nuget install Microsoft.ArtifactSigning.Client -x -NonInteractive -OutputDirectory $pkgDir | Out-Null
if ($LASTEXITCODE -ne 0) {
    & $nuget install Microsoft.Trusted.Signing.Client -x -NonInteractive -OutputDirectory $pkgDir | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Failed to install the Artifact Signing client package.' }
}
$signtool = Get-ChildItem -Path $pkgDir -Recurse -Filter 'signtool.exe' |
    Where-Object { $_.FullName -match '\\x64\\' } | Select-Object -First 1
$dlib = Get-ChildItem -Path $pkgDir -Recurse -Filter 'Azure.CodeSigning.Dlib.dll' |
    Where-Object { $_.FullName -match '\\x64\\' } | Select-Object -First 1
if (-not $signtool -or -not $dlib) {
    throw "signtool.exe or Azure.CodeSigning.Dlib.dll not found under $pkgDir"
}

$metadata = Join-Path $workDir 'metadata.json'
@{
    Endpoint               = $env:AZURE_SIGNING_ENDPOINT
    CodeSigningAccountName = $env:AZURE_SIGNING_ACCOUNT
    CertificateProfileName = $env:AZURE_SIGNING_PROFILE
} | ConvertTo-Json | Set-Content -Path $metadata -Encoding ascii

# The shim reads these (AZURE_* are already in the environment).
$env:SIGNTOOL_PATH = $signtool.FullName
$env:ACS_DLIB = $dlib.FullName
$env:ACS_JSON = $metadata

# Copy the shim next to the tools: --signtool gets a short, space-free path,
# independent of where the repo is checked out.
$shimDir = Join-Path $workDir 'shim'
New-Item -ItemType Directory -Force -Path $shimDir | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'pace\aax-signtool.bat') $shimDir
Copy-Item (Join-Path $PSScriptRoot 'pace\aax-signtool.py') $shimDir
$shim = Join-Path $shimDir 'aax-signtool.bat'
if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    throw 'python not found on PATH (the signtool shim needs it).'
}

# 4. Open the iLok Cloud session, sign, and always close the session again
#    (a stale open session can lock the license until it times out).
Write-Host 'Opening iLok Cloud session...'
& $iloktool cloud --open --account $env:PACE_ILOK_ACCOUNT --password $env:PACE_ILOK_PASSWORD -v
if ($LASTEXITCODE -ne 0) { throw "iloktool cloud --open failed ($LASTEXITCODE)" }

try {
    # --signid 1 is a placeholder wraptool insists on; the shim discards it
    # and signs with the Artifact Signing dlib instead.
    Write-Host "Signing $AaxBundle with wraptool..."
    & $wraptool sign --verbose `
        --signtool $shim `
        --signid 1 `
        --installedbinaries `
        --account $env:PACE_ILOK_ACCOUNT `
        --password $env:PACE_ILOK_PASSWORD `
        --wcguid $env:PACE_WCGUID `
        --allowsigningservice `
        --in $AaxBundle `
        --out $AaxBundle
    if ($LASTEXITCODE -ne 0) { throw "wraptool sign failed ($LASTEXITCODE)" }

    # Best-effort: verify can require a full installed Eden SDK; a verify
    # failure after a successful sign is a tooling/environment problem, not
    # a signature problem, so it must never block a release.
    Write-Host 'Verifying PACE signature...'
    & $wraptool verify --verbose --in $AaxBundle
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "wraptool verify failed ($LASTEXITCODE); non-fatal, sign already succeeded."
    }
}
finally {
    Write-Host 'Closing iLok Cloud session...'
    & $iloktool cloud --close -v
}

Write-Host "AAX signed: $AaxBundle"
