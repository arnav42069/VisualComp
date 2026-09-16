#include "HardwareKnob.h"

#include <BinaryData.h>

namespace HardwareKnob
{
namespace
{
constexpr int kWorkingWidth = 384;

juce::Image boxDownsample(const juce::Image& src, int targetWidth)
{
    if (!src.isValid() || targetWidth >= src.getWidth())
        return src;

    const int targetHeight = juce::jmax(1, juce::roundToInt(
        double(src.getHeight()) * double(targetWidth) / double(src.getWidth())));
    juce::Image dst(juce::Image::ARGB, targetWidth, targetHeight, true);

    const juce::Image::BitmapData source(src, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData output(dst, juce::Image::BitmapData::writeOnly);
    const double scaleX = double(src.getWidth()) / double(targetWidth);
    const double scaleY = double(src.getHeight()) / double(targetHeight);

    for (int oy = 0; oy < targetHeight; ++oy)
    {
        const int y0 = int(double(oy) * scaleY);
        const int y1 = juce::jmax(y0 + 1,
                                  juce::jmin(src.getHeight(), int(double(oy + 1) * scaleY)));
        auto* dstRow = reinterpret_cast<juce::PixelARGB*>(output.getLinePointer(oy));

        for (int ox = 0; ox < targetWidth; ++ox)
        {
            const int x0 = int(double(ox) * scaleX);
            const int x1 = juce::jmax(x0 + 1,
                                      juce::jmin(src.getWidth(), int(double(ox + 1) * scaleX)));
            juce::uint64 a = 0, r = 0, gr = 0, b = 0, count = 0;

            for (int sy = y0; sy < y1; ++sy)
            {
                const auto* srcRow = reinterpret_cast<const juce::PixelARGB*>(
                    source.getLinePointer(sy));
                for (int sx = x0; sx < x1; ++sx)
                {
                    const auto& p = srcRow[sx]; // JUCE ARGB is premultiplied.
                    a  += p.getAlpha();
                    r  += p.getRed();
                    gr += p.getGreen();
                    b  += p.getBlue();
                    ++count;
                }
            }

            const auto round = count / 2;
            dstRow[ox].setARGB(juce::uint8((a + round) / count),
                               juce::uint8((r + round) / count),
                               juce::uint8((gr + round) / count),
                               juce::uint8((b + round) / count));
        }
    }

    return dst;
}

juce::Image loadKeyedBody()
{
    auto image = juce::ImageFileFormat::loadFrom(
        BinaryData::knobhardware5degchroma_png,
        size_t(BinaryData::knobhardware5degchroma_pngSize));
    if (!image.isValid())
        return {};

    image = image.convertedToFormat(juce::Image::ARGB);
    int left = image.getWidth(), top = image.getHeight(), right = -1, bottom = -1;

    {
        juce::Image::BitmapData pixels(image, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < image.getHeight(); ++y)
        {
            auto* row = reinterpret_cast<juce::PixelARGB*>(pixels.getLinePointer(y));
            for (int x = 0; x < image.getWidth(); ++x)
            {
                auto p = row[x];
                p.unpremultiply();
                const int red = p.getRed();
                const int green = p.getGreen();
                const int blue = p.getBlue();
                const int neutral = juce::jmax(red, blue);
                const int greenExcess = green - neutral;

                // The generated source uses a saturated chroma field. A soft
                // ramp, rather than one hard colour comparison, keeps the
                // antialiased silhouette while removing its green spill.
                const float keyedAlpha = juce::jlimit(0.0f, 1.0f,
                    (92.0f - float(greenExcess)) / 54.0f);
                const int alpha = juce::roundToInt(255.0f * keyedAlpha);
                const int cleanGreen = juce::jmin(green, neutral + 7);

                p.setARGB(juce::uint8(alpha), juce::uint8(red),
                          juce::uint8(cleanGreen), juce::uint8(blue));
                p.premultiply();
                row[x] = p;

                if (alpha > 10)
                {
                    left = juce::jmin(left, x);
                    top = juce::jmin(top, y);
                    right = juce::jmax(right, x);
                    bottom = juce::jmax(bottom, y);
                }
            }
        }
    }

    if (right < left || bottom < top)
        return {};

    auto crop = juce::Rectangle<int>::leftTopRightBottom(left, top, right + 1, bottom + 1)
                    .expanded(3)
                    .getIntersection(image.getBounds());
    auto body = image.getClippedImage(crop).createCopy();
    return boxDownsample(body, kWorkingWidth);
}

const juce::Image& body()
{
    static const juce::Image image = loadKeyedBody();
    return image;
}
} // namespace

bool draw(juce::Graphics& g, juce::Rectangle<float> dest,
          juce::Rectangle<float>* fittedBounds)
{
    const auto& image = body();
    if (!image.isValid() || dest.isEmpty())
        return false;

    const float naturalAspect = float(image.getHeight()) / float(image.getWidth());
    float drawW = dest.getWidth();
    float drawH = drawW * naturalAspect;
    if (drawH > dest.getHeight())
    {
        drawH = dest.getHeight();
        drawW = drawH / naturalAspect;
    }
    const auto fitted = juce::Rectangle<float>(drawW, drawH).withCentre(dest.getCentre());
    if (fittedBounds != nullptr)
        *fittedBounds = fitted;

    const juce::Graphics::ScopedSaveState save(g);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(image, fitted);
    return true;
}

} // namespace HardwareKnob
