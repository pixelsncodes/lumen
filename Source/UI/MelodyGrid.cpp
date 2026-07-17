#include "UI/MelodyGrid.h"

namespace melodygrid
{
namespace
{
    juce::Rectangle<float> cellRect (const DrawInfo& info, int col, int row)
    {
        const float cw = info.imageArea.getWidth() / juce::jmax (1, info.cols);
        const float ch = info.imageArea.getHeight() / juce::jmax (1, info.rows);
        return { info.imageArea.getX() + col * cw, info.imageArea.getY() + row * ch, cw, ch };
    }
} // namespace

void draw (juce::Graphics& g, const DrawInfo& info)
{
    const auto area = info.imageArea;
    if (area.isEmpty() || info.cols <= 0 || info.rows <= 0)
        return;

    // Thin, semi-transparent accent grid lines.
    g.setColour (info.accent.withAlpha (0.22f));
    for (int c = 1; c < info.cols; ++c)
    {
        const float x = area.getX() + area.getWidth() * c / info.cols;
        g.drawLine (x, area.getY(), x, area.getBottom(), 1.0f);
    }
    for (int r = 1; r < info.rows; ++r)
    {
        const float y = area.getY() + area.getHeight() * r / info.rows;
        g.drawLine (area.getX(), y, area.getRight(), y, 1.0f);
    }

    // Stopped: trace the sampled path faintly (dots joined in visiting order).
    if (! info.playing && info.seq != nullptr && ! info.seq->steps.empty())
    {
        juce::Path path;
        for (std::size_t i = 0; i < info.seq->steps.size(); ++i)
        {
            const auto& s = info.seq->steps[i];
            const auto centre = cellRect (info, s.col, s.row).getCentre();
            if (i == 0) path.startNewSubPath (centre);
            else        path.lineTo (centre);
        }
        g.setColour (info.accent.withAlpha (0.30f));
        g.strokePath (path, juce::PathStrokeType (1.2f));

        g.setColour (info.accent.withAlpha (0.55f));
        for (const auto& s : info.seq->steps)
        {
            const auto centre = cellRect (info, s.col, s.row).getCentre();
            g.fillEllipse (centre.x - 1.6f, centre.y - 1.6f, 3.2f, 3.2f);
        }
    }

    // Playing: glow the currently sounding cell, fading over ~150 ms.
    if (info.glow > 0.001f && info.liveCol >= 0 && info.liveRow >= 0)
    {
        const auto cell = cellRect (info, info.liveCol, info.liveRow);
        g.setColour (info.accent.withAlpha (0.10f + 0.45f * info.glow));
        g.fillRect (cell);
        g.setColour (info.accent.withAlpha (0.35f + 0.55f * info.glow));
        g.drawRect (cell, 1.4f);
    }
}
} // namespace melodygrid
