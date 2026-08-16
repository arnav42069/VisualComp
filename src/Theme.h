#pragma once
#include <JuceHeader.h>

// Azazel Audio house palette — single warm accent on near-black.
// Change `accent` to re-skin the plugin.
namespace Theme
{
    // Chassis
    const juce::Colour bg        { 0xff1d1d1b };   // Azazel logo black
    const juce::Colour bgDeep    { 0xff121211 };   // recessed screens
    const juce::Colour bgRaised  { 0xff262624 };   // raised bars / header
    const juce::Colour charcoal  { 0xff2b2b28 };   // secondary buttons

    // Accent
    const juce::Colour accent    { 0xffff7a1f };   // Azazel orange
    const juce::Colour accentMid { 0xffc25a15 };
    const juce::Colour accentDim { 0xff7a3910 };
    const juce::Colour accentDeep{ 0xff33190a };

    // Signal colours
    const juce::Colour inputCol  { 0xffff7a1f };
    const juce::Colour outputCol { 0xffffb257 };
    const juce::Colour warn      { 0xffe63414 };
    const juce::Colour ice       { 0xffbcd9f5 };

    // Text / lines
    const juce::Colour text      { 0xffe8dfd2 };
    const juce::Colour textDim   { 0xff9a9184 };
    const juce::Colour line      { 0xff3a3733 };

    // ---------------------------------------------------------------------
    // Design tokens (added 2026-08-16, "de-slop" pass).
    //
    // The rule these encode: the plugin is a NEUTRAL object with a warm
    // accent, not an orange object. Accent marks live signal and active
    // state ONLY — never a static label, never a border, never a control
    // that is merely present. If a thing is not moving with audio and not
    // switched on, it is drawn from the neutral ramp.
    // ---------------------------------------------------------------------

    // Surface depth ramp. Exactly four depths exist; do not invent a fifth
    // or nudge these by a few points — the flatness of the old UI came from
    // every panel sitting at the same depth with the same 1px border, and
    // that is only fixed by depths being few and clearly separated.
    const juce::Colour surfSunk   { 0xff0e0e0d };   // recessed screens: graphs, meters, waveforms
    const juce::Colour surfChassis{ 0xff1d1d1b };   // the body of the plugin (== bg)
    const juce::Colour surfRaised { 0xff262624 };   // controls sitting proud of the chassis
    const juce::Colour surfHover  { 0xff32312e };   // hover / pressed state of a raised control

    // Edges. Depth reads from a light top edge + dark bottom edge, NOT from
    // an all-round outline. Reserve `hairline` for genuine dividers between
    // sections; a panel that is already a different depth needs no border.
    const juce::Colour edgeTop    { 0x18ffffff };   // 1px highlight along a raised element's top
    const juce::Colour edgeBottom { 0x60000000 };   // 1px shadow along its bottom
    const juce::Colour hairline   { 0xff2a2926 };   // section divider — deliberately dimmer than `line`

    // Text ramp. Four steps, each with a job:
    //   textHi   values the user is reading right now
    //   text     ordinary legible copy
    //   textMid  parameter names / section titles
    //   textFaint axis ticks, units, hints — present but recessive
    const juce::Colour textHi     { 0xfff5efe6 };
    const juce::Colour textMid    { 0xff8a8278 };
    const juce::Colour textFaint  { 0xff5c574f };

    // Meter / signal ramp — a real meter changes colour with level. A single
    // flat orange bar is the giveaway of a decorative meter.
    const juce::Colour meterLow   { 0xff6f8f4a };   // safe
    const juce::Colour meterMid   { 0xffd9a441 };   // approaching
    const juce::Colour meterHot   { 0xffe63414 };   // over (== warn)

    // Futura is the house face; fall back gracefully if it is not installed.
    inline const juce::String& uiFontName()
    {
        static const juce::String resolved = []() -> juce::String
        {
            const auto available = juce::Font::findAllTypefaceNames();
            for (const char* candidate : { "Futura", "Futura Md BT", "Futura PT",
                                           "Century Gothic", "Avenir Next", "Jost" })
                if (available.contains(candidate))
                    return juce::String(candidate);
            return juce::Font::getDefaultSansSerifFontName();
        }();
        return resolved;
    }

    inline juce::Font label(float h, int style = juce::Font::bold)
    {
        return juce::Font(uiFontName(), h, style);
    }

