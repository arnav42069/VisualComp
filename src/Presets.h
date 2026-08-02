#pragma once
// Factory presets for VisualComp 2.
// Values informed by classic hardware practice: SSL G-Bus (glue: 2:1/4:1,
// 10-30ms attack, auto release), 1176-style FET (fast attack, punchy),
// LA-2A-style optical (slow, musical, vocals/bass), and variable-mu tube
// (mastering warmth). Ranges must stay inside the APVTS parameter ranges:
// threshold -60..0, knee 0..20, ratio 1..20, attack 0.1..200ms,
// release 1..2000ms, gain -24..24, mix 0..1, mode 0=VCA 1=FET 2=Opt 3=Tube.
//
// clipMode matches ClipMode (ClipEngine.h): 0=Soft, 1=Brickwall, 2=Off.
// Mastering-facing presets use Brickwall for a transparent, hard 0 dB
// ceiling; presets built around deliberate colour (tape/tube/parallel
// crush) use Soft; presets meant to stay maximally clean and rely on the
// compressor alone use Off.

struct FactoryPreset
{
    const char* category;
    const char* name;
    float threshold, knee, ratio, attackMs, releaseMs;
    float gainIn, gainOut, mix;
    bool  limiter;
    int   mode;
    int   clipMode;
};

static constexpr FactoryPreset kFactoryPresets[] =
{
    // ── Mastering / Mix Bus ──────────────────────────────────────────────────
    { "Mastering", "Mastering Glue",          -10.0f,  6.0f,  2.0f,  30.0f,  100.0f, 0.0f, 0.0f, 1.00f, false, 0, 1 },
    { "Mastering", "Mastering Warm Tube",      -8.0f,  8.0f,  1.5f,  50.0f,  300.0f, 0.0f, 0.0f, 1.00f, false, 3, 1 },
    { "Mastering", "Mastering Loud & Proud",  -14.0f,  4.0f,  3.0f,  10.0f,  100.0f, 0.0f, 2.0f, 1.00f, true,  0, 1 },
    { "Mastering", "Mix Bus Punch",           -12.0f,  3.0f,  4.0f,  30.0f,  100.0f, 0.0f, 0.0f, 1.00f, false, 0, 1 },
    { "Mastering", "Mix Bus Smooth",          -10.0f, 10.0f,  2.0f,  30.0f,  300.0f, 0.0f, 0.0f, 1.00f, false, 0, 1 },
    { "Mastering", "Gentle Tape Feel",         -6.0f, 12.0f,  1.5f,  80.0f,  400.0f, 0.0f, 0.0f, 1.00f, false, 3, 0 },

    // ── Drums ────────────────────────────────────────────────────────────────
    { "Drums", "Drum Bus Glue",               -12.0f,  4.0f,  4.0f,  30.0f,  150.0f, 0.0f, 0.0f, 1.00f, false, 0, 0 },
    { "Drums", "Drum Bus Smash (Parallel)",   -30.0f,  2.0f, 10.0f,   1.0f,  100.0f, 0.0f, 3.0f, 0.35f, false, 1, 0 },
    { "Drums", "Kick Punch",                  -15.0f,  2.0f,  4.0f,  30.0f,  100.0f, 0.0f, 0.0f, 1.00f, false, 1, 2 },
    { "Drums", "Snare Crack",                 -15.0f,  2.0f,  4.0f,  10.0f,  150.0f, 0.0f, 0.0f, 1.00f, false, 1, 2 },
    { "Drums", "Room Crush",                  -35.0f,  0.0f, 12.0f,   0.5f,   80.0f, 0.0f, 2.0f, 0.50f, false, 1, 0 },
    { "Drums", "Overheads Tame",              -18.0f,  8.0f,  3.0f,  30.0f,  300.0f, 0.0f, 0.0f, 1.00f, false, 2, 2 },
    { "Drums", "Percussion Control",          -18.0f,  4.0f,  4.0f,  15.0f,  120.0f, 0.0f, 0.0f, 1.00f, false, 0, 2 },

    // ── Vocals ───────────────────────────────────────────────────────────────
    { "Vocals", "Lead Vocal Smooth",          -18.0f,  8.0f,  3.0f,  30.0f,  500.0f, 0.0f, 2.0f, 1.00f, false, 2, 2 },
    { "Vocals", "Lead Vocal Upfront",         -20.0f,  4.0f,  4.0f,   5.0f,  200.0f, 0.0f, 3.0f, 1.00f, false, 1, 0 },
    { "Vocals", "Vocal Thickener",            -16.0f,  6.0f,  3.0f,  50.0f,  300.0f, 0.0f, 2.0f, 1.00f, false, 3, 0 },
    { "Vocals", "Backing Vocals Tight",       -22.0f,  6.0f,  5.0f,  10.0f,  200.0f, 0.0f, 2.0f, 1.00f, false, 0, 2 },
    { "Vocals", "Rap Vocal In-Your-Face",     -24.0f,  2.0f,  6.0f,   2.0f,  150.0f, 0.0f, 4.0f, 1.00f, false, 1, 0 },
    { "Vocals", "Vocal Leveller (Serial)",    -14.0f, 10.0f,  2.5f,  40.0f,  600.0f, 0.0f, 1.0f, 1.00f, false, 2, 2 },
    { "Vocals", "Whisper Boost",              -30.0f,  8.0f,  4.0f,  20.0f,  400.0f, 0.0f, 6.0f, 1.00f, false, 2, 1 },

    // ── Bass ─────────────────────────────────────────────────────────────────
    { "Bass", "Bass Guitar Solid",            -18.0f,  4.0f,  5.0f,   7.0f,  600.0f, 0.0f, 2.0f, 1.00f, false, 1, 2 },
    { "Bass", "Bass Synth Flat",              -16.0f,  2.0f,  8.0f,   5.0f,  200.0f, 0.0f, 2.0f, 1.00f, false, 0, 2 },
    { "Bass", "Upright Warmth",               -14.0f,  8.0f,  3.0f,  40.0f,  300.0f, 0.0f, 1.0f, 1.00f, false, 3, 0 },
    { "Bass", "808 Control",                  -12.0f,  4.0f,  4.0f,  30.0f,  250.0f, 0.0f, 0.0f, 1.00f, false, 0, 1 },

    // ── Guitar / Keys ────────────────────────────────────────────────────────
    { "Guitar & Keys", "Acoustic Guitar Even",  -16.0f,  8.0f,  3.0f,  15.0f,  250.0f, 0.0f, 1.0f, 1.00f, false, 2, 2 },
    { "Guitar & Keys", "Electric Rhythm Tight", -18.0f,  4.0f,  4.0f,   5.0f,  200.0f, 0.0f, 1.0f, 1.00f, false, 0, 2 },
    { "Guitar & Keys", "Funk Guitar Snap",      -20.0f,  2.0f,  6.0f,   8.0f,  120.0f, 0.0f, 2.0f, 1.00f, false, 1, 0 },
    { "Guitar & Keys", "Piano Glue",            -14.0f,  8.0f,  2.5f,  30.0f,  300.0f, 0.0f, 0.0f, 1.00f, false, 0, 2 },
    { "Guitar & Keys", "Synth Pad Smooth",      -16.0f, 10.0f,  2.5f,  50.0f,  400.0f, 0.0f, 0.0f, 1.00f, false, 2, 2 },

    // ── Utility / FX ─────────────────────────────────────────────────────────
    { "Utility & FX", "EDM Pump (use SC)",     -25.0f,  2.0f,  8.0f,   1.0f,  200.0f, 0.0f, 0.0f, 1.00f, false, 0, 1 },
    { "Utility & FX", "Podcast Levelling",     -20.0f, 10.0f,  3.0f,  20.0f,  400.0f, 0.0f, 3.0f, 1.00f, true,  2, 1 },
    { "Utility & FX", "Parallel Thickener",    -28.0f,  4.0f,  8.0f,   3.0f,  150.0f, 0.0f, 2.0f, 0.40f, false, 1, 0 },
    { "Utility & FX", "Brickwall Safety",       -6.0f,  2.0f, 10.0f,   0.5f,   80.0f, 0.0f, 0.0f, 1.00f, true,  0, 1 },
    { "Utility & FX", "Do Nothing (Reset)",      0.0f,  0.0f,  1.0f,  30.0f,  100.0f, 0.0f, 0.0f, 1.00f, false, 0, 2 },
};

static constexpr int kNumFactoryPresets = int(sizeof(kFactoryPresets) / sizeof(kFactoryPresets[0]));
