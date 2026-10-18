# Packaging script for zynamfx Windows releases
# Run from repository root: powershell -ExecutionPolicy Bypass -File script/package-windows-release.ps1

param (
    [string]$ArtefactsDir = "build/plugin/TONE3000_artefacts/Release",
    [string]$OutputDir = "release",
    [switch]$SkipInstaller = $false
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot

# Read version
$VersionFile = Join-Path $RepoRoot "VERSION"
if (Test-Path $VersionFile) {
    $Version = (Get-Content $VersionFile).Trim()
} else {
    $Version = "2.2.0"
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Packaging zynamfx v$Version (Windows x64)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$AbsArtefactsDir = Resolve-Path (Join-Path $RepoRoot $ArtefactsDir) -ErrorAction SilentlyContinue
if (-not $AbsArtefactsDir -or -not (Test-Path $AbsArtefactsDir)) {
    Write-Error "Artefacts directory not found: $ArtefactsDir. Please build the project first."
}

$AbsOutputDir = Join-Path $RepoRoot $OutputDir
if (-not (Test-Path $AbsOutputDir)) {
    New-Item -ItemType Directory -Path $AbsOutputDir -Force | Out-Null
}

$TempStage = Join-Path $AbsOutputDir "staging_temp"
if (Test-Path $TempStage) {
    Remove-Item -Recurse -Force $TempStage
}
New-Item -ItemType Directory -Path $TempStage -Force | Out-Null

try {
    # 1. Package Standalone
    $StandaloneExe = Join-Path $AbsArtefactsDir "Standalone\zynamfx.exe"
    if (Test-Path $StandaloneExe) {
        Write-Host "Packaging Standalone..." -ForegroundColor Green
        $StageStandalone = Join-Path $TempStage "standalone"
        New-Item -ItemType Directory -Path $StageStandalone -Force | Out-Null
        Copy-Item $StandaloneExe -Destination $StageStandalone

        $ReadmeContent = @"
zynamfx v$Version - Standalone (Windows x64)
============================================

Quick Start:
1. Double click zynamfx.exe to run.
2. Open Settings (gear icon) -> Audio to select your audio device.
   - For lowest latency, select your Audio Interface's native ASIO driver.
3. Plug in your guitar, load tones, or load your own .nam / IR files!

For issues and updates:
https://github.com/zackyusof/zynamfx
"@
        Set-Content -Path (Join-Path $StageStandalone "README.txt") -Value $ReadmeContent

        $StandaloneZip = Join-Path $AbsOutputDir "zynamfx-$Version-windows-x64-standalone.zip"
        if (Test-Path $StandaloneZip) { Remove-Item -Force $StandaloneZip }
        Compress-Archive -Path "$StageStandalone\*" -DestinationPath $StandaloneZip
        Write-Host " -> Created $StandaloneZip" -ForegroundColor Yellow
    } else {
        Write-Warning "Standalone executable not found at $StandaloneExe"
    }

    # 2. Package VST3
    $Vst3Dir = Join-Path $AbsArtefactsDir "VST3\zynamfx.vst3"
    if (Test-Path $Vst3Dir) {
        Write-Host "Packaging VST3..." -ForegroundColor Green
        $StageVst3 = Join-Path $TempStage "vst3"
        New-Item -ItemType Directory -Path $StageVst3 -Force | Out-Null
        Copy-Item -Recurse $Vst3Dir -Destination $StageVst3

        $Vst3Readme = @"
zynamfx v$Version - VST3 Plugin (Windows x64)
=============================================

Installation:
1. Copy the entire 'zynamfx.vst3' directory into your standard Windows VST3 folder:
   C:\Program Files\Common Files\VST3\

2. Rescan plugins in your DAW (Reaper, Cubase, Ableton, FL Studio, Studio One, etc.).

3. Insert 'zynamfx' on your audio track.
"@
        Set-Content -Path (Join-Path $StageVst3 "INSTALL.txt") -Value $Vst3Readme

        $Vst3Zip = Join-Path $AbsOutputDir "zynamfx-$Version-windows-x64-vst3.zip"
        if (Test-Path $Vst3Zip) { Remove-Item -Force $Vst3Zip }
        Compress-Archive -Path "$StageVst3\*" -DestinationPath $Vst3Zip
        Write-Host " -> Created $Vst3Zip" -ForegroundColor Yellow
    } else {
        Write-Warning "VST3 bundle not found at $Vst3Dir"
    }

    # 3. Package Complete Bundle (Standalone + VST3 + CLAP + Factory Presets)
    Write-Host "Packaging Complete Bundle..." -ForegroundColor Green
    $StageComplete = Join-Path $TempStage "complete"
    New-Item -ItemType Directory -Path $StageComplete -Force | Out-Null

    if (Test-Path $StandaloneExe) {
        $CompleteStandaloneDir = Join-Path $StageComplete "Standalone"
        New-Item -ItemType Directory -Path $CompleteStandaloneDir -Force | Out-Null
        Copy-Item $StandaloneExe -Destination $CompleteStandaloneDir
    }
    if (Test-Path $Vst3Dir) {
        Copy-Item -Recurse $Vst3Dir -Destination $StageComplete
    }
    $ClapFile = Join-Path $AbsArtefactsDir "CLAP\zynamfx.clap"
    if (Test-Path $ClapFile) {
        Copy-Item $ClapFile -Destination $StageComplete
    }
    $PresetsSrc = Join-Path $RepoRoot "resources\factory-presets"
    if (Test-Path $PresetsSrc) {
        $CompletePresetsDir = Join-Path $StageComplete "Factory Presets"
        New-Item -ItemType Directory -Path $CompletePresetsDir -Force | Out-Null
        Copy-Item (Join-Path $PresetsSrc "*.t3kpreset") -Destination $CompletePresetsDir
    }

    $CompleteReadme = @"
zynamfx v$Version - Complete Windows x64 Bundle
===============================================

Included components:
- Standalone\zynamfx.exe : Standalone application (with ASIO audio driver support)
- zynamfx.vst3           : VST3 plugin (copy to C:\Program Files\Common Files\VST3\)
- zynamfx.clap           : CLAP plugin (copy to C:\Program Files\Common Files\CLAP\)
- Factory Presets\       : Shipped factory presets (.t3kpreset)
                           Optional manual install: %ProgramData%\zynamfx\Presets\Factory\
                           or user presets: %APPDATA%\zynamfx\Presets\

License: MIT
Repository: https://github.com/zackyusof/zynamfx
"@
    Set-Content -Path (Join-Path $StageComplete "README.txt") -Value $CompleteReadme

    $CompleteZip = Join-Path $AbsOutputDir "zynamfx-$Version-windows-x64-complete.zip"
    if (Test-Path $CompleteZip) { Remove-Item -Force $CompleteZip }
    Compress-Archive -Path "$StageComplete\*" -DestinationPath $CompleteZip
    Write-Host " -> Created $CompleteZip" -ForegroundColor Yellow

    # 4. Inno Setup Compiler (.exe Installer) if available
    if (-not $SkipInstaller) {
        $IsccCmd = Get-Command "iscc" -ErrorAction SilentlyContinue
        $IsccPath = ""
        if ($IsccCmd) {
            $IsccPath = $IsccCmd.Source
        } elseif (Test-Path "C:\Program Files (x86)\Inno Setup 6\ISCC.exe") {
            $IsccPath = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
        } elseif (Test-Path "C:\Program Files\Inno Setup 6\ISCC.exe") {
            $IsccPath = "C:\Program Files\Inno Setup 6\ISCC.exe"
        }

        if ($IsccPath) {
            Write-Host "Compiling Inno Setup installer using $IsccPath..." -ForegroundColor Green
            $IssFile = Join-Path $RepoRoot "script\installer\windows\zynamfx.iss"
            & $IsccPath "/DVersion=$Version" "/DArtefactsDir=$AbsArtefactsDir" "/DOutputDir=$AbsOutputDir" $IssFile
            Write-Host " -> Installer compiled successfully!" -ForegroundColor Yellow
        } else {
            Write-Host "Note: Inno Setup (ISCC) not found on PATH. Skipped installer creation (zip packages ready)." -ForegroundColor Cyan
        }
    }

} finally {
    if (Test-Path $TempStage) {
        Remove-Item -Recurse -Force $TempStage
    }
}

Write-Host "`nRelease files created in ${AbsOutputDir}:" -ForegroundColor Green
Get-ChildItem -Path $AbsOutputDir -Filter "zynamfx-$Version-windows-x64-*" | ForEach-Object {
    $Hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash
    Write-Host ("  - {0,-45} ({1:N2} MB)  SHA256: {2}" -f $_.Name, ($_.Length / 1MB), $Hash.Substring(0, 16))
}
Write-Host "Done!" -ForegroundColor Green
