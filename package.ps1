# Assembles the VisualComp 2.21 release bundle and zips it into "Build Final".
# Run after a Release build of VisualComp_VST3 and VisualComp_Standalone.

$ErrorActionPreference = 'Stop'
$root    = Split-Path -Parent $MyInvocation.MyCommand.Path
$version = '2.21'

# Use whichever build tree holds the newest plugin. build-pkg is built with
# -DVC2_INSTALL_PLUGIN=OFF so it never touches files a running DAW has locked.
$candidates = @(
    (Join-Path $root 'build\VisualComp_artefacts\Release'),
    (Join-Path $root 'build-pkg\VisualComp_artefacts\Release')
) | Where-Object { Test-Path -LiteralPath (Join-Path $_ "VST3\VisualComp $version.vst3") }

if (-not $candidates) { throw "No build output found for VisualComp $version" }
$artefacts = $candidates |
    Sort-Object { (Get-Item -LiteralPath (Join-Path $_ "VST3\VisualComp $version.vst3\Contents\x86_64-win\VisualComp $version.vst3")).LastWriteTime } -Descending |
    Select-Object -First 1
$vst3Src   = Join-Path $artefacts "VST3\VisualComp $version.vst3"
$exeSrc    = Join-Path $artefacts "Standalone\VisualComp $version.exe"
$manual    = Join-Path $root "docs\VisualComp $version - User Manual.pdf"

foreach ($p in @($vst3Src, $exeSrc, $manual)) {
    if (-not (Test-Path -LiteralPath $p)) { throw "Missing build output: $p" }
}

$buildFinal = Join-Path $root 'Build Final'
$stage      = Join-Path $buildFinal "_stage"
$bundleName = "VisualComp $version - Windows and Mac"
$stageRoot  = Join-Path $stage $bundleName

New-Item -ItemType Directory -Force -Path $buildFinal | Out-Null
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stageRoot | Out-Null

# ── Root: readme + manual ───────────────────────────────────────────────────
Copy-Item (Join-Path $root 'installer\README.txt') (Join-Path $stageRoot 'README.txt') -Force
Copy-Item $manual (Join-Path $stageRoot "VisualComp $version - User Manual.pdf") -Force

# ── Windows: installer + plugin + standalone ────────────────────────────────
$win = Join-Path $stageRoot 'Windows'
New-Item -ItemType Directory -Force -Path $win | Out-Null
Copy-Item (Join-Path $root "installer\Install VisualComp $version.bat")   $win -Force
Copy-Item (Join-Path $root "installer\Uninstall VisualComp $version.bat") $win -Force
Copy-Item (Join-Path $root 'installer\install.ps1')                $win -Force
Copy-Item (Join-Path $root 'installer\uninstall.ps1')              $win -Force
Copy-Item $exeSrc $win -Force
$vst3Dst = Join-Path $win "VisualComp $version.vst3"
New-Item -ItemType Directory -Force -Path $vst3Dst | Out-Null
Copy-Item (Join-Path $vst3Src '*') $vst3Dst -Recurse -Force

# ── Mac: build steps + full source ──────────────────────────────────────────
$mac = Join-Path $stageRoot 'Mac'
New-Item -ItemType Directory -Force -Path $mac | Out-Null
Copy-Item (Join-Path $root 'build-macos\README.txt') (Join-Path $mac 'README - Mac Build Steps.txt') -Force
Copy-Item (Join-Path $root 'build-macos\build.sh')   (Join-Path $mac 'build.sh') -Force

$srcOut = Join-Path $mac 'Source'
New-Item -ItemType Directory -Force -Path $srcOut | Out-Null
Copy-Item (Join-Path $root 'CMakeLists.txt') $srcOut -Force
Copy-Item (Join-Path $root 'src')         (Join-Path $srcOut 'src')         -Recurse -Force
Copy-Item (Join-Path $root 'build-macos') (Join-Path $srcOut 'build-macos') -Recurse -Force

# ── Zip ─────────────────────────────────────────────────────────────────────
$zip = Join-Path $buildFinal "$bundleName.zip"
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -Path $stageRoot -DestinationPath $zip -CompressionLevel Optimal
Remove-Item -LiteralPath $stage -Recurse -Force

# ── Loose standalone exe for quick manual testing (not zipped) ──────────────
$exeDst = Join-Path $buildFinal "VisualComp $version.exe"
Copy-Item $exeSrc $exeDst -Force

$size = (Get-Item -LiteralPath $zip).Length / 1MB
Write-Host ''
Write-Host ("  Packaged : {0}" -f $zip) -ForegroundColor Green
Write-Host ("  Size     : {0:N1} MB" -f $size) -ForegroundColor DarkGray
Write-Host ("  Test exe : {0}" -f $exeDst) -ForegroundColor Green
Write-Host ''
