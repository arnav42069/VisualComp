# AGENTS.md

This file provides guidance to agents when working with code in this repository.

- Version is centralized in [`CMakeLists.txt`](CMakeLists.txt:2) / [`package.ps1`](package.ps1:6); do not hardcode another copy.
- The current product/plugin ID is [`Vc22`](CMakeLists.txt:47); changing version text deliberately renames the built plugin for host rescans.
- Release builds should use [`cmake --build build --config Release --target VisualComp_VST3`](CMakeLists.txt:1) for the installed VST3 and [`cmake --build build --config Release --target VisualComp_VST3 VisualComp_Standalone`](testbuild.ps1:16) for the quick test loop.
- Single fast validation path is [`testbuild.ps1`](testbuild.ps1:1): it bumps version, builds VST3+Standalone, copies the newest exe to [`Build Final\Standalone Test`](testbuild.ps1:30), and launches it.
- Packaging is [`package.ps1`](package.ps1:1); it picks the newest output from [`build`](package.ps1:70) or [`build-pkg`](package.ps1:70), stages the release bundle, and copies a loose test exe into [`Build Final`](package.ps1:134).
- No separate lint/test framework is configured; build scripts are the validation surface.
- Add new `.cpp`/`.h` pairs to [`target_sources`](CMakeLists.txt:114) immediately.
- New processor state must stay in custom [`ValueTree`](AGENTS.md:1) properties, not new APVTS parameters.
- Preserve the docked-panel layout pattern: `paint()` uses one `AffineTransform::translation(ox, 0)` while `resized()` applies `+ox` manually.
- Do not replace the `SpinLock` + audio-thread `tryLock` snapshot pattern in [`src/EqEngine.h`](src/EqEngine.h:43); the audio thread must never block.
- Linked EQ nodes, edge bonds, and manual Island placement are intentional UI behaviors; keep their non-persistent/local-state semantics intact.
- User presets stay under [`Documents\Azazel Audio\VisualComp 2\Presets`](AGENTS.md:1); do not rename that folder.
- Read [`CLAUDE.md`](CLAUDE.md:1) before editing; it contains the fuller project-specific ruleset and supersedes guesswork.
