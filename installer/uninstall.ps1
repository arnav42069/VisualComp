# VisualComp 2.28 — Azazel Audio — uninstaller
$ErrorActionPreference = 'SilentlyContinue'

Write-Host ''
Write-Host '  Removing VisualComp 2.28...' -ForegroundColor Yellow

$targets = @(
    'C:\Program Files\Common Files\VST3\VisualComp 2.28.vst3',
    'C:\Program Files\Azazel Audio\VisualComp 2.28',
    # earlier releases, removed too so no duplicates are left in the DAW
    'C:\Program Files\Common Files\VST3\VisualComp 2.27.vst3',
    'C:\Program Files\Azazel Audio\VisualComp 2.27',
    'C:\Program Files\Common Files\VST3\VisualComp 2.26.vst3',
    'C:\Program Files\Azazel Audio\VisualComp 2.26',
    'C:\Program Files\Common Files\VST3\VisualComp 2.25.vst3',
    'C:\Program Files\Azazel Audio\VisualComp 2.25',
    'C:\Program Files\Common Files\VST3\VisualComp 2.24.vst3',
    'C:\Program Files\Azazel Audio\VisualComp 2.24',
    'C:\Program Files\Common Files\VST3\VisualComp 2.23.vst3',
    'C:\Program Files\Azazel Audio\VisualComp 2.23',
    'C:\Program Files\Common Files\VST3\VisualComp 2.22.vst3',
    'C:\Program Files\Azazel Audio\VisualComp 2.22',
    'C:\Program Files\Common Files\VST3\VisualComp 2.21.vst3',
    'C:\Program Files\Azazel Audio\VisualComp 2.21',
    'C:\Program Files\Common Files\VST3\VisualComp 2.2.vst3',
    'C:\Program Files\Azazel Audio\VisualComp 2.2',
    'C:\Program Files\Common Files\VST3\VisualComp 2.1.vst3',
    'C:\Program Files\Azazel Audio\VisualComp 2.1',
    'C:\Program Files\Common Files\VST3\VisualComp 2.vst3',
    'C:\Program Files\Azazel Audio\VisualComp 2',
    (Join-Path $env:ProgramData 'Microsoft\Windows\Start Menu\Programs\Azazel Audio')
)

foreach ($t in $targets) {
    if (Test-Path -LiteralPath $t) {
        Remove-Item -LiteralPath $t -Recurse -Force
        if (Test-Path -LiteralPath $t) {
            Write-Host "  LOCKED (close your DAW and retry): $t" -ForegroundColor Red
        } else {
            Write-Host "  removed: $t" -ForegroundColor DarkGray
        }
    }
}

Write-Host ''
Write-Host '  Your presets and settings were left untouched:' -ForegroundColor White
Write-Host '    Documents\Azazel Audio\VisualComp 2\Presets'
Write-Host ''
Write-Host '  Done.' -ForegroundColor Green
Write-Host ''
