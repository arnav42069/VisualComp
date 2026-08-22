================================================================
 VISUALCOMP 2.37                                      AZAZEL AUDIO
 See your compression. Shape your sound.
================================================================

 A visual compressor for Windows and macOS.
 VST3 + Standalone. Four detector circuits, 34 presets,
 real-time waveform displays and visual sidechain ducking.


 WHAT'S IN THIS PACKAGE
----------------------------------------------------------------
   Windows\      Installer, VST3 plugin and standalone app
   Mac\          Build steps and full source for macOS
   VisualComp 2.37 - User Manual.pdf   Complete illustrated manual
   README.txt    This file


 WINDOWS - INSTALL
----------------------------------------------------------------
   1. Open the Windows folder.
   2. Double-click  "Install VisualComp 2.37.bat"
      Windows will ask for administrator permission - this is
      needed to write into the shared VST3 folder.
   3. It installs:
        - the VST3 plugin  -> C:\Program Files\Common Files\VST3\
        - the standalone   -> C:\Program Files\Azazel Audio\
        - a Start Menu shortcut
   4. Open your DAW and rescan plugins:
        FL Studio:  Options > Manage plugins > Find more plugins
        Others:     rescan in plugin preferences
   5. The plugin appears as "VisualComp 2.37" by Azazel Audio.

   To remove it later, run "Uninstall VisualComp 2.37.bat".
   Your presets are never touched by the uninstaller.

   Prefer to install by hand? Copy the "VisualComp 2.37.vst3"
   folder into C:\Program Files\Common Files\VST3\ yourself,
   and run "VisualComp 2.37.exe" from anywhere.


 macOS - BUILD AND INSTALL
----------------------------------------------------------------
   macOS plugins must be compiled on a Mac. Everything needed
   is in the Mac folder.

   1. Copy the Mac folder to your Mac.
   2. Install the tools (one time only):
        xcode-select --install
        brew install cmake
   3. In Terminal:
        cd Mac/Source/build-macos
        chmod +x build.sh
        ./build.sh
   4. It builds a universal binary (Apple Silicon + Intel) and
      installs it to ~/Library/Audio/Plug-Ins/VST3/
   5. Rescan plugins in your DAW.

   Not code-signed. If your DAW refuses to load it:
     xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"VisualComp 2.37.vst3"

   See Mac\README - Mac Build Steps.txt for the long version.


 FIRST FIVE MINUTES
----------------------------------------------------------------
   - A guided tour opens the first time you load the plugin.
     Skip it any time; bring it back by clicking the Azazel
     logo (top-left) and choosing "Show Help".
   - Drag anywhere inside a control's box to change it.
     Hold Ctrl (Cmd on Mac) for fine adjustment.
     Double-click to reset a control.
   - Leave the default "Mastering Glue" preset loaded, lower
     THRESHOLD until the needle sits around -2 to -4 dB on
     peaks, and you have a working mix-bus compressor.

   Everything else is in the manual.


 SIDECHAIN IN FL STUDIO (the usual sticking point)
----------------------------------------------------------------
   1. Route the key track (e.g. the kick) to the track that
      hosts VisualComp 2.37, using the send arrow at the bottom
      of the mixer channel.
   2. In the plugin wrapper menu, go to
        Processing > Connections
      and select your External input from the dropdown.
   3. Press SC in the plugin.
   Other DAWs are covered in section 10 of the manual.


 FILE LOCATIONS
----------------------------------------------------------------
   User presets
     Windows:  Documents\Azazel Audio\VisualComp 2\Presets\
     macOS:    ~/Documents/Azazel Audio/VisualComp 2/Presets/


 SYSTEM REQUIREMENTS
----------------------------------------------------------------
   Windows 10 / 11 (64-bit), VST3 host
   macOS 10.13 or later, Apple Silicon or Intel


================================================================
 Azazel Audio  -  Designed and built by Arnav Singh
 azazelaudio.com
================================================================
