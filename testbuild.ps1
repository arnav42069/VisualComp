# Builds just the Standalone target and drops it into "Build Final\Standalone
# Test" for a fast build-and-play loop, then launches it. Companion to
# package.ps1/package-release, but skips the VST3 build and zip bundling.
# Run after a Release build of VisualComp_Standalone.

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

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
