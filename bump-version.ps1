# Increments VisualComp's version string across every file that needs to
# stay in sync (see CLAUDE.md's "Bumping the version" checklist -- this
# script exists precisely because that checklist was historically easy to
# apply incompletely: the DIST_DIR block and the manual's PDF path both went
# silently stale across the 2.1->2.2 bump). Run automatically by
# /package-release before the CMake build, so every packaged release gets a
# fresh PRODUCT_NAME/plugin ID -- the mechanism CLAUDE.md documents for
# getting FL Studio (or any host) to detect it as a new plugin instead of
# reusing a locked, already-loaded one.
#
# Usage: ./bump-version.ps1 [-NewVersion '2.22']
#   With no argument, adds 0.01 to the version as a real decimal number,
#   formatted back to two decimal places (2.21 -> 2.22 -> 2.23 -> ... ->
#   2.99 -> 3.00 -> 3.01 ...). This is real decimal arithmetic, not a
#   string-integer bump on the trailing component -- that earlier approach
#   broke on single-digit fractions (e.g. "2.9" + 1 on the suffix produced
#   "2.10", which is a jump of +0.91, not +0.01, and even sorts *before*
#   2.9 as a real number). Pass -NewVersion explicitly for anything else
#   (e.g. a major bump to '3.0').

param([string]$NewVersion)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

function Read-Utf8([string]$Path) { [System.IO.File]::ReadAllText($Path) }
function Write-Utf8([string]$Path, [string]$Content) {
    # No BOM -- a BOM would corrupt build.sh's #!/usr/bin/env bash shebang
    # and adds pointless diff noise everywhere else.
    $enc = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $enc)
}
function Replace-InFile([string]$Path, [string]$Old, [string]$New) {
    Write-Utf8 $Path ((Read-Utf8 $Path).Replace($Old, $New))
}

# ── 1. Determine old/new version ─────────────────────────────────────────
$cmakePath = Join-Path $root 'CMakeLists.txt'
$cmakeText = Read-Utf8 $cmakePath
$m = [regex]::Match($cmakeText, 'set\(VC2_VERSION_STRING "([\d.]+)"\)')
if (-not $m.Success) { throw "Couldn't find VC2_VERSION_STRING in CMakeLists.txt" }
$old = $m.Groups[1].Value

if (-not $NewVersion) {
    # Real decimal +0.01, not a string-integer bump on the trailing
    # component (see the usage comment above for why that broke on
    # single-digit fractions). [math]::Round guards against binary-float
    # noise (e.g. 2.22 + 0.01 landing on 2.2299999999999995) before it hits
    # formatting.
    $oldNum = [double]::Parse($old, [System.Globalization.CultureInfo]::InvariantCulture)
    $newNum = [math]::Round($oldNum + 0.01, 2)
    $new = $newNum.ToString('0.00', [System.Globalization.CultureInfo]::InvariantCulture)
} else {
    $new = $NewVersion
}
if ($new -eq $old) { throw "New version ($new) is the same as the old version -- refusing to bump." }

Write-Host ''
Write-Host ("  Bumping VisualComp {0} -> {1}" -f $old, $new) -ForegroundColor Cyan
Write-Host ''

# ── 2. Plain blanket-replace files (no historical content to protect) ───
$plainFiles = @(
    'CMakeLists.txt',
    'package.ps1',
    '.claude\skills\package-release\SKILL.md',
    '.claude\skills\testbuild\SKILL.md',
    'installer\install.ps1',
    'installer\README.txt',
    'build-macos\build.sh',
    'build-macos\README.txt'
)
foreach ($rel in $plainFiles) {
    $p = Join-Path $root $rel
    if (-not (Test-Path -LiteralPath $p)) { Write-Host "  SKIP (not found): $rel" -ForegroundColor DarkYellow; continue }
    Replace-InFile $p $old $new
    Write-Host "  updated: $rel" -ForegroundColor DarkGray
}

