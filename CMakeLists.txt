cmake_minimum_required(VERSION 3.15)
project(VisuComp VERSION 2.39.0)

# Single source of truth for the version string embedded in PRODUCT_NAME below
# and in the distribution-folder paths further down -- keep these in sync
# rather than hardcoding "2.39" in more than one place (a prior version bump
# updated PRODUCT_NAME but missed the DIST_DIR block, which then silently
# kept referencing stale "2.1" paths for an entire release cycle).
set(VC2_VERSION_STRING "2.39")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Emit PDBs for Release builds too. CMake's stock Release flags for MSVC omit
# /Zi entirely, so a Release .exe ships with zero debug symbols -- fine for
# distribution, but it means a crash dump/WER report can only ever point at a
# raw module+offset (e.g. "ntdll.dll+0x6a8e6"), never a real function name or
# call stack in our own code. /Zi + /DEBUG add symbol info without touching
# optimization (/O2 etc. are untouched); /OPT:REF /OPT:ICF keep the linker's
# usual Release-mode dead-code stripping/folding, which /DEBUG alone does not
# disable but is worth pinning explicitly since we're touching linker flags.
if(MSVC)
    add_compile_options($<$<CONFIG:Release>:/Zi>)
    add_link_options($<$<CONFIG:Release>:/DEBUG> $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9
)
FetchContent_MakeAvailable(JUCE)

# Copy the built plugin into the system VST3 folder after each build.
# Turn OFF (-DVC2_INSTALL_PLUGIN=OFF) to build a release copy while a DAW has
# the installed plugin open and locked.
option(VC2_INSTALL_PLUGIN "Install the plugin to the system VST3 folder after building" ON)

# Where the built VST3 gets auto-copied so the DAW picks up new builds
if(WIN32)
    set(VST3_INSTALL_DIR "C:/Program Files/Common Files/VST3")
else()
    set(VST3_INSTALL_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
endif()

juce_add_plugin(VisualComp
    PLUGIN_MANUFACTURER_CODE Azzl
    PLUGIN_CODE Vc22
    # AU only actually builds on macOS -- JUCE's _juce_get_platform_plugin_kinds
    # filters FORMATS by CMAKE_SYSTEM_NAME, so listing it here is a silent no-op
    # on this Windows machine and only takes effect when build-macos/build.sh is
    # run on an actual Mac (see CLAUDE.md; Apple's toolchain doesn't run on
    # Windows, so there's no way to produce an AU here either way).
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "VisualComp ${VC2_VERSION_STRING}"
    COMPANY_NAME "Azazel Audio"
    COMPANY_WEBSITE "https://brella.co.in"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${VC2_INSTALL_PLUGIN}
    VST3_COPY_DIR "${VST3_INSTALL_DIR}"
    VST3_CATEGORIES "Fx" "Dynamics"
    ICON_BIG "${CMAKE_SOURCE_DIR}/resources/azazel-icon.png"
)

juce_generate_juce_header(VisualComp)

# ── Knob filmstrip asset ──────────────────────────────────────────────────
# One 192px / 61-frame master (~2 MB). Every on-screen knob size is derived
# from it at runtime and cached rather than shipping a second, smaller strip —
# see src/KnobStrip.cpp. Regenerate with:
#   python scripts/make_knob_filmstrip.py --size 192 --frames 61 #          --brush 0.052 --out resources/knob-azazel-192x61.png
# The higher --brush is not a different look: this master is drawn at ~78px, a
# 2.5x reduction, and the box filter in KnobStrip.cpp averages the fine grain
# octaves away. 0.052 here lands at the same perceived grain as the 0.030 the
# 128px concept strips are rendered at and viewed 1:1.
# (that script verifies its own output; don't hand-edit the PNG). The
# 128x31 / 256x31 strips alongside it in resources/ are the delivered design
# asset in the standard interchange format and are NOT compiled in.
juce_add_binary_data(VisualCompData
    SOURCES
        resources/knob-azazel-192x61.png
)

# ── Distribution folder: every Windows build refreshes the release bundle ──
if(WIN32 AND VC2_INSTALL_PLUGIN)
    set(DIST_DIR "${CMAKE_SOURCE_DIR}/VisualComp ${VC2_VERSION_STRING} - Win and Mac")
    add_custom_command(TARGET VisualComp_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_FILE_DIR:VisualComp_VST3>/../.."
            "${DIST_DIR}/Windows/VisualComp ${VC2_VERSION_STRING}.vst3"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/build-macos/build.sh"
            "${DIST_DIR}/Mac/build.sh"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/build-macos/README.txt"
            "${DIST_DIR}/Mac/README.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/docs/VisualComp ${VC2_VERSION_STRING} - User Manual.pdf"
            "${DIST_DIR}/Documentation/VisualComp ${VC2_VERSION_STRING} - User Manual.pdf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/About VisualComp ${VC2_VERSION_STRING}.md"
            "${DIST_DIR}/Documentation/About VisualComp ${VC2_VERSION_STRING}.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/installer/README.txt"
            "${DIST_DIR}/README.txt"
        COMMENT "Refreshing distribution folder: ${DIST_DIR}")
endif()

target_sources(VisualComp PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/WaveformDisplay.cpp
    src/VuMeter.cpp
    src/GrCurveDisplay.cpp
    src/EqPanel.cpp
    src/NodeIsland.cpp
    src/KnobStrip.cpp
    src/LevelMeter.cpp
    src/LufsMeter.cpp
    src/LicenseVerifier.cpp
    src/LicenseManager.cpp
    src/MachineFingerprint.cpp
    src/DemoModeIndicator.cpp
)

target_include_directories(VisualComp PRIVATE src)

target_compile_definitions(VisualComp
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(VisualComp
    PRIVATE
        VisualCompData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_plugin_client
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_dsp
        juce::juce_cryptography
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
