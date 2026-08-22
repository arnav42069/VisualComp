VisualComp 2.28 — macOS Build Guide
Azazel Audio
===============================

IMPORTANT: macOS plugins can only be compiled ON a Mac (Apple's toolchain
does not run on Windows). This folder contains everything needed — copy
the whole project to a Mac and run one script.

WHAT YOU GET
------------
A universal VST3 AND AU (Apple Silicon + Intel, macOS 10.13+), both built
in one pass and installed into their standard plugin folders automatically:
  ~/Library/Audio/Plug-Ins/VST3/VisualComp 2.28.vst3
  ~/Library/Audio/Plug-Ins/Components/VisualComp 2.28.component

PREREQUISITES (one-time setup on the Mac)
-----------------------------------------
1. Xcode Command Line Tools — open Terminal and run:
     xcode-select --install
2. Homebrew (if not installed) — see https://brew.sh
3. CMake:
     brew install cmake

BUILD STEPS
-----------
1. Copy the ENTIRE project folder to the Mac (USB, AirDrop, cloud drive).
2. Open Terminal and run:
     cd path/to/project/build-macos
     chmod +x build.sh
     ./build.sh
3. Wait for both "SUCCESS — ... installed at: ..." lines (the first build
   takes a few minutes because it downloads and compiles the JUCE framework).
4. Open your DAW and rescan plugins. The plugin appears as
   "VisualComp 2.28" by Azazel Audio.

NOTES FOR MAC USERS
-------------------
- Gatekeeper: these plugins are not code-signed. If the DAW refuses to
  load one, run:
     xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"VisualComp 2.28.vst3"
     xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"VisualComp 2.28.component"
- Logic Pro only loads AU plugins, not VST3 — that's why build.sh builds
  both formats in one pass; no manual CMakeLists.txt edit needed.
- Rebuilding after code changes: run ./build.sh again — it recompiles
  only what changed and reinstalls automatically.
- User presets are saved to:
     ~/Documents/Azazel Audio/VisualComp 2/Presets/

FILE LOCATIONS SUMMARY
----------------------
Windows plugin (VST3 only — no AU on Windows, it's an Apple-only format):
  C:\Program Files\Common Files\VST3\VisualComp 2.28.vst3
macOS plugins (after running build.sh):
  ~/Library/Audio/Plug-Ins/VST3/VisualComp 2.28.vst3
  ~/Library/Audio/Plug-Ins/Components/VisualComp 2.28.component

The full user manual (PDF) is in the Documentation folder of the
release bundle.
