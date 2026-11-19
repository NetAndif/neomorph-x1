#include "StepSeqCanvas.h"
#include "../PluginEditor.h"
#include "../PluginProcessor.h"

StepSeqCanvas::StepSeqCanvas (NeomorphX1AudioProcessorEditor& ownerEditor)
    : owner (ownerEditor)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void StepSeqCanvas::mouseDown (const juce::MouseEvent& e)
{
    lastEditedStep = -1;
    applyMouseEdit (e, e.mods.isRightButtonDown());
}

void StepSeqCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (! e.mods.isRightButtonDown())
        applyMouseEdit (e, false);
}

void StepSeqCanvas::applyMouseEdit (const juce::MouseEvent& e, bool toggleGateOnly)
{
    const auto inner = getLocalBounds().toFloat().reduced (6.0f, 6.0f);
    const float colW = inner.getWidth() / 16.0f;

    const int stepIdx = juce::jlimit (0, 15, static_cast<int> ((e.position.x - inner.getX()) / juce::jmax (1.0f, colW)));
    const auto stStr  = juce::String (stepIdx + 1);

    if (toggleGateOnly)
    {
        if (stepIdx == lastEditedStep)
            return;
        lastEditedStep = stepIdx;

        const auto baseGateID = "s" + juce::String (owner.getEditingScene()) + "_seq_st" + stStr + "_g";
        const bool wasOn      = (owner.getProcessor().apvts.getRawParameterValue (baseGateID)->load() > 0.5f);
        const float newGate   = wasOn ? 0.0f : 1.0f;

        for (int sc = 1; sc <= 2; ++sc)
        {
            if (! owner.isSceneLinkEnabled() && sc != owner.getEditingScene())
                continue;
            if (auto* pg = owner.getProcessor().apvts.getParameter ("s" + juce::String (sc) + "_seq_st" + stStr + "_g"))
                pg->setValueNotifyingHost (newGate);
        }

        owner.triggerLcdNotification (
            juce::String::formatted ("S%d // STEP %02d GATE", owner.getEditingScene(), stepIdx + 1),
            wasOn ? "MUTED" : "ACTIVE");
        repaint();
        return;
    }

    const float rawNorm = juce::jlimit (-1.0f, 1.0f, juce::jmap (e.position.y, inner.getBottom(), inner.getY(), -1.0f, 1.0f));
    const int mode      = owner.getStepSeqModeIndex();

    const float val = (mode == 1) ? (std::round (rawNorm * 12.0f) / 12.0f) : rawNorm;

    for (int sc = 1; sc <= 2; ++sc)
    {
        if (! owner.isSceneLinkEnabled() && sc != owner.getEditingScene())
            continue;
        if (auto* pv = owner.getProcessor().apvts.getParameter ("s" + juce::String (sc) + "_seq_st" + stStr + "_v"))
            pv->setValueNotifyingHost (pv->convertTo0to1 (val));
    }

    if (mode == 1)
    {
        const int semi = juce::roundToInt (val * 12.0f);
        owner.triggerLcdNotification (
            juce::String::formatted ("S%d // STEP %02d PITCH", owner.getEditingScene(), stepIdx + 1),
            juce::String::formatted ("%+d ST", semi));
    }
    else
    {
        owner.triggerLcdNotification (
            juce::String::formatted ("S%d // STEP %02d VALUE", owner.getEditingScene(), stepIdx + 1),
            juce::String::formatted ("%+d%%", juce::roundToInt (val * 100.0f)));
    }

    repaint();
}

void StepSeqCanvas::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto cyan   = juce::Colour::fromRGB (0, 240, 255);
    const auto pink   = juce::Colour::fromRGB (255, 60, 180);

    g.setColour (juce::Colour::fromRGB (6, 10, 16));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (juce::Colour::fromRGB (35, 55, 75).withAlpha (0.7f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    const auto inner = bounds.reduced (6.0f, 6.0f);
    const float midY = inner.getCentreY();
    const float colW = inner.getWidth() / 16.0f;

    constexpr int seqLengths[3] = { 8, 12, 16 };
    const int activeLen = seqLengths[juce::jlimit (0, 2, owner.getStepSeqLengthIndex())];
    const int playingStep = owner.getProcessor().getCurrentSeqStep (owner.getEditingScene());

    g.setColour (cyan.withAlpha (0.16f));
    g.drawHorizontalLine (juce::roundToInt (midY), inner.getX(), inner.getRight());

    for (int st = 0; st < 16; ++st)
    {
        const auto stStr  = juce::String (st + 1);
        const auto prefix = "s" + juce::String (owner.getEditingScene()) + "_seq_st" + stStr;
        const float val   = owner.getProcessor().apvts.getRawParameterValue (prefix + "_v")->load();
        const bool gateOn = (owner.getProcessor().apvts.getRawParameterValue (prefix + "_g")->load() > 0.5f);
        const bool inRange = (st < activeLen);
        const bool isCurrent = (st == playingStep && inRange);

        auto cell = juce::Rectangle<float> (inner.getX() + static_cast<float> (st) * colW + 1.5f,
                                            inner.getY(),
                                            colW - 3.0f,
                                            inner.getHeight());

        if ((st % 4) == 0 && st > 0)
        {
            g.setColour (cyan.withAlpha (0.12f));
            g.drawVerticalLine (juce::roundToInt (cell.getX() - 1.5f), inner.getY(), inner.getBottom());
        }

        if (isCurrent)
        {
            g.setColour (pink.withAlpha (0.18f));
            g.fillRoundedRectangle (cell, 2.0f);
        }

        const auto barCol = ! inRange ? juce::Colour::fromRGB (45, 55, 70).withAlpha (0.35f)
                          : ! gateOn  ? juce::Colour::fromRGB (70, 90, 110).withAlpha (0.35f)
                          : isCurrent ? pink
                                      : cyan;

        const float valY = juce::jmap (val, -1.0f, 1.0f, inner.getBottom(), inner.getY());
        const float topY = juce::jmin (midY, valY);
        const float barH = juce::jmax (2.5f, std::abs (valY - midY));

        auto barRect = juce::Rectangle<float> (cell.getX(), topY, cell.getWidth(), barH);
        g.setColour (barCol.withAlpha (gateOn && inRange ? 0.78f : 0.30f));
        g.fillRoundedRectangle (barRect, 2.0f);

        g.setColour (barCol);
        g.drawHorizontalLine (juce::roundToInt (valY), cell.getX(), cell.getRight());
    }
}
