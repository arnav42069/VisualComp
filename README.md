# VisualComp 2.23

**A compressor that sees your mix.** Multiband dynamics, a Visual Parametric EQ, Sidechain Compression from an External Input, and mastering-grade output shaping (Glue Compression) in one plugin — controlled by a live, interactive graph instead of a wall of knobs.

**[Download Free (VST3 & Standalone for Windows & macOS)](https://www.azazelaudio.com/visualcomp)**

---

## Highlights

* **One graph, two jobs:** Draw your EQ curve and get per-band compression automatically.
* **4 Circuit Characters:** VCA, FET, Opto, and Tube models in one broadband stage.
* **Upward & Downward Dynamics:** Flip between downward taming and upward detail lifting via a single Range knob.
* **Zero-Latency Output Clipper:** Switch between Soft, Brickwall, or Off live without introducing clicks or changing host latency.
* **Integrated Metering:** Peak, RMS, and continuous approximate LUFS readings alongside a live GR/Transfer Curve display.
* **Preset Library:** Curated factory presets for Mastering, Drums, Vocals, Bass, and more.

---

## Core Features

### 1. Visual, Multiband-Aware EQ

A full 8-node parametric equalizer supporting Bell, Low Shelf, High Shelf, High-Pass, Low-Pass, and Notch filters (built on industry-standard RBJ biquad coefficients).

* Double-click the graph to create a node.
* Drag to set frequency and gain.
* Use the mouse wheel or the on-canvas Dynamic Island popup to adjust Q and filter types.

### 2. Per-Band Dynamics (Multiband as an EQ)

Any EQ node can be linked directly to the compressor engine. Linking a node gives it a dedicated bandpass-filtered detector, transforming it into a full dynamics band with its own Threshold, Knee, Ratio, Range, Attack, and Release.

* **Upward & Downward Mode:** The Range control ($\pm30\text{ dB}$) sets dynamic gain swing. Negative values apply classic downward compression; positive values flip the band to upward compression, bringing up low-level details (tails, room tone) without affecting loud peaks.
* **Precision Snapping:** Band edge markers snap to neighbours within 100 Hz for precise spectral control.

### 3. Core Broadband Compressor & Circuit Characters

A broadband compression engine with parallel-style wet/dry blending, Auto Gain, and four analog-inspired character models:

* **VCA:** Clean, fast, and transparent bus glue.
* **FET:** Ultra-fast attack with musical bite for drums, bass, and parallel compression.
* **Opto:** Program-dependent, smooth compression tailored for vocals and acoustic instruments.
* **Tube:** Variable-mu warmth and gentle harmonics for lead vocals and mastering.

### 4. Metering & Output Control

* **Metering Column:** Includes Peak, RMS, continuous EMA-smoothed K-weighted LUFS estimation, an analog-style GR needle, and a live input/output transfer curve with an active operating-point indicator.
* **Master Output Clipper:** Three-mode output clipper (Soft, Brickwall, or Off) sharing a fixed lookahead line so real-time mode switching never drops audio or changes latency.

---

## Interface & Workflow Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│ 01. Waveform Displays (Pre/Post Oscilloscopes with Threshold Overlay)   │
├─────────────────────────────────────────────────────────────────────────┤
│ 02. Dynamics Row (In/Out Gain, Core Controls, Sidechain, Auto Gain)    │
├───────────────────────────────────────────────────────┬─────────────────┤
│                                                       │ 04. Metering    │
│ 03. Parametric EQ & Multiband Graph                   │     Column      │
│     (Interactive 8-Node Canvas & Dynamic Island)      │     (LUFS, GR,  │
│                                                       │      Curve)     │
└───────────────────────────────────────────────────────┴─────────────────┘

```

1. **Waveform Displays:** Oscilloscopes show audio before and after processing with a live threshold overlay, making attack/release behaviors and gain reduction immediately visible.
2. **Dynamics Row:** Centralized controls for primary compression parameters. Drag anywhere inside a parameter box to adjust. Hold `Ctrl` (`Cmd` on macOS) for fine adjustment, or double-click to reset.
3. **Parametric EQ Panel:** Interactive main workspace to add, sculpt, and link frequency nodes to detector bands.
4. **Metering Column:** Instant feedback on gain reduction, loudness, and transfer curves.

---

## Technical Specifications

| Feature | Specification |
| --- | --- |
| **Formats** | VST3, Standalone |
| **Platforms** | Windows (64-bit), macOS (Intel & Apple Silicon) |
| **EQ Engine** | 8 Nodes (Bell, Low/High Shelf, HPF, LPF, Notch via RBJ biquads) |
| **Dynamics Engine** | Broadband + Per-Band, Upward/Downward, Range $\pm30\text{ dB}$ |
| **Circuit Models** | VCA, FET, Opto, Tube |
| **Clipper Modes** | Soft, Brickwall, Off (Fixed Latency) |
| **Metering** | Peak, RMS, Approximate LUFS (K-Weighted), Dynamic Curve / GR |

---

## Factory Presets

VisualComp includes a curated factory library designed for real mixing scenarios:

* **Mastering:** Mix-bus glue, gentle warmth, and peak smoothing.
* **Drums:** Transient punch for kicks, snare bite, room squash, and overhead control.
* **Vocals:** Lead smoothing, backing vocal management, and breath control.
* **Bass & Instruments:** Tonal correction and analog-style saturation settings.

---

## License & Support

VisualComp 2.23 is available as a free download. For bug reports, feature requests, or contributions, please open an issue or pull request in this repository.