    inline juce::Font mono(float h, int style = juce::Font::plain)
    {
        return juce::Font(juce::Font::getDefaultMonospacedFontName(), h, style);
    }

    // ---------------------------------------------------------------------
    // Type scale. THREE sizes, and each has one job. The old UI drew every
    // label at the same caps-bold weight, which is why nothing looked more
    // important than anything else. (Declared after `uiFontName()` because
    // they call it.)
    // ---------------------------------------------------------------------

    // Parameter names, section titles: small, letterspaced caps, recessive.
    // Caller applies the tracking (JUCE has no letter-spacing on Font).
    inline juce::Font micro(float h = 9.5f)
    {
        return juce::Font(uiFontName(), h, juce::Font::bold);
    }

    // The number the user is actually reading. Bigger and brighter than its
    // own label — the value leads, the name follows.
    inline juce::Font value(float h = 13.5f)
    {
        return juce::Font(uiFontName(), h, juce::Font::plain);
    }

    // Draws `s` as letterspaced caps — the single most effective way to make
    // a small label look designed rather than defaulted. `tracking` is extra
    // pixels between glyphs. Uses whatever font/colour is already set on `g`.
    inline void drawTracked(juce::Graphics& g, const juce::String& s,
                            juce::Rectangle<int> area,
                            juce::Justification just = juce::Justification::centred,
                            float tracking = 1.2f)
    {
        const auto up = s.toUpperCase();
        const auto f  = g.getCurrentFont();

        float total = 0.0f;
        for (int i = 0; i < up.length(); ++i)
            total += f.getStringWidthFloat(up.substring(i, i + 1)) + tracking;
        total -= tracking;

        float x = float(area.getX());
        if (just.testFlags(juce::Justification::horizontallyCentred))
            x = area.getCentreX() - total * 0.5f;
        else if (just.testFlags(juce::Justification::right))
            x = float(area.getRight()) - total;

        const float baseline = area.getCentreY() + f.getAscent() * 0.5f - f.getDescent() * 0.5f;

        for (int i = 0; i < up.length(); ++i)
        {
            const auto ch = up.substring(i, i + 1);
            g.drawSingleLineText(ch, juce::roundToInt(x), juce::roundToInt(baseline));
            x += f.getStringWidthFloat(ch) + tracking;
        }
    }

    // A recessed screen: flat sunk fill + a soft inner shadow along the top
    // edge. No outline — the depth change alone separates it from the
    // chassis. Use for anything that displays signal (graphs, meters, scopes).
    inline void drawRecess(juce::Graphics& g, juce::Rectangle<float> r, float radius = 3.0f)
    {
        g.setColour(surfSunk);
        g.fillRoundedRectangle(r, radius);

        for (int i = 0; i < 4; ++i)
        {
            g.setColour(juce::Colour(0xff000000).withAlpha(0.30f - float(i) * 0.07f));
            g.drawLine(r.getX() + radius, r.getY() + 0.5f + float(i),
                       r.getRight() - radius, r.getY() + 0.5f + float(i), 1.0f);
        }

        g.setColour(edgeTop);
        g.drawLine(r.getX() + radius, r.getBottom() - 0.5f,
                   r.getRight() - radius, r.getBottom() - 0.5f, 1.0f);
    }

    // A control sitting proud of the chassis: fill + top highlight + bottom
    // shadow. Again no full outline.
    inline void drawRaised(juce::Graphics& g, juce::Rectangle<float> r,
                           float radius = 3.0f, bool on = false)
    {
        g.setColour(on ? accentDim : surfRaised);
        g.fillRoundedRectangle(r, radius);

        g.setColour(on ? accent.withAlpha(0.55f) : edgeTop);
        g.drawLine(r.getX() + radius, r.getY() + 0.5f,
                   r.getRight() - radius, r.getY() + 0.5f, 1.0f);

        g.setColour(edgeBottom);
        g.drawLine(r.getX() + radius, r.getBottom() - 0.5f,
                   r.getRight() - radius, r.getBottom() - 0.5f, 1.0f);
    }

    // Level-dependent meter colour. Use for anything showing signal amplitude
    // so meters stop being flat orange sticks.
    inline juce::Colour meterColour(float db)
    {
        if (db >= -1.0f)  return meterHot;
        if (db >= -6.0f)  return meterMid;
        if (db >= -18.0f) return accent;
        return meterLow;
    }
}