# ── 3. uninstall.ps1 -- current-version lines bump, AND the outgoing
#      version gets appended to the "earlier releases" legacy cleanup list
#      so upgraders' old install doesn't get orphaned. ───────────────────
$uninstallPath = Join-Path $root 'installer\uninstall.ps1'
if (Test-Path -LiteralPath $uninstallPath) {
    $lines = (Read-Utf8 $uninstallPath) -split "`r?`n"
    $commentIdx = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match 'earlier releases, removed too') { $commentIdx = $i; break }
    }
    if ($commentIdx -lt 2) {
        throw "uninstall.ps1: couldn't find the 'earlier releases' legacy-list marker -- check its structure before re-running."
    }
    # The two lines directly above the marker are the current-version entries.
    $oldVst3Line = $lines[$commentIdx - 2]
    $oldAppLine  = $lines[$commentIdx - 1]
    if ($oldVst3Line -notlike "*$old*" -or $oldAppLine -notlike "*$old*") {
        throw "uninstall.ps1: the lines above the legacy-list marker don't mention $old -- check its structure before re-running."
    }

    # Blanket-replace the whole file (header comment, "Removing..." line,
    # and those same two current-version target entries -- $old doesn't
    # collide with any of the shorter legacy version numbers already in
    # the list, e.g. "2.2"/"2.1"/"2", so this can't touch them).
    $text = ($lines -join "`n").Replace($old, $new)
    $lines = $text -split "`n"

    # Re-find the marker (index is unchanged -- no lines were added/removed
    # yet) and insert the ORIGINAL (pre-replace) old-version entries right
    # after it, ahead of the existing legacy entries.
    $newLines = New-Object System.Collections.Generic.List[string]
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $newLines.Add($lines[$i])
        if ($i -eq $commentIdx) {
            $newLines.Add($oldVst3Line)
            $newLines.Add($oldAppLine)
        }
    }
    Write-Utf8 $uninstallPath ($newLines -join "`n")
    Write-Host "  updated: installer\uninstall.ps1 (kept $old in the legacy cleanup list)" -ForegroundColor DarkGray
} else {
    Write-Host "  SKIP (not found): installer\uninstall.ps1" -ForegroundColor DarkYellow
}

# ── 4. Rename + content-bump files whose filename embeds the version ────
$renames = @(
    @{ Dir = 'installer'; OldName = "Install VisualComp $old.bat";   NewName = "Install VisualComp $new.bat" },
    @{ Dir = 'installer'; OldName = "Uninstall VisualComp $old.bat"; NewName = "Uninstall VisualComp $new.bat" },
    @{ Dir = '.';         OldName = "About VisualComp $old.md";      NewName = "About VisualComp $new.md" }
)
foreach ($r in $renames) {
    $oldPath = Join-Path (Join-Path $root $r.Dir) $r.OldName
    $newPath = Join-Path (Join-Path $root $r.Dir) $r.NewName
    if (-not (Test-Path -LiteralPath $oldPath)) { Write-Host "  SKIP (not found): $($r.Dir)\$($r.OldName)" -ForegroundColor DarkYellow; continue }
    $content = (Read-Utf8 $oldPath).Replace($old, $new)
    Write-Utf8 $newPath $content
    Remove-Item -LiteralPath $oldPath -Force
    Write-Host "  renamed: $($r.OldName) -> $($r.NewName)" -ForegroundColor DarkGray
}

