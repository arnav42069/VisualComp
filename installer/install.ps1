# VisualComp 2.26 — Azazel Audio
# Installs the VST3 plugin and the standalone application.
# Launched by "Install VisualComp 2.26.bat", which handles the admin elevation.

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

$vst3Src   = Join-Path $here 'VisualComp 2.26.vst3'
$exeSrc    = Join-Path $here 'VisualComp 2.26.exe'
$vst3Dest  = 'C:\Program Files\Common Files\VST3\VisualComp 2.26.vst3'
$appDir    = 'C:\Program Files\Azazel Audio\VisualComp 2.26'

Write-Host ''
Write-Host '  ================================================' -ForegroundColor DarkYellow
Write-Host '   VisualComp 2.26  -  Azazel Audio' -ForegroundColor Yellow
Write-Host '   See your compression. Shape your sound.' -ForegroundColor DarkGray
Write-Host '  ================================================' -ForegroundColor DarkYellow
Write-Host ''

if (-not (Test-Path -LiteralPath $vst3Src)) {
    Write-Host "  ERROR: 'VisualComp 2.26.vst3' not found next to this script." -ForegroundColor Red
    Write-Host "  Keep all files in the Windows folder together and run again." -ForegroundColor Red
    exit 1
}

# --- 1. VST3 plugin ---------------------------------------------------------
Write-Host '  [1/3] Installing VST3 plugin...' -ForegroundColor Cyan
try {
    if (Test-Path -LiteralPath $vst3Dest) { Remove-Item -LiteralPath $vst3Dest -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $vst3Dest | Out-Null
    Copy-Item -Path (Join-Path $vst3Src '*') -Destination $vst3Dest -Recurse -Force
    Write-Host "        -> $vst3Dest" -ForegroundColor DarkGray
}
catch {
    Write-Host '  ERROR: could not write to the VST3 folder.' -ForegroundColor Red
    Write-Host '  Close your DAW (it locks the plugin file) and run this installer again.' -ForegroundColor Red
    exit 1
}

# --- 2. Standalone application ---------------------------------------------
Write-Host '  [2/3] Installing standalone application...' -ForegroundColor Cyan
if (Test-Path -LiteralPath $exeSrc) {
    New-Item -ItemType Directory -Force -Path $appDir | Out-Null
    Copy-Item -LiteralPath $exeSrc -Destination (Join-Path $appDir 'VisualComp 2.26.exe') -Force
    Write-Host "        -> $appDir" -ForegroundColor DarkGray
} else {
    Write-Host '        (standalone not included in this package - skipped)' -ForegroundColor DarkGray
}

# --- 3. Start Menu shortcut -------------------------------------------------
Write-Host '  [3/3] Creating Start Menu shortcut...' -ForegroundColor Cyan
try {
    $startMenu = Join-Path $env:ProgramData 'Microsoft\Windows\Start Menu\Programs\Azazel Audio'
    New-Item -ItemType Directory -Force -Path $startMenu | Out-Null
    $exePath = Join-Path $appDir 'VisualComp 2.26.exe'
    if (Test-Path -LiteralPath $exePath) {
        $shell = New-Object -ComObject WScript.Shell
        $lnk = $shell.CreateShortcut((Join-Path $startMenu 'VisualComp 2.26.lnk'))
        $lnk.TargetPath       = $exePath
        $lnk.WorkingDirectory = $appDir
        $lnk.Description      = 'VisualComp 2.26 - Azazel Audio'
        $lnk.Save()
        Write-Host "        -> Start Menu > Azazel Audio > VisualComp 2.26" -ForegroundColor DarkGray
    }
}
catch { Write-Host '        (shortcut skipped)' -ForegroundColor DarkGray }

Write-Host ''
Write-Host '  INSTALLATION COMPLETE' -ForegroundColor Green
Write-Host ''
Write-Host '  Next steps:' -ForegroundColor White
Write-Host '    1. Open your DAW and rescan plugins.'
Write-Host '       FL Studio:  Options > Manage plugins > Find more plugins'
Write-Host '       Others:     rescan in plugin preferences'
Write-Host '    2. The plugin appears as "VisualComp 2.26" by Azazel Audio (Dynamics).'
Write-Host ''
Write-Host '  If you already had it loaded, remove the plugin instance and re-add it' -ForegroundColor DarkGray
Write-Host '  so your DAW picks up the new version.' -ForegroundColor DarkGray
Write-Host ''
