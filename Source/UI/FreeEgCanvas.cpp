#include "FreeEgCanvas.h"
#include "../PluginEditor.h"
#include "../PluginProcessor.h"

FreeEgCanvas::FreeEgCanvas (NeomorphX1AudioProcessorEditor& ownerEditor)
    : owner (ownerEditor)
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

juce::Point<float> FreeEgCanvas::getPointPosition (int idx, juce::Rectangle<float> inner) const
{
    const auto prefix = "s" + juce::String (owner.getEditingScene()) + "_feg_pt" + juce::String (idx + 1);
    const float t = owner.getProcessor().apvts.getRawParameterValue (prefix + "_t")->load();
    const float v = owner.getProcessor().apvts.getRawParameterValue (prefix + "_v")->load();

    const float x = inner.getX() + t * inner.getWidth();
    const float y = juce::jmap (v, -1.0f, 1.0f, inner.getBottom(), inner.getY());
    return { x, y };
}

int FreeEgCanvas::findPointAt (juce::Point<float> pos, juce::Rectangle<float> inner) const
{
    int closestIdx    = -1;
    float closestDist = 14.0f;

    for (int i = 0; i < 6; ++i)
    {
        const float d = pos.getDistanceFrom (getPointPosition (i, inner));
        if (d < closestDist)
        {
            closestDist = d;
            closestIdx  = i;
        }
    }
    return closestIdx;
}

void FreeEgCanvas::mouseMove (const juce::MouseEvent& e)
{
    const auto inner = getLocalBounds().toFloat().reduced (8.0f, 8.0f);
    const int newHover = findPointAt (e.position, inner);
    if (newHover != hoveredPoint)
    {
        hoveredPoint = newHover;
        setMouseCursor (hoveredPoint >= 0 ? juce::MouseCursor::DraggingHandCursor
                                          : juce::MouseCursor::CrosshairCursor);
        repaint();
    }
}

void FreeEgCanvas::mouseDown (const juce::MouseEvent& e)
{
    const auto inner = getLocalBounds().toFloat().reduced (8.0f, 8.0f);

    if (e.mods.isRightButtonDown())
    {
        for (int i = 0; i < 6; ++i)
        {
            const auto ptStr = juce::String (i + 1);
            for (int sc = 1; sc <= 2; ++sc)
            {
                if (! owner.isSceneLinkEnabled() && sc != owner.getEditingScene())
                    continue;

                const auto prefix = "s" + juce::String (sc) + "_feg_pt" + ptStr;
                if (auto* pt = owner.getProcessor().apvts.getParameter (prefix + "_t"))
                    pt->setValueNotifyingHost (pt->getDefaultValue());
                if (auto* pv = owner.getProcessor().apvts.getParameter (prefix + "_v"))
                    pv->setValueNotifyingHost (pv->getDefaultValue());
            }
        }
        owner.triggerLcdNotification ("S" + juce::String (owner.getEditingScene()) + " // FREE EG", "CURVE RESET");
        repaint();
        return;
    }

    draggedPoint = findPointAt (e.position, inner);
}

void FreeEgCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedPoint < 0)
        return;

    const auto inner = getLocalBounds().toFloat().reduced (8.0f, 8.0f);
    const auto prefix = "s" + juce::String (owner.getEditingScene()) + "_feg_";

    float newT = juce::jlimit (0.0f, 1.0f, (e.position.x - inner.getX()) / juce::jmax (1.0f, inner.getWidth()));
    float newV = juce::jlimit (-1.0f, 1.0f, juce::jmap (e.position.y, inner.getBottom(), inner.getY(), -1.0f, 1.0f));

    if (draggedPoint == 0)
        newT = 0.0f;
    else if (draggedPoint == 5)
        newT = 1.0f;
    else
    {
        const float prevT = owner.getProcessor().apvts.getRawParameterValue (prefix + "pt" + juce::String (draggedPoint)     + "_t")->load();
        const float nextT = owner.getProcessor().apvts.getRawParameterValue (prefix + "pt" + juce::String (draggedPoint + 2) + "_t")->load();
        newT = juce::jlimit (prevT + 0.02f, nextT - 0.02f, newT);
    }

    const auto ptStr = juce::String (draggedPoint + 1);
    for (int sc = 1; sc <= 2; ++sc)
    {
        if (! owner.isSceneLinkEnabled() && sc != owner.getEditingScene())
            continue;

        const auto scPrefix = "s" + juce::String (sc) + "_feg_pt" + ptStr;
        if (auto* pt = owner.getProcessor().apvts.getParameter (scPrefix + "_t"))
            pt->setValueNotifyingHost (pt->convertTo0to1 (newT));
        if (auto* pv = owner.getProcessor().apvts.getParameter (scPrefix + "_v"))
            pv->setValueNotifyingHost (pv->convertTo0to1 (newV));
    }

    owner.triggerLcdNotification (
        juce::String::formatted ("S%d // FEG PT %d", owner.getEditingScene(), draggedPoint + 1),
        juce::String::formatted ("T:%d%%  V:%+d%%", juce::roundToInt (newT * 100.0f), juce::roundToInt (newV * 100.0f)));
    repaint();
}

void FreeEgCanvas::mouseUp (const juce::MouseEvent&)
{
    draggedPoint = -1;
}

void FreeEgCanvas::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto cyan   = juce::Colour::fromRGB (0, 240, 255);
    const auto pink   = juce::Colour::fromRGB (255, 60, 180);

    g.setColour (juce::Colour::fromRGB (6, 10, 16));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (juce::Colour::fromRGB (35, 55, 75).withAlpha (0.7f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    const auto inner = bounds.reduced (8.0f, 8.0f);

    g.setColour (cyan.withAlpha (0.14f));
    g.drawHorizontalLine (juce::roundToInt (inner.getCentreY()), inner.getX(), inner.getRight());
    for (int col = 1; col < 4; ++col)
    {
        const float gx = inner.getX() + (inner.getWidth() * 0.25f) * static_cast<float> (col);
        g.drawVerticalLine (juce::roundToInt (gx), inner.getY(), inner.getBottom());
    }

    std::array<juce::Point<float>, 6> pts;
    for (int i = 0; i < 6; ++i)
        pts[static_cast<size_t> (i)] = getPointPosition (i, inner);

    g.setColour (pink.withAlpha (0.45f));
    g.drawVerticalLine (juce::roundToInt (pts[4].x), inner.getY(), inner.getBottom());

    juce::Path curvePath;
    curvePath.startNewSubPath (pts[0]);
    for (size_t i = 1; i < 6; ++i)
        curvePath.lineTo (pts[i]);

    juce::Path fillPath (curvePath);
    fillPath.lineTo (pts[5].x, inner.getCentreY());
    fillPath.lineTo (pts[0].x, inner.getCentreY());
    fillPath.closeSubPath();

    g.setColour (cyan.withAlpha (0.14f));
    g.fillPath (fillPath);

    g.setColour (cyan.withAlpha (0.30f));
    g.strokePath (curvePath, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cyan);
    g.strokePath (curvePath, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (int i = 0; i < 6; ++i)
    {
        const auto p       = pts[static_cast<size_t> (i)];
        const bool active  = (i == draggedPoint || i == hoveredPoint);
        const auto ptCol   = (i == 4) ? pink : cyan;
        const float radius = active ? 5.0f : 3.6f;

        if (active)
        {
            g.setColour (ptCol.withAlpha (0.35f));
            g.fillEllipse (p.x - radius * 1.8f, p.y - radius * 1.8f, radius * 3.6f, radius * 3.6f);
        }

        g.setColour (juce::Colour::fromRGB (10, 15, 24));
        g.fillEllipse (p.x - radius, p.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour (ptCol);
        g.drawEllipse (p.x - radius, p.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);
    }
}