# ── 5. Manual: bump every "current version" stamp, but freeze the dedicated
#      per-release changelog section (and the tip box that forward-references
#      it) as history -- see CLAUDE.md, this is deliberately NOT authored
#      automatically each release; only the version stamps move forward. ──
$manualPath = Join-Path $root 'docs\manual.html'
if (Test-Path -LiteralPath $manualPath) {
    $html = Read-Utf8 $manualPath
    # ASCII-only markers deliberately -- Windows PowerShell 5.1 parses a .ps1
    # file without a BOM using the system codepage, not UTF-8, so a non-ASCII
    # literal (e.g. manual.html's "======" box-drawing rule) embedded directly
    # in this script's own source would get silently mangled at parse time
    # and never match. These plain-ASCII substrings are already unique within
    # the file (verified against the section-heading comments) and sidestep
    # the whole problem.
    $sectionMarker = '19 VERSION '
    $specsMarker   = '20 SPECS'
    $idx19 = $html.IndexOf($sectionMarker)
    if ($idx19 -ge 0) {
        $tipIdx = $html.LastIndexOf('<div class="tip">', $idx19)
        $protectStart = if ($tipIdx -ge 0) { $tipIdx } else { $idx19 }
        $protectEnd = $html.IndexOf($specsMarker, $idx19)
        if ($protectEnd -lt 0) { $protectEnd = $protectStart }   # marker missing: protect nothing extra

        $before  = $html.Substring(0, $protectStart)
        $frozen  = $html.Substring($protectStart, $protectEnd - $protectStart)
        $after   = $html.Substring($protectEnd)
        $html = $before.Replace($old, $new) + $frozen + $after.Replace($old, $new)
        Write-Host "  updated: docs\manual.html (Section 19 changelog left as historical $old record)" -ForegroundColor DarkGray
    } else {
        $html = $html.Replace($old, $new)
        Write-Host "  updated: docs\manual.html (no changelog section found to protect)" -ForegroundColor DarkYellow
    }
    Write-Utf8 $manualPath $html

    # Regenerate the PDF from the just-edited HTML and drop the old one --
    # package.ps1 requires the exact new-version filename to exist.
    $oldPdf = Join-Path $root "docs\VisualComp $old - User Manual.pdf"
    $newPdf = Join-Path $root "docs\VisualComp $new - User Manual.pdf"
    $chrome = 'C:\Program Files\Google\Chrome\Application\chrome.exe'
    if (Test-Path -LiteralPath $chrome) {
        $manualUri = ([Uri](Resolve-Path -LiteralPath $manualPath).Path).AbsoluteUri
        # Chrome writes its own "N bytes written to file ..." diagnostic to
        # stderr on success (not just on failure) -- under this script's
        # $ErrorActionPreference = 'Stop', PowerShell 5.1 turns ANY stderr
        # line from a native exe into a terminating error, so that success
        # message alone would otherwise abort the script. Relax it just for
        # this call.
        $prevEAP = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        & $chrome --headless --disable-gpu --no-pdf-header-footer `
            "--print-to-pdf=$newPdf" $manualUri 2>$null | Out-Null
        $ErrorActionPreference = $prevEAP
        if (Test-Path -LiteralPath $newPdf) {
            Write-Host "  generated: docs\VisualComp $new - User Manual.pdf" -ForegroundColor DarkGray
            if (Test-Path -LiteralPath $oldPdf) {
                Remove-Item -LiteralPath $oldPdf -Force
                Write-Host "  removed:   docs\VisualComp $old - User Manual.pdf (superseded)" -ForegroundColor DarkGray
            }
        } else {
            Write-Host "  WARNING: manual PDF generation did not produce an output file -- package.ps1 will throw until docs\VisualComp $new - User Manual.pdf exists." -ForegroundColor Red
        }
    } else {
        Write-Host "  WARNING: Chrome not found at $chrome -- couldn't regenerate the manual PDF. Rename/regenerate docs\VisualComp $new - User Manual.pdf manually before packaging." -ForegroundColor Red
    }
} else {
    Write-Host "  SKIP (not found): docs\manual.html" -ForegroundColor DarkYellow
}

Write-Host ''
Write-Host "  Done. Version is now $new." -ForegroundColor Green
Write-Host "  Reminder: this bumps version STAMPS only -- it does not author a new" -ForegroundColor DarkGray
Write-Host "  manual changelog section for what's new in $new. Add one by hand in" -ForegroundColor DarkGray
Write-Host "  docs\manual.html (see the existing 'Section 19' entry as a template)" -ForegroundColor DarkGray
Write-Host "  when there's user-facing material worth documenting." -ForegroundColor DarkGray
Write-Host ''
