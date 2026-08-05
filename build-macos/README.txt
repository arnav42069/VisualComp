VisualComp 2.1 — macOS Build Guide
Azazel Audio
===============================

IMPORTANT: macOS plugins can only be compiled ON a Mac (Apple's toolchain
does not run on Windows). This folder contains everything needed — copy
the whole project to a Mac and run one script.

WHAT YOU GET
------------
A universal VST3 (Apple Silicon + Intel, macOS 10.13+) that installs
itself into the standard plugin folder automatically:
  ~/Library/Audio/Plug-Ins/VST3/VisualComp 2.1.vst3

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
3. Wait for "SUCCESS — plugin installed at: ..." (the first build takes a
   few minutes because it downloads and compiles the JUCE framework).
4. Open your DAW and rescan plugins. The plugin appears as
   "VisualComp 2.1" by Azazel Audio.

NOTES FOR MAC USERS
-------------------
- Gatekeeper: this plugin is not code-signed. If the DAW refuses to
  load it, run:
     xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"VisualComp 2.1.vst3"
- Logic Pro only loads AU plugins, not VST3. To also build an AU
  version, edit CMakeLists.txt: change  FORMATS VST3 Standalone
  to  FORMATS VST3 AU Standalone  and rebuild.
- Rebuilding after code changes: run ./build.sh again — it recompiles
  only what changed and reinstalls automatically.
- User presets are saved to:
     ~/Documents/Azazel Audio/VisualComp 2/Presets/

FILE LOCATIONS SUMMARY
----------------------
Windows plugin:
  C:\Program Files\Common Files\VST3\VisualComp 2.1.vst3
macOS plugin (after running build.sh):
  ~/Library/Audio/Plug-Ins/VST3/VisualComp 2.1.vst3

The full user manual (PDF) is in the Documentation folder of the
release bundle.
