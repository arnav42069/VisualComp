#include "KnobStrip.h"
#include <BinaryData.h>
#include <map>

namespace KnobStrip
{

// Frame size of the embedded master, in px. Sized so the largest dial in the
// UI (the ~91 px Dynamics knobs) still has real pixels behind it at 2x display
// scaling, which is the common case on a scaled Windows laptop.
static constexpr int kMasterCell = 192;

//==============================================================================
/*  The embedded master, decoded once.

    ~9 MB resident. This is a function-local static, so it is shared by every
    plugin instance in the process rather than paid for per editor -- which is
    also why the per-size caches below derive from it instead of each editor
    keeping its own scaled copy.
*/
static const juce::Image& master()
{
    static const juce::Image img = []
    {
        // Note the symbol name: JUCE's binary-data mangler DELETES hyphens
        // rather than turning them into underscores, so "knob-azazel-192x61.png"
        // becomes knobazazel192x61_png. Check the generated
        // build/juce_binarydata_VisualCompData/JuceLibraryCode/BinaryData.h
        // if the asset is ever renamed.
        auto i = juce::ImageFileFormat::loadFrom (BinaryData::knobazazel192x61_png,
                                                  (size_t) BinaryData::knobazazel192x61_pngSize);

        // Force ARGB: boxDownsample() reads PixelARGB straight out of the
        // bitmap, and JUCE stores those premultiplied -- which is the only
        // space a resample may happen in without transparent edges bleeding
        // black into the silhouette. (The Python renderer downsamples
        // premultiplied for exactly the same reason; doing it right there and
        // wrong here would put the fringe back.)
        return i.isValid() ? i.convertedToFormat (juce::Image::ARGB) : i;
    }();

    return img;
}

//==============================================================================
/*  Area-averages the strip down to `cell` px per frame.

    Graphics::highResamplingQuality is bilinear -- it samples a 2x2
    neighbourhood, which is fine for a mild reduction but not for what happens
    here: the smallest dials (the 45 px Mix knob, the 49 px Dynamic Island
    knobs) are a 4x reduction of the master, where bilinear discards roughly
    fifteen sixteenths of the source. The spun grain is the first thing to go,
    and it goes to aliased sparkle rather than to smooth metal. A box filter
    over the full source footprint keeps grain reading as grain.

    Each frame is resampled into its own slot so no filter footprint can
    straddle a frame boundary -- the renderer guarantees a clear margin between
    frames, and this keeps it that way.
*/
static juce::Image boxDownsample (const juce::Image& src, int cell)
{
    juce::Image dst (juce::Image::ARGB, cell, cell * kFrames, true);

    const juce::Image::BitmapData s (src, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData       d (dst, juce::Image::BitmapData::writeOnly);

    const double ratio = double (kMasterCell) / double (cell);

    for (int f = 0; f < kFrames; ++f)
    {
        const int srcTop = f * kMasterCell;
        const int dstTop = f * cell;

        for (int oy = 0; oy < cell; ++oy)
        {
            const int y0 = int (oy * ratio);
            const int y1 = juce::jmax (y0 + 1, juce::jmin (kMasterCell, int ((oy + 1) * ratio)));

            auto* out = reinterpret_cast<juce::PixelARGB*> (d.getLinePointer (dstTop + oy));

            for (int ox = 0; ox < cell; ++ox)
            {
                const int x0 = int (ox * ratio);
                const int x1 = juce::jmax (x0 + 1, juce::jmin (kMasterCell, int ((ox + 1) * ratio)));

                juce::uint32 a = 0, r = 0, gr = 0, b = 0, n = 0;

                for (int yy = y0; yy < y1; ++yy)
                {
                    auto* row = reinterpret_cast<const juce::PixelARGB*> (s.getLinePointer (srcTop + yy));

                    for (int xx = x0; xx < x1; ++xx)
                    {
                        const auto& p = row[xx];   // premultiplied
                        a  += p.getAlpha();
                        r  += p.getRed();
                        gr += p.getGreen();
                        b  += p.getBlue();
                        ++n;
                    }
                }

                const juce::uint32 h = n / 2;   // round-to-nearest, not truncate
                out[ox].setARGB ((juce::uint8) ((a  + h) / n),
                                 (juce::uint8) ((r  + h) / n),
                                 (juce::uint8) ((gr + h) / n),
                                 (juce::uint8) ((b  + h) / n));
            }
        }
    }

    return dst;
}

//==============================================================================
static const juce::Image& stripAt (int cell)
{
    // Painting is message-thread only, but the lock costs nothing and this is
    // the one piece of state shared across every editor in the process.
    static juce::CriticalSection    lock;
    static std::map<int, juce::Image> cache;

    const juce::ScopedLock sl (lock);

    auto it = cache.find (cell);

    if (it == cache.end())
        it = cache.emplace (cell, cell == kMasterCell ? master()
                                                      : boxDownsample (master(), cell)).first;

    return it->second;   // std::map references are stable across later inserts
}

//==============================================================================
bool draw (juce::Graphics& g, juce::Rectangle<float> dest, float pos)
{
    if (dest.getWidth() < 4.0f || ! master().isValid())
        return false;

    // Resolve the size in *physical* pixels, so the blit below lands 1:1 on a
    // HiDPI display instead of being scaled twice. Clamped at the master size:
    // past 2x scaling the largest dial outgrows the asset and JUCE upscales it,
    // which is soft but never aliased.
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int   cell  = juce::jlimit (8, kMasterCell,
                                      juce::roundToInt (dest.getWidth() * scale));

    const auto& strip = stripAt (cell);

    if (! strip.isValid())
        return false;

    const int frame = juce::jlimit (0, kFrames - 1,
                                    juce::roundToInt (pos * float (kFrames - 1)));

    const juce::Graphics::ScopedSaveState save (g);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (strip,
                 juce::roundToInt (dest.getX()),     juce::roundToInt (dest.getY()),
                 juce::roundToInt (dest.getWidth()), juce::roundToInt (dest.getHeight()),
                 0, frame * cell, cell, cell);

    return true;
}

}  // namespace KnobStrip
