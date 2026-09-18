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
        if (! i.isValid())
            return i;

        i = i.convertedToFormat (juce::Image::ARGB);
        // Apply the requested 30% darkening once, before all size caches.
        // Scale premultiplied RGB only: opacity and shadow coverage stay intact.
        juce::Image::BitmapData pixels (i, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < i.getHeight(); ++y)
        {
            auto* row = reinterpret_cast<juce::PixelARGB*> (pixels.getLinePointer (y));
            for (int x = 0; x < i.getWidth(); ++x)
            {
                auto& p = row[x];
                p.setARGB (p.getAlpha(),
                           (juce::uint8) ((unsigned (p.getRed()) * 7 + 5) / 10),
                           (juce::uint8) ((unsigned (p.getGreen()) * 7 + 5) / 10),
                           (juce::uint8) ((unsigned (p.getBlue()) * 7 + 5) / 10));
            }
        }
        return i;
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

void drawMetallicFallback (juce::Graphics& g, juce::Rectangle<float> dest, float pos)
{
    const auto c = dest.getCentre();
    const float outerR = juce::jmin (dest.getWidth(), dest.getHeight()) * 0.5f;
    const auto ellipse = [&](float radius, float height)
    {
        const float r = outerR * radius / 64.0f;
        const float centreY = c.y + outerR * (3.0f - height * kTiltSin) / 64.0f;
        return juce::Rectangle<float> (c.x - r, centreY - r * kTopYScale,
                                       r * 2.0f, r * 2.0f * kTopYScale);
    };
    const auto seat = ellipse (53.0f, 2.0f);
    juce::ColourGradient seatGrad (juce::Colour (0xffd9dcda), seat.getX(), seat.getY(),
                                   juce::Colour (0xff8d9290), seat.getRight(), seat.getBottom(), true);
    g.setGradientFill (seatGrad);
    g.fillEllipse (seat);

    const auto shell = ellipse (50.0f, 5.0f);
    g.setColour (juce::Colour (0xff050606));
    g.fillEllipse (shell);

    // The lower shell remains visible below the elevated top: actual height,
    // rather than nested concentric rings that read as a recess.
    const auto bevel = ellipse (44.0f, kShoulderHeight);
    juce::ColourGradient bevelGrad (juce::Colour (0xff35393a), bevel.getX(), bevel.getY(),
                                    juce::Colour (0xff151718), bevel.getRight(), bevel.getBottom(), true);
    g.setGradientFill (bevelGrad);
    g.fillEllipse (bevel);

    const auto face = ellipse (38.0f, kTopHeight);
    juce::ColourGradient faceGrad (juce::Colour (0xff25292a), face.getX(), face.getY(),
                                   juce::Colour (0xff191b1c), face.getRight(), face.getBottom(), true);
    g.setGradientFill (faceGrad);
    g.fillEllipse (face);

    const float a = juce::jmap (pos, 0.0f, 1.0f, juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f);
    const float sa = std::sin (a), ca = -std::cos (a);
    g.setColour (juce::Colour (0xffff7a1f));
    const float topY = c.y + outerR * kTopOffsetYFrac;
    g.drawLine (c.x + outerR * (28.0f / 64.0f) * sa, topY + outerR * (28.0f / 64.0f) * ca * kTopYScale,
                c.x + outerR * (33.5f / 64.0f) * sa, topY + outerR * (33.5f / 64.0f) * ca * kTopYScale,
                juce::jmax (1.0f, outerR * 0.035f));
}

//==============================================================================
bool draw (juce::Graphics& g, juce::Rectangle<float> dest, float pos)
{
    if (dest.getWidth() < 4.0f)
        return false;

    if (! master().isValid())
    {
        drawMetallicFallback (g, dest, pos);
        return true;
    }

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
