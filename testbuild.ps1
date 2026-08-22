# Builds the VST3 (installing it into the system VST3 folder) plus Standalone,
# then drops the executable into "Build Final\Standalone Test" and launches it.
# This script owns the full versioned quick-build loop.

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

& (Join-Path $root 'bump-version.ps1')
& cmake --build (Join-Path $root 'build') --config Release --target VisualComp_VST3 VisualComp_Standalone
if ($LASTEXITCODE -ne 0) { throw "VST3/Standalone build failed with exit code $LASTEXITCODE" }

$standaloneDir = Join-Path $root 'build\VisualComp_artefacts\Release\Standalone'
if (-not (Test-Path -LiteralPath $standaloneDir)) {
    throw "No Standalone build output found at $standaloneDir"
}

$exeSrc = Get-ChildItem -LiteralPath $standaloneDir -Filter '*.exe' |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $exeSrc) { throw "No .exe found in $standaloneDir" }

$destDir = Join-Path $root 'Build Final\Standalone Test'
New-Item -ItemType Directory -Force -Path $destDir | Out-Null
$destExe = Join-Path $destDir $exeSrc.Name
Copy-Item -LiteralPath $exeSrc.FullName -Destination $destExe -Force

Write-Host ''
Write-Host ("  Test build: {0}" -f $destExe) -ForegroundColor Green
Write-Host ''

Start-Process -FilePath $destExe
