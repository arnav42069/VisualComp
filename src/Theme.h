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
}
