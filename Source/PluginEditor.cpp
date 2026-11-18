#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Interaktiver 6-Punkt-Kurven-Editor für Tab 4 (Free EG)
// ==============================================================================
NeomorphX1AudioProcessorEditor::FreeEgCanvas::FreeEgCanvas (NeomorphX1AudioProcessorEditor& ownerEditor)
    : owner (ownerEditor)
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

juce::Point<float> NeomorphX1AudioProcessorEditor::FreeEgCanvas::getPointPosition (int idx, juce::Rectangle<float> inner) const
{
    const auto prefix = "s" + juce::String (owner.editingScene) + "_feg_pt" + juce::String (idx + 1);
    const float t = owner.audioProcessor.apvts.getRawParameterValue (prefix + "_t")->load();
    const float v = owner.audioProcessor.apvts.getRawParameterValue (prefix + "_v")->load();

    const float x = inner.getX() + t * inner.getWidth();
    const float y = juce::jmap (v, -1.0f, 1.0f, inner.getBottom(), inner.getY());
    return { x, y };
}

int NeomorphX1AudioProcessorEditor::FreeEgCanvas::findPointAt (juce::Point<float> pos, juce::Rectangle<float> inner) const
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

void NeomorphX1AudioProcessorEditor::FreeEgCanvas::mouseMove (const juce::MouseEvent& e)
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

void NeomorphX1AudioProcessorEditor::FreeEgCanvas::mouseDown (const juce::MouseEvent& e)
{
    const auto inner = getLocalBounds().toFloat().reduced (8.0f, 8.0f);

    if (e.mods.isRightButtonDown())
    {
        for (int i = 0; i < 6; ++i)
        {
            const auto ptStr = juce::String (i + 1);
            for (int sc = 1; sc <= 2; ++sc)
            {
                if (! owner.sceneLink && sc != owner.editingScene)
                    continue;

                const auto prefix = "s" + juce::String (sc) + "_feg_pt" + ptStr;
                if (auto* pt = owner.audioProcessor.apvts.getParameter (prefix + "_t"))
                    pt->setValueNotifyingHost (pt->getDefaultValue());
                if (auto* pv = owner.audioProcessor.apvts.getParameter (prefix + "_v"))
                    pv->setValueNotifyingHost (pv->getDefaultValue());
            }
        }
        owner.triggerLcdNotification ("S" + juce::String (owner.editingScene) + " // FREE EG", "CURVE RESET");
        repaint();
        return;
    }

    draggedPoint = findPointAt (e.position, inner);
}

void NeomorphX1AudioProcessorEditor::FreeEgCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedPoint < 0)
        return;

    const auto inner = getLocalBounds().toFloat().reduced (8.0f, 8.0f);
    const auto prefix = "s" + juce::String (owner.editingScene) + "_feg_";

    float newT = juce::jlimit (0.0f, 1.0f, (e.position.x - inner.getX()) / juce::jmax (1.0f, inner.getWidth()));
    float newV = juce::jlimit (-1.0f, 1.0f, juce::jmap (e.position.y, inner.getBottom(), inner.getY(), -1.0f, 1.0f));

    if (draggedPoint == 0)
        newT = 0.0f;
    else if (draggedPoint == 5)
        newT = 1.0f;
    else
    {
        const float prevT = owner.audioProcessor.apvts.getRawParameterValue (prefix + "pt" + juce::String (draggedPoint)     + "_t")->load();
        const float nextT = owner.audioProcessor.apvts.getRawParameterValue (prefix + "pt" + juce::String (draggedPoint + 2) + "_t")->load();
        newT = juce::jlimit (prevT + 0.02f, nextT - 0.02f, newT);
    }

    const auto ptStr = juce::String (draggedPoint + 1);
    for (int sc = 1; sc <= 2; ++sc)
    {
        if (! owner.sceneLink && sc != owner.editingScene)
            continue;

        const auto scPrefix = "s" + juce::String (sc) + "_feg_pt" + ptStr;
        if (auto* pt = owner.audioProcessor.apvts.getParameter (scPrefix + "_t"))
            pt->setValueNotifyingHost (pt->convertTo0to1 (newT));
        if (auto* pv = owner.audioProcessor.apvts.getParameter (scPrefix + "_v"))
            pv->setValueNotifyingHost (pv->convertTo0to1 (newV));
    }

    owner.triggerLcdNotification (
        juce::String::formatted ("S%d // FEG PT %d", owner.editingScene, draggedPoint + 1),
        juce::String::formatted ("T:%d%%  V:%+d%%", juce::roundToInt (newT * 100.0f), juce::roundToInt (newV * 100.0f)));
    repaint();
}

void NeomorphX1AudioProcessorEditor::FreeEgCanvas::mouseUp (const juce::MouseEvent&)
{
    draggedPoint = -1;
}

void NeomorphX1AudioProcessorEditor::FreeEgCanvas::paint (juce::Graphics& g)
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

// ==============================================================================
// Interaktives 16-Step-Canvas für Tab 5 (Step Sequencer)
// ==============================================================================
NeomorphX1AudioProcessorEditor::StepSeqCanvas::StepSeqCanvas (NeomorphX1AudioProcessorEditor& ownerEditor)
    : owner (ownerEditor)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void NeomorphX1AudioProcessorEditor::StepSeqCanvas::mouseDown (const juce::MouseEvent& e)
{
    lastEditedStep = -1;
    applyMouseEdit (e, e.mods.isRightButtonDown());
}

void NeomorphX1AudioProcessorEditor::StepSeqCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (! e.mods.isRightButtonDown())
        applyMouseEdit (e, false);
}

void NeomorphX1AudioProcessorEditor::StepSeqCanvas::applyMouseEdit (const juce::MouseEvent& e, bool toggleGateOnly)
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

        const auto baseGateID = "s" + juce::String (owner.editingScene) + "_seq_st" + stStr + "_g";
        const bool wasOn      = (owner.audioProcessor.apvts.getRawParameterValue (baseGateID)->load() > 0.5f);
        const float newGate   = wasOn ? 0.0f : 1.0f;

        for (int sc = 1; sc <= 2; ++sc)
        {
            if (! owner.sceneLink && sc != owner.editingScene)
                continue;
            if (auto* pg = owner.audioProcessor.apvts.getParameter ("s" + juce::String (sc) + "_seq_st" + stStr + "_g"))
                pg->setValueNotifyingHost (newGate);
        }

        owner.triggerLcdNotification (
            juce::String::formatted ("S%d // STEP %02d GATE", owner.editingScene, stepIdx + 1),
            wasOn ? "MUTED" : "ACTIVE");
        repaint();
        return;
    }

    const float rawNorm = juce::jlimit (-1.0f, 1.0f, juce::jmap (e.position.y, inner.getBottom(), inner.getY(), -1.0f, 1.0f));
    const int mode      = owner.stepSeqControls.modeBox.getSelectedItemIndex();

    const float val = (mode == 1) ? (std::round (rawNorm * 12.0f) / 12.0f) : rawNorm;

    for (int sc = 1; sc <= 2; ++sc)
    {
        if (! owner.sceneLink && sc != owner.editingScene)
            continue;
        if (auto* pv = owner.audioProcessor.apvts.getParameter ("s" + juce::String (sc) + "_seq_st" + stStr + "_v"))
            pv->setValueNotifyingHost (pv->convertTo0to1 (val));
    }

    if (mode == 1)
    {
        const int semi = juce::roundToInt (val * 12.0f);
        owner.triggerLcdNotification (
            juce::String::formatted ("S%d // STEP %02d PITCH", owner.editingScene, stepIdx + 1),
            juce::String::formatted ("%+d ST", semi));
    }
    else
    {
        owner.triggerLcdNotification (
            juce::String::formatted ("S%d // STEP %02d VALUE", owner.editingScene, stepIdx + 1),
            juce::String::formatted ("%+d%%", juce::roundToInt (val * 100.0f)));
    }

    repaint();
}

void NeomorphX1AudioProcessorEditor::StepSeqCanvas::paint (juce::Graphics& g)
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
    const int activeLen = seqLengths[juce::jlimit (0, 2, owner.stepSeqControls.lengthBox.getSelectedItemIndex())];
    const int playingStep = owner.audioProcessor.getCurrentSeqStep (owner.editingScene);

    g.setColour (cyan.withAlpha (0.16f));
    g.drawHorizontalLine (juce::roundToInt (midY), inner.getX(), inner.getRight());

    for (int st = 0; st < 16; ++st)
    {
        const auto stStr  = juce::String (st + 1);
        const auto prefix = "s" + juce::String (owner.editingScene) + "_seq_st" + stStr;
        const float val   = owner.audioProcessor.apvts.getRawParameterValue (prefix + "_v")->load();
        const bool gateOn = (owner.audioProcessor.apvts.getRawParameterValue (prefix + "_g")->load() > 0.5f);
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

// ==============================================================================
// NeomorphX1AudioProcessorEditor Konstruktor
// ==============================================================================
NeomorphX1AudioProcessorEditor::NeomorphX1AudioProcessorEditor (NeomorphX1AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&cyberpunkLookAndFeel);
    setWantsKeyboardFocus (true);

    backgroundImage = juce::ImageCache::getFromMemory (BinaryData::background_main_png,
                                                       BinaryData::background_main_pngSize);

    // --- Top-Bar Preset-Verwaltung ---
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.setTooltip ("Select Preset");
    presetBox.onChange = [this]
    {
        if (suppressPresetBoxCallback)
            return;

        const auto selectedName = presetBox.getText();
        if (selectedName.isNotEmpty() && audioProcessor.loadPresetByName (selectedName))
        {
            freeEgControls.canvas.repaint();
            stepSeqControls.canvas.repaint();
            triggerLcdNotification ("PRESET LOADED", selectedName);
        }
    };
    addAndMakeVisible (presetBox);

    savePresetButton.setAlpha (0.0f);
    savePresetButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    savePresetButton.setTooltip ("Save Preset (Right-Click: Save As...)");
    savePresetButton.onClick = [this] { handleSavePresetClick (false); };
    savePresetButton.onRightClick = [this] { handleSavePresetClick (true); };
    addAndMakeVisible (savePresetButton);

    loadPresetButton.setAlpha (0.0f);
    loadPresetButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    loadPresetButton.setTooltip ("Load / Import External Preset (.xml)");
    loadPresetButton.onClick = [this] { handleLoadPresetClick(); };
    addAndMakeVisible (loadPresetButton);

    panicButton.setAlpha (0.0f);
    panicButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    panicButton.setTooltip ("All Notes Off & Reset Audio Engine");
    panicButton.onClick = [this] { handlePanicClick(); };
    addAndMakeVisible (panicButton);

    prevPresetButton.setAlpha (0.0f);
    prevPresetButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    prevPresetButton.setTooltip ("Previous Preset");
    prevPresetButton.onClick = [this] { stepPreset (-1); };
    addAndMakeVisible (prevPresetButton);

    nextPresetButton.setAlpha (0.0f);
    nextPresetButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    nextPresetButton.setTooltip ("Next Preset");
    nextPresetButton.onClick = [this] { stepPreset (1); };
    addAndMakeVisible (nextPresetButton);

    refreshPresetList();

    // --- 10er-Nummernblock (1..9, CLR, 0, ENT) ---
    for (size_t i = 0; i < keypadButtons.size(); ++i)
    {
        auto& btn = keypadButtons[i];
        btn.setAlpha (0.0f);
        btn.setMouseCursor (juce::MouseCursor::PointingHandCursor);

        if (i < 9)
        {
            const int digit = static_cast<int> (i + 1);
            btn.setTooltip ("Keypad " + juce::String (digit));
            btn.onClick = [this, digit] { handleKeypadDigit (digit); };
        }
        else if (i == 9)
        {
            btn.setTooltip ("Clear Input (or Toggle +/- if Empty)");
            btn.onClick = [this] { handleKeypadClear(); };
        }
        else if (i == 10)
        {
            btn.setTooltip ("Keypad 0");
            btn.onClick = [this] { handleKeypadDigit (0); };
        }
        else
        {
            btn.setTooltip ("Apply Value (Enter)");
            btn.onClick = [this] { handleKeypadEnter(); };
        }

        addAndMakeVisible (btn);
    }

    for (size_t i = 0; i < tabButtons.size(); ++i)
    {
        auto& btn = tabButtons[i];
        btn.setAlpha (0.0f);
        btn.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        btn.onClick = [this, i] { setActiveTab (static_cast<ActiveTab> (i)); };
        addAndMakeVisible (btn);
    }

    for (size_t i = 0; i < sceneHardwareButtons.size(); ++i)
    {
        auto& btn = sceneHardwareButtons[i];
        btn.setAlpha (0.0f);
        btn.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        btn.onClick = [this, i] { handleSceneButton (static_cast<int> (i)); };
        addAndMakeVisible (btn);
    }

    for (size_t i = 0; i < lcdPageButtons.size(); ++i)
    {
        auto& btn = lcdPageButtons[i];
        btn.setAlpha (0.0f);
        btn.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        btn.onClick = [this, i]
        {
            currentLcdPage = static_cast<LcdPage> (i);
            showLcdNotify  = false;
            keypadActive   = false;
            repaint();
        };
        addAndMakeVisible (btn);
    }

    settingsButton.setAlpha (0.0f);
    settingsButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    settingsButton.onClick = [this] { showSettingsMenu(); };
    addAndMakeVisible (settingsButton);

    // 10 rechte Drehregler: [0] = Master Volume (80%), [1] = Master Pan (50%), [2..9] = Assign 1..8
    for (size_t i = 0; i < matrixKnobs.size(); ++i)
    {
        auto& knob = matrixKnobs[i];
        knob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

        const auto accentCol = (i == 0) ? juce::Colour::fromRGB (0, 240, 255)
                             : (i == 1) ? juce::Colour::fromRGB (255, 60, 180)
                                        : juce::Colour::fromRGB (0, 240, 255);
        knob.setColour (juce::Slider::rotarySliderFillColourId, accentCol);
        knob.getProperties().set ("isBipolar", i == 1);

        const auto paramID = "knob_" + juce::String (static_cast<int> (i + 1));
        if (auto* param = audioProcessor.apvts.getParameter (paramID))
            knob.setDoubleClickReturnValue (true, param->getDefaultValue());

        knob.onValueChange = [this, i, paramID]
        {
            const double val = matrixKnobs[i].getValue();
            if (i == 0)
            {
                setKeypadTarget (paramID, "MASTER VOLUME");
                const int pct = juce::roundToInt (val * 100.0);
                triggerLcdNotification ("MASTER VOLUME", juce::String (pct) + "%");
            }
            else if (i == 1)
            {
                setKeypadTarget (paramID, "MASTER PAN");
                const int panOffset = juce::roundToInt ((val - 0.5) * 200.0);
                if (std::abs (panOffset) <= 1)
                    triggerLcdNotification ("MASTER PAN", "CENTER (50%)");
                else
                    triggerLcdNotification ("MASTER PAN",
                                            juce::String::formatted ("%d%% %s", std::abs (panOffset), panOffset < 0 ? "L" : "R"));
            }
            else
            {
                const auto lbl = "MATRIX ASSIGN " + juce::String (static_cast<int> (i - 1));
                setKeypadTarget (paramID, lbl);
                const int pct = juce::roundToInt (val * 100.0);
                triggerLcdNotification (lbl, juce::String (pct) + "%");
            }
        };

        addAndMakeVisible (knob);

        knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, paramID, knob);
    }

    const char* faderParamIDs[3] = { "scene_1", "scene_morph", "scene_2" };
    const char* faderLabels[3]   = { "SCENE 1 LEVEL", "SCENE MORPH", "SCENE 2 LEVEL" };
    const juce::Colour faderColours[3] = {
        juce::Colour::fromRGB (0, 240, 255),
        juce::Colour::fromRGB (255, 60, 180),
        juce::Colour::fromRGB (0, 240, 255)
    };

    for (size_t i = 0; i < sceneFaders.size(); ++i)
    {
        auto& fader = sceneFaders[i];
        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setColour (juce::Slider::thumbColourId, faderColours[i]);

        const juce::String pID (faderParamIDs[i]);
        if (auto* param = audioProcessor.apvts.getParameter (pID))
            fader.setDoubleClickReturnValue (true, param->getDefaultValue());

        const juce::String label (faderLabels[i]);
        fader.onValueChange = [this, i, pID, label]
        {
            setKeypadTarget (pID, label);
            const int pct = juce::roundToInt (sceneFaders[i].getValue() * 100.0);
            triggerLcdNotification (label, juce::String (pct) + "%");
        };

        addAndMakeVisible (fader);

        faderAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, pID, fader);
    }

    pitchWheel.setSliderStyle (juce::Slider::LinearVertical);
    pitchWheel.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pitchWheel.getProperties().set ("isWheel", true);
    pitchWheel.setColour (juce::Slider::thumbColourId, juce::Colour::fromRGB (0, 240, 255));
    pitchWheel.springBackEnabled = true;
    pitchWheel.restingValue      = 0.0;
    pitchWheel.onValueChange = [this]
    {
        const int pct = juce::roundToInt (pitchWheel.getValue() * 100.0);
        triggerLcdNotification ("PITCH BEND", juce::String::formatted ("%+d%%", pct));
    };
    addAndMakeVisible (pitchWheel);
    pitchWheelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, "pitch_wheel", pitchWheel);

    modWheel.setSliderStyle (juce::Slider::LinearVertical);
    modWheel.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    modWheel.getProperties().set ("isWheel", true);
    modWheel.setColour (juce::Slider::thumbColourId, juce::Colour::fromRGB (255, 60, 180));
    modWheel.setDoubleClickReturnValue (true, 0.0);
    modWheel.onValueChange = [this]
    {
        setKeypadTarget ("mod_wheel", "MODULATION WHEEL");
        const int pct = juce::roundToInt (modWheel.getValue() * 100.0);
        triggerLcdNotification ("MODULATION WHEEL", juce::String (pct) + "%");
    };
    addAndMakeVisible (modWheel);
    modWheelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, "mod_wheel", modWheel);

    const auto cyanAccent    = juce::Colour::fromRGB (0, 240, 255);
    const auto magentaAccent = juce::Colour::fromRGB (255, 60, 180);

    // --- Tab 1: Oscillators Controls ---
    const juce::StringArray waveformItems = {
        "Basic Analog", "PWM / Pulse", "Formant", "Digital Fold", "SuperSaw", "Vocal Morph"
    };

    for (size_t i = 0; i < oscControls.size(); ++i)
    {
        auto& osc = oscControls[i];
        const int oscNum  = static_cast<int> (i + 1);
        const auto accent = (i == 0) ? cyanAccent : magentaAccent;

        osc.waveBox.addItemList (waveformItems, 1);
        osc.waveBox.setJustificationType (juce::Justification::centredLeft);
        osc.waveBox.onChange = [this, oscNum, i]
        {
            if (sceneLink)
            {
                const int otherScene = (editingScene == 1) ? 2 : 1;
                const auto srcID = "s" + juce::String (editingScene) + "_osc" + juce::String (oscNum) + "_wave";
                const auto dstID = "s" + juce::String (otherScene)   + "_osc" + juce::String (oscNum) + "_wave";
                if (auto* srcP = audioProcessor.apvts.getParameter (srcID))
                    if (auto* dstP = audioProcessor.apvts.getParameter (dstID))
                        dstP->setValueNotifyingHost (srcP->getValue());
            }

            triggerLcdNotification (juce::String::formatted ("S%d // OSC %d TABLE", editingScene, oscNum),
                                    oscControls[i].waveBox.getText());
        };
        addChildComponent (osc.waveBox);

        auto setupOscKnob = [this, accent, oscNum] (juce::Slider& slider,
                                                    const juce::String& paramSuffix,
                                                    bool bipolar,
                                                    std::function<juce::String (double)> formatTitle,
                                                    std::function<juce::String (double)> formatVal)
        {
            slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
            slider.getProperties().set ("isBipolar", bipolar);

            slider.onValueChange = [this, &slider, oscNum, paramSuffix, formatTitle, formatVal]
            {
                const auto srcID = "s" + juce::String (editingScene) + "_osc" + juce::String (oscNum) + "_" + paramSuffix;
                if (sceneLink)
                {
                    const int otherScene = (editingScene == 1) ? 2 : 1;
                    const auto dstID = "s" + juce::String (otherScene) + "_osc" + juce::String (oscNum) + "_" + paramSuffix;
                    if (auto* srcP = audioProcessor.apvts.getParameter (srcID))
                        if (auto* dstP = audioProcessor.apvts.getParameter (dstID))
                            dstP->setValueNotifyingHost (srcP->getValue());
                }

                const double v = slider.getValue();
                const auto title = formatTitle (v);
                setKeypadTarget (srcID, title);
                triggerLcdNotification (title, formatVal (v));
            };

            addChildComponent (slider);
        };

        setupOscKnob (osc.octaveKnob, "octave", true,
            [this, oscNum] (double) { return juce::String::formatted ("S%d // OSC %d OCTAVE", editingScene, oscNum); },
            [] (double v) { return juce::String::formatted ("%+d OCT", juce::roundToInt (v)); });

        setupOscKnob (osc.semiKnob, "semi", true,
            [this, oscNum] (double) { return juce::String::formatted ("S%d // OSC %d SEMITONE", editingScene, oscNum); },
            [] (double v) { return juce::String::formatted ("%+d ST", juce::roundToInt (v)); });

        setupOscKnob (osc.detuneKnob, "detune", true,
            [this, oscNum] (double) { return juce::String::formatted ("S%d // OSC %d DETUNE", editingScene, oscNum); },
            [] (double v) { return juce::String::formatted ("%+.1f CT", v); });

        setupOscKnob (osc.positionKnob, "position", false,
            [this, oscNum] (double) { return juce::String::formatted ("S%d // OSC %d POSITION", editingScene, oscNum); },
            [] (double v) { return juce::String::formatted ("%d%%", juce::roundToInt (v * 100.0)); });

        setupOscKnob (osc.morphKnob, "morph", false,
            [this, oscNum] (double) { return juce::String::formatted ("S%d // OSC %d MORPH", editingScene, oscNum); },
            [] (double v) { return juce::String::formatted ("%d%%", juce::roundToInt (v * 100.0)); });

        setupOscKnob (osc.phaseKnob, "phase", false,
            [this, oscNum] (double) { return juce::String::formatted ("S%d // OSC %d PHASE", editingScene, oscNum); },
            [] (double v) { return juce::String::formatted ("%d DEG", juce::roundToInt (v)); });

        setupOscKnob (osc.panKnob, "pan", true,
            [this, oscNum] (double) { return juce::String::formatted ("S%d // OSC %d PAN", editingScene, oscNum); },
            [] (double v) {
                const int pct = juce::roundToInt (v * 100.0);
                if (pct == 0) return juce::String ("CENTER");
                return juce::String::formatted ("%d%% %s", std::abs (pct), pct < 0 ? "L" : "R");
            });

        setupOscKnob (osc.levelKnob, "level", false,
            [this, oscNum] (double) { return juce::String::formatted ("S%d // OSC %d LEVEL", editingScene, oscNum); },
            [] (double v) { return juce::String::formatted ("%d%%", juce::roundToInt (v * 100.0)); });
    }

    // --- Tab 2: Filter Controls ---
    const juce::StringArray filterTypeItems = {
        "LP 12dB", "LP 24dB Ladder", "BP 12dB", "HP 12dB", "Notch", "Comb / Formant"
    };
    const juce::StringArray filterRoutingItems = {
        "Osc 1 + 2", "Osc 1 Only", "Bypass"
    };

    auto mirrorFilterParam = [this] (const juce::String& suffix)
    {
        if (! sceneLink)
            return;

        const int otherScene = (editingScene == 1) ? 2 : 1;
        const auto srcID = "s" + juce::String (editingScene) + "_flt_" + suffix;
        const auto dstID = "s" + juce::String (otherScene)   + "_flt_" + suffix;
        if (auto* srcP = audioProcessor.apvts.getParameter (srcID))
            if (auto* dstP = audioProcessor.apvts.getParameter (dstID))
                dstP->setValueNotifyingHost (srcP->getValue());
    };

    filterControls.typeBox.addItemList (filterTypeItems, 1);
    filterControls.typeBox.setJustificationType (juce::Justification::centredLeft);
    filterControls.typeBox.onChange = [this, mirrorFilterParam]
    {
        mirrorFilterParam ("type");
        triggerLcdNotification ("S" + juce::String (editingScene) + " // FILTER TYPE",
                                filterControls.typeBox.getText());
    };
    addChildComponent (filterControls.typeBox);

    filterControls.routingBox.addItemList (filterRoutingItems, 1);
    filterControls.routingBox.setJustificationType (juce::Justification::centredLeft);
    filterControls.routingBox.onChange = [this, mirrorFilterParam]
    {
        mirrorFilterParam ("routing");
        triggerLcdNotification ("S" + juce::String (editingScene) + " // FILTER ROUTE",
                                filterControls.routingBox.getText());
    };
    addChildComponent (filterControls.routingBox);

    auto setupFilterKnob = [this, mirrorFilterParam] (juce::Slider& slider,
                                                      const juce::String& paramSuffix,
                                                      const juce::String& paramName,
                                                      juce::Colour accent,
                                                      bool bipolar,
                                                      std::function<juce::String (double)> formatVal)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.getProperties().set ("isBipolar", bipolar);

        slider.onValueChange = [this, &slider, mirrorFilterParam, paramSuffix, paramName, formatVal]
        {
            mirrorFilterParam (paramSuffix);
            const auto srcID = "s" + juce::String (editingScene) + "_flt_" + paramSuffix;
            const auto title = "S" + juce::String (editingScene) + " // FLT " + paramName;
            setKeypadTarget (srcID, title);
            triggerLcdNotification (title, formatVal (slider.getValue()));
        };

        addChildComponent (slider);
    };

    setupFilterKnob (filterControls.cutoffKnob, "cutoff", "CUTOFF", cyanAccent, false,
        [] (double hz) -> juce::String {
            if (hz < 1000.0)
                return juce::String::formatted ("%d HZ", juce::roundToInt (hz));
            return juce::String::formatted ("%.2f KHZ", hz / 1000.0);
        });

    setupFilterKnob (filterControls.resonanceKnob, "resonance", "RESONANCE", cyanAccent, false,
        [] (double v) { return juce::String::formatted ("%d%%", juce::roundToInt (v * 100.0)); });

    setupFilterKnob (filterControls.driveKnob, "drive", "DRIVE", magentaAccent, false,
        [] (double v) { return juce::String::formatted ("%d%%", juce::roundToInt (v * 100.0)); });

    setupFilterKnob (filterControls.envAmtKnob, "env_amt", "ENV AMT", magentaAccent, true,
        [] (double v) { return juce::String::formatted ("%+d%%", juce::roundToInt (v * 100.0)); });

    setupFilterKnob (filterControls.keytrackKnob, "keytrack", "KEYTRACK", magentaAccent, false,
        [] (double v) { return juce::String::formatted ("%d%%", juce::roundToInt (v * 100.0)); });

    setupFilterKnob (filterControls.velSensKnob, "vel_sens", "VEL SENS", magentaAccent, false,
        [] (double v) { return juce::String::formatted ("%d%%", juce::roundToInt (v * 100.0)); });

    // --- Tab 3: Envelopes Controls ---
    auto formatEnvTime = [] (double sec) -> juce::String
    {
        if (sec < 1.0)
            return juce::String::formatted ("%d MS", juce::roundToInt (sec * 1000.0));
        return juce::String::formatted ("%.2f S", sec);
    };

    for (size_t i = 0; i < envControls.size(); ++i)
    {
        auto& env = envControls[i];
        const int envNum    = static_cast<int> (i + 1);
        const auto envTitle = (i == 0) ? "AMP EG" : "MOD EG";
        const auto accent   = (i == 0) ? cyanAccent : magentaAccent;

        auto setupEnvKnob = [this, accent, envNum, envTitle] (juce::Slider& slider,
                                                              const juce::String& paramSuffix,
                                                              const juce::String& paramName,
                                                              std::function<juce::String (double)> formatVal)
        {
            slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
            slider.getProperties().set ("isBipolar", false);

            slider.onValueChange = [this, &slider, envNum, envTitle, paramSuffix, paramName, formatVal]
            {
                const auto srcID = "s" + juce::String (editingScene) + "_env" + juce::String (envNum) + "_" + paramSuffix;
                if (sceneLink)
                {
                    const int otherScene = (editingScene == 1) ? 2 : 1;
                    const auto dstID = "s" + juce::String (otherScene) + "_env" + juce::String (envNum) + "_" + paramSuffix;
                    if (auto* srcP = audioProcessor.apvts.getParameter (srcID))
                        if (auto* dstP = audioProcessor.apvts.getParameter (dstID))
                            dstP->setValueNotifyingHost (srcP->getValue());
                }

                const auto title = juce::String::formatted ("S%d // %s %s", editingScene, envTitle, paramName.toRawUTF8());
                setKeypadTarget (srcID, title);
                triggerLcdNotification (title, formatVal (slider.getValue()));
            };

            addChildComponent (slider);
        };

        setupEnvKnob (env.attackKnob,  "attack",  "ATTACK",  formatEnvTime);
        setupEnvKnob (env.decayKnob,   "decay",   "DECAY",   formatEnvTime);
        setupEnvKnob (env.sustainKnob, "sustain", "SUSTAIN", [] (double v) {
            return juce::String::formatted ("%d%%", juce::roundToInt (v * 100.0));
        });
        setupEnvKnob (env.releaseKnob, "release", "RELEASE", formatEnvTime);
    }

    // --- Tab 4: Free EG Controls ---
    const juce::StringArray fegTargetItems = {
        "Off", "Filter Cutoff", "Osc 1+2 Position", "Osc 1+2 Morph", "Osc Pitch", "Scene Morph"
    };
    const juce::StringArray fegModeItems = {
        "One-Shot", "Sustain Hold", "Loop (Fwd)", "Loop (Ping-Pong)"
    };
    const juce::StringArray fegSyncItems = {
        "Free (Sec)", "1/1 Bar", "1/2 Note", "1/4 Note", "1/8 Note", "1/16 Note"
    };

    auto mirrorFegParam = [this] (const juce::String& suffix)
    {
        if (! sceneLink)
            return;

        const int otherScene = (editingScene == 1) ? 2 : 1;
        const auto srcID = "s" + juce::String (editingScene) + "_feg_" + suffix;
        const auto dstID = "s" + juce::String (otherScene)   + "_feg_" + suffix;
        if (auto* srcP = audioProcessor.apvts.getParameter (srcID))
            if (auto* dstP = audioProcessor.apvts.getParameter (dstID))
                dstP->setValueNotifyingHost (srcP->getValue());
    };

    addChildComponent (freeEgControls.canvas);

    freeEgControls.targetBox.addItemList (fegTargetItems, 1);
    freeEgControls.targetBox.setJustificationType (juce::Justification::centredLeft);
    freeEgControls.targetBox.onChange = [this, mirrorFegParam]
    {
        mirrorFegParam ("target");
        triggerLcdNotification ("S" + juce::String (editingScene) + " // FEG TARGET",
                                freeEgControls.targetBox.getText());
    };
    addChildComponent (freeEgControls.targetBox);

    freeEgControls.modeBox.addItemList (fegModeItems, 1);
    freeEgControls.modeBox.setJustificationType (juce::Justification::centredLeft);
    freeEgControls.modeBox.onChange = [this, mirrorFegParam]
    {
        mirrorFegParam ("mode");
        triggerLcdNotification ("S" + juce::String (editingScene) + " // FEG MODE",
                                freeEgControls.modeBox.getText());
    };
    addChildComponent (freeEgControls.modeBox);

    freeEgControls.syncBox.addItemList (fegSyncItems, 1);
    freeEgControls.syncBox.setJustificationType (juce::Justification::centredLeft);
    freeEgControls.syncBox.onChange = [this, mirrorFegParam]
    {
        mirrorFegParam ("sync");
        triggerLcdNotification ("S" + juce::String (editingScene) + " // FEG SYNC",
                                freeEgControls.syncBox.getText());
    };
    addChildComponent (freeEgControls.syncBox);

    auto setupFegKnob = [this, mirrorFegParam, magentaAccent] (juce::Slider& slider,
                                                               const juce::String& paramSuffix,
                                                               const juce::String& paramName,
                                                               bool bipolar,
                                                               std::function<juce::String (double)> formatVal)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::rotarySliderFillColourId, magentaAccent);
        slider.getProperties().set ("isBipolar", bipolar);

        slider.onValueChange = [this, &slider, mirrorFegParam, paramSuffix, paramName, formatVal]
        {
            mirrorFegParam (paramSuffix);
            const auto srcID = "s" + juce::String (editingScene) + "_feg_" + paramSuffix;
            const auto title = "S" + juce::String (editingScene) + " // FEG " + paramName;
            setKeypadTarget (srcID, title);
            triggerLcdNotification (title, formatVal (slider.getValue()));
        };

        addChildComponent (slider);
    };

    setupFegKnob (freeEgControls.timeKnob, "time", "TIME", false,
        [] (double sec) -> juce::String {
            if (sec < 1.0)
                return juce::String::formatted ("%d MS", juce::roundToInt (sec * 1000.0));
            return juce::String::formatted ("%.2f S", sec);
        });

    setupFegKnob (freeEgControls.amountKnob, "amount", "AMOUNT", true,
        [] (double v) { return juce::String::formatted ("%+d%%", juce::roundToInt (v * 100.0)); });

    setupFegKnob (freeEgControls.smoothKnob, "smooth", "SMOOTH", false,
        [] (double v) { return juce::String::formatted ("%d%%", juce::roundToInt (v * 100.0)); });

    // --- Tab 5: Step Sequencer Controls ---
    const juce::StringArray seqModeItems = {
        "Off", "Pitch (Seq)", "Filter Cutoff", "Osc Position", "Scene Morph"
    };
    const juce::StringArray seqLengthItems = {
        "8 Steps", "12 Steps", "16 Steps"
    };
    const juce::StringArray seqRates = {
        "1/4 Note", "1/8 Note", "1/8 Dotted", "1/16 Note", "1/16 Triplet", "1/32 Note"
    };

    auto mirrorSeqParam = [this] (const juce::String& suffix)
    {
        if (! sceneLink)
            return;

        const int otherScene = (editingScene == 1) ? 2 : 1;
        const auto srcID = "s" + juce::String (editingScene) + "_seq_" + suffix;
        const auto dstID = "s" + juce::String (otherScene)   + "_seq_" + suffix;
        if (auto* srcP = audioProcessor.apvts.getParameter (srcID))
            if (auto* dstP = audioProcessor.apvts.getParameter (dstID))
                dstP->setValueNotifyingHost (srcP->getValue());
    };

    addChildComponent (stepSeqControls.canvas);

    stepSeqControls.modeBox.addItemList (seqModeItems, 1);
    stepSeqControls.modeBox.setJustificationType (juce::Justification::centredLeft);
    stepSeqControls.modeBox.onChange = [this, mirrorSeqParam]
    {
        mirrorSeqParam ("mode");
        stepSeqControls.canvas.repaint();
        triggerLcdNotification ("S" + juce::String (editingScene) + " // SEQ MODE",
                                stepSeqControls.modeBox.getText());
    };
    addChildComponent (stepSeqControls.modeBox);

    stepSeqControls.lengthBox.addItemList (seqLengthItems, 1);
    stepSeqControls.lengthBox.setJustificationType (juce::Justification::centredLeft);
    stepSeqControls.lengthBox.onChange = [this, mirrorSeqParam]
    {
        mirrorSeqParam ("length");
        stepSeqControls.canvas.repaint();
        triggerLcdNotification ("S" + juce::String (editingScene) + " // SEQ LENGTH",
                                stepSeqControls.lengthBox.getText());
    };
    addChildComponent (stepSeqControls.lengthBox);

    stepSeqControls.rateBox.addItemList (seqRates, 1);
    stepSeqControls.rateBox.setJustificationType (juce::Justification::centredLeft);
    stepSeqControls.rateBox.onChange = [this, mirrorSeqParam]
    {
        mirrorSeqParam ("rate");
        triggerLcdNotification ("S" + juce::String (editingScene) + " // SEQ RATE",
                                stepSeqControls.rateBox.getText());
    };
    addChildComponent (stepSeqControls.rateBox);

    auto setupSeqKnob = [this, mirrorSeqParam, magentaAccent] (juce::Slider& slider,
                                                               const juce::String& paramSuffix,
                                                               const juce::String& paramName,
                                                               std::function<juce::String (double)> formatVal)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::rotarySliderFillColourId, magentaAccent);
        slider.getProperties().set ("isBipolar", false);

        slider.onValueChange = [this, &slider, mirrorSeqParam, paramSuffix, paramName, formatVal]
        {
            mirrorSeqParam (paramSuffix);
            const auto srcID = "s" + juce::String (editingScene) + "_seq_" + paramSuffix;
            const auto title = "S" + juce::String (editingScene) + " // SEQ " + paramName;
            setKeypadTarget (srcID, title);
            triggerLcdNotification (title, formatVal (slider.getValue()));
        };

        addChildComponent (slider);
    };

    setupSeqKnob (stepSeqControls.gateKnob, "gate", "GATE",
        [] (double v) { return juce::String::formatted ("%d%%", juce::roundToInt (v * 100.0)); });

    setupSeqKnob (stepSeqControls.swingKnob, "swing", "SWING",
        [] (double v) { return juce::String::formatted ("%d%%", juce::roundToInt (v * 100.0)); });

    setupSeqKnob (stepSeqControls.amountKnob, "amount", "GLIDE / AMT",
        [] (double v) { return juce::String::formatted ("%d%%", juce::roundToInt (v * 100.0)); });

    // --- Tab 6: Mod Matrix & LFO 1 Controls ---
    const juce::StringArray modSrcItems = {
        "Off",
        "LFO 1 (Sine)", "LFO 1 (Tri)", "LFO 1 (Saw)",
        "Mod Wheel", "Velocity", "Keytrack",
        "Amp EG", "Mod EG", "Free EG", "Step Seq",
        "Assign 1", "Assign 2", "Assign 3", "Assign 4",
        "Assign 5", "Assign 6", "Assign 7", "Assign 8"
    };

    const juce::StringArray modDstItems = {
        "Off",
        "Filter Cutoff", "Filter Resonance",
        "Osc 1 Pitch", "Osc 2 Pitch", "Osc 1+2 Pitch",
        "Osc 1 Position", "Osc 2 Position", "Osc 1+2 Morph",
        "Osc 1 Level", "Osc 2 Level",
        "Scene Morph", "LFO 1 Rate", "LFO 1 Depth"
    };

    for (size_t m = 0; m < 4; ++m)
    {
        auto& slot = modMatrixControls.slots[m];
        const int slotNum = static_cast<int> (m + 1);

        auto mirrorModSlot = [this, slotNum] (const juce::String& suffix)
        {
            if (! sceneLink)
                return;

            const int otherScene = (editingScene == 1) ? 2 : 1;
            const auto srcID = "s" + juce::String (editingScene) + "_mod" + juce::String (slotNum) + "_" + suffix;
            const auto dstID = "s" + juce::String (otherScene)   + "_mod" + juce::String (slotNum) + "_" + suffix;
            if (auto* srcP = audioProcessor.apvts.getParameter (srcID))
                if (auto* dstP = audioProcessor.apvts.getParameter (dstID))
                    dstP->setValueNotifyingHost (srcP->getValue());
        };

        slot.srcBox.addItemList (modSrcItems, 1);
        slot.srcBox.setJustificationType (juce::Justification::centredLeft);
        slot.srcBox.onChange = [this, &slot, slotNum, mirrorModSlot]
        {
            mirrorModSlot ("src");
            triggerLcdNotification (juce::String::formatted ("S%d // MOD %d SRC", editingScene, slotNum),
                                    slot.srcBox.getText());
        };
        addChildComponent (slot.srcBox);

        slot.dstBox.addItemList (modDstItems, 1);
        slot.dstBox.setJustificationType (juce::Justification::centredLeft);
        slot.dstBox.onChange = [this, &slot, slotNum, mirrorModSlot]
        {
            mirrorModSlot ("dst");
            triggerLcdNotification (juce::String::formatted ("S%d // MOD %d DST", editingScene, slotNum),
                                    slot.dstBox.getText());
        };
        addChildComponent (slot.dstBox);

        slot.amtKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slot.amtKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slot.amtKnob.setColour (juce::Slider::rotarySliderFillColourId, cyanAccent);
        slot.amtKnob.getProperties().set ("isBipolar", true);
        slot.amtKnob.onValueChange = [this, &slot, slotNum, mirrorModSlot]
        {
            mirrorModSlot ("amt");
            const auto srcID = "s" + juce::String (editingScene) + "_mod" + juce::String (slotNum) + "_amt";
            const auto title = juce::String::formatted ("S%d // MOD %d AMT", editingScene, slotNum);
            setKeypadTarget (srcID, title);
            const int pct = juce::roundToInt (slot.amtKnob.getValue() * 100.0);
            triggerLcdNotification (title, juce::String::formatted ("%+d%%", pct));
        };
        addChildComponent (slot.amtKnob);
    }

    const juce::StringArray lfoSyncItems = {
        "Free (Hz)", "1/1 Bar", "1/2 Note", "1/4 Note", "1/8 Note", "1/16 Note"
    };

    auto mirrorLfoParam = [this] (const juce::String& suffix)
    {
        if (! sceneLink)
            return;

        const int otherScene = (editingScene == 1) ? 2 : 1;
        const auto srcID = "s" + juce::String (editingScene) + "_lfo1_" + suffix;
        const auto dstID = "s" + juce::String (otherScene)   + "_lfo1_" + suffix;
        if (auto* srcP = audioProcessor.apvts.getParameter (srcID))
            if (auto* dstP = audioProcessor.apvts.getParameter (dstID))
                dstP->setValueNotifyingHost (srcP->getValue());
    };

    modMatrixControls.lfoSyncBox.addItemList (lfoSyncItems, 1);
    modMatrixControls.lfoSyncBox.setJustificationType (juce::Justification::centredLeft);
    modMatrixControls.lfoSyncBox.onChange = [this, mirrorLfoParam]
    {
        mirrorLfoParam ("sync");
        triggerLcdNotification ("S" + juce::String (editingScene) + " // LFO 1 SYNC",
                                modMatrixControls.lfoSyncBox.getText());
    };
    addChildComponent (modMatrixControls.lfoSyncBox);

    auto setupLfoKnob = [this, mirrorLfoParam, magentaAccent] (juce::Slider& slider,
                                                               const juce::String& paramSuffix,
                                                               const juce::String& paramName,
                                                               std::function<juce::String (double)> formatVal)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::rotarySliderFillColourId, magentaAccent);
        slider.getProperties().set ("isBipolar", false);

        slider.onValueChange = [this, &slider, mirrorLfoParam, paramSuffix, paramName, formatVal]
        {
            mirrorLfoParam (paramSuffix);
            const auto srcID = "s" + juce::String (editingScene) + "_lfo1_" + paramSuffix;
            const auto title = "S" + juce::String (editingScene) + " // LFO " + paramName;
            setKeypadTarget (srcID, title);
            triggerLcdNotification (title, formatVal (slider.getValue()));
        };

        addChildComponent (slider);
    };

    setupLfoKnob (modMatrixControls.lfoRateKnob, "rate", "RATE",
        [] (double hz) { return juce::String::formatted ("%.2f HZ", hz); });

    setupLfoKnob (modMatrixControls.lfoFadeKnob, "fade", "FADE IN",
        [] (double sec) -> juce::String {
            if (sec < 1.0)
                return juce::String::formatted ("%d MS", juce::roundToInt (sec * 1000.0));
            return juce::String::formatted ("%.2f S", sec);
        });

    setupLfoKnob (modMatrixControls.lfoDepthKnob, "depth", "DEPTH",
        [] (double v) { return juce::String::formatted ("%d%%", juce::roundToInt (v * 100.0)); });

    bindOscillatorAttachments();
    bindFilterAttachments();
    bindEnvelopeAttachments();
    bindFreeEgAttachments();
    bindStepSeqAttachments();
    bindModMatrixAttachments();
    updateTabVisibility();

    startTimerHz (30);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (16.0 / 9.0);
    setResizeLimits (800, 450, 2200, 1238);

   #if JUCE_LINUX
    setSize (1400, 788);
   #else
    setSize (1120, 630);
   #endif
}

NeomorphX1AudioProcessorEditor::~NeomorphX1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ==============================================================================
// 10er-Nummernblock Logik (Direkte Parameter-Eingabe)
// ==============================================================================
void NeomorphX1AudioProcessorEditor::setKeypadTarget (const juce::String& paramID, const juce::String& displayName)
{
    if (keypadTargetParamID != paramID)
    {
        keypadBuffer.clear();
        keypadNegative = false;
        keypadActive   = false;
    }
    keypadTargetParamID = paramID;
    keypadTargetLabel   = displayName;
}

void NeomorphX1AudioProcessorEditor::handleKeypadDigit (int digit)
{
    if (keypadBuffer.length() >= 6)
        return;

    if (keypadBuffer == "0")
        keypadBuffer = juce::String (digit);
    else
        keypadBuffer += juce::String (digit);

    keypadActive = true;
    const auto signStr = keypadNegative ? "-" : "";
    triggerLcdNotification ("KEYPAD // " + keypadTargetLabel, signStr + keypadBuffer + "_");
}

void NeomorphX1AudioProcessorEditor::handleKeypadClear()
{
    // Wenn Puffer leer ist, schaltet CLR das Vorzeichen (+/-) um – praktisch für bipolare Parameter
    if (keypadBuffer.isEmpty())
    {
        handleKeypadToggleSign();
        return;
    }

    keypadBuffer.clear();
    keypadNegative = false;
    keypadActive   = false;
    triggerLcdNotification ("KEYPAD // " + keypadTargetLabel, "CLEARED");
}

void NeomorphX1AudioProcessorEditor::handleKeypadToggleSign()
{
    keypadNegative = ! keypadNegative;
    keypadActive   = true;
    const auto signStr = keypadNegative ? "-" : "+";
    triggerLcdNotification ("KEYPAD // " + keypadTargetLabel, signStr + keypadBuffer + "_");
}

void NeomorphX1AudioProcessorEditor::handleKeypadEnter()
{
    if (keypadBuffer.isEmpty())
        return;

    auto* param = audioProcessor.apvts.getParameter (keypadTargetParamID);
    if (param == nullptr)
    {
        keypadBuffer.clear();
        keypadActive = false;
        return;
    }

    const double rawVal = keypadBuffer.getDoubleValue() * (keypadNegative ? -1.0 : 1.0);
    float realValue = 0.0f;

    if (keypadTargetParamID.endsWith ("_cutoff"))
    {
        // > 20 direkt als Hz (z.B. 1200 -> 1200 Hz), <= 20 als kHz (z.B. 4 -> 4000 Hz)
        const double hz = (std::abs (rawVal) <= 20.0) ? (std::abs (rawVal) * 1000.0) : std::abs (rawVal);
        realValue = juce::jlimit (20.0f, 20000.0f, static_cast<float> (hz));
    }
    else if (keypadTargetParamID.endsWith ("_attack")
          || keypadTargetParamID.endsWith ("_decay")
          || keypadTargetParamID.endsWith ("_release")
          || keypadTargetParamID.endsWith ("_time")
          || keypadTargetParamID.endsWith ("_fade"))
    {
        // <= 16 als Sekunden, > 16 als Millisekunden (z.B. 250 -> 0.250 s)
        const double sec = (std::abs (rawVal) > 16.0) ? (std::abs (rawVal) / 1000.0) : std::abs (rawVal);
        realValue = static_cast<float> (sec);
    }
    else if (keypadTargetParamID.endsWith ("_octave"))
    {
        realValue = juce::jlimit (-2.0f, 2.0f, static_cast<float> (rawVal));
    }
    else if (keypadTargetParamID.endsWith ("_semi"))
    {
        realValue = juce::jlimit (-12.0f, 12.0f, static_cast<float> (rawVal));
    }
    else if (keypadTargetParamID.endsWith ("_detune"))
    {
        realValue = juce::jlimit (-50.0f, 50.0f, static_cast<float> (rawVal));
    }
    else if (keypadTargetParamID.endsWith ("_phase"))
    {
        realValue = juce::jlimit (0.0f, 360.0f, static_cast<float> (std::abs (rawVal)));
    }
    else if (keypadTargetParamID.endsWith ("_rate"))
    {
        realValue = juce::jlimit (0.05f, 20.0f, static_cast<float> (std::abs (rawVal)));
    }
    else if (keypadTargetParamID == "knob_2")
    {
        // Master Pan: -100..+100 -> 0.0..1.0
        const float panNorm = juce::jlimit (-100.0f, 100.0f, static_cast<float> (rawVal)) / 200.0f + 0.5f;
        realValue = panNorm;
    }
    else if (keypadTargetParamID.endsWith ("_pan")
          || keypadTargetParamID.endsWith ("_env_amt")
          || keypadTargetParamID.endsWith ("_amt")
          || keypadTargetParamID.endsWith ("_amount"))
    {
        // Bipolare Prozentwerte (-100..+100 -> -1.0..+1.0)
        realValue = juce::jlimit (-100.0f, 100.0f, static_cast<float> (rawVal)) / 100.0f;
    }
    else
    {
        // Standard Unipolar Prozent (0..100 -> 0.0..1.0)
        realValue = juce::jlimit (0.0f, 100.0f, static_cast<float> (std::abs (rawVal))) / 100.0f;
    }

    const float norm = juce::jlimit (0.0f, 1.0f, param->convertTo0to1 (realValue));
    param->setValueNotifyingHost (norm);

    keypadBuffer.clear();
    keypadNegative = false;
    keypadActive   = false;
}

bool NeomorphX1AudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    const auto ch = key.getTextCharacter();
    if (ch >= '0' && ch <= '9')
    {
        handleKeypadDigit (ch - '0');
        return true;
    }
    if (ch == '-')
    {
        handleKeypadToggleSign();
        return true;
    }
    if (key == juce::KeyPress::returnKey)
    {
        handleKeypadEnter();
        return true;
    }
    if (key == juce::KeyPress::backspaceKey || key == juce::KeyPress::escapeKey)
    {
        handleKeypadClear();
        return true;
    }
    return false;
}

// ==============================================================================
// Preset-Verwaltung UI-Logik
// ==============================================================================
void NeomorphX1AudioProcessorEditor::refreshPresetList()
{
    suppressPresetBoxCallback = true;

    presetBox.clear (juce::dontSendNotification);
    const auto presets = audioProcessor.getAvailablePresets();
    const auto current = audioProcessor.getCurrentPresetName();

    int matchId = 1;
    for (int i = 0; i < presets.size(); ++i)
    {
        presetBox.addItem (presets[i], i + 1);
        if (presets[i].equalsIgnoreCase (current))
            matchId = i + 1;
    }

    presetBox.setSelectedId (matchId, juce::dontSendNotification);
    if (presetBox.getText().isEmpty())
        presetBox.setText (current, juce::dontSendNotification);

    suppressPresetBoxCallback = false;
}

void NeomorphX1AudioProcessorEditor::handleSavePresetClick (bool forceSaveAsDialog)
{
    if (! forceSaveAsDialog && audioProcessor.isCurrentPresetSaved())
    {
        const auto name = audioProcessor.getCurrentPresetName();
        if (audioProcessor.savePreset (name))
            triggerLcdNotification ("PRESET SAVED", name);
        return;
    }

    auto* alert = new juce::AlertWindow ("NEOMORPH X1 // SAVE PRESET",
                                         "Enter a name for the new preset:",
                                         juce::MessageBoxIconType::NoIcon);

    const auto suggestedName = audioProcessor.isCurrentPresetSaved()
                                   ? (audioProcessor.getCurrentPresetName() + " Copy")
                                   : "Neomorph Patch 01";

    alert->addTextEditor ("presetName", suggestedName, "Preset Name:");
    alert->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    alert->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, alert] (int result)
        {
            if (result == 1)
            {
                const auto newName = alert->getTextEditorContents ("presetName").trim();
                if (newName.isNotEmpty() && ! newName.equalsIgnoreCase ("Init"))
                {
                    if (audioProcessor.savePreset (newName))
                    {
                        refreshPresetList();
                        triggerLcdNotification ("PRESET SAVED", newName);
                    }
                }
                else if (newName.equalsIgnoreCase ("Init"))
                {
                    triggerLcdNotification ("SAVE BLOCKED", "INIT IS PROTECTED");
                }
            }
        }), true);
}

void NeomorphX1AudioProcessorEditor::handleLoadPresetClick()
{
    activeFileChooser = std::make_unique<juce::FileChooser> (
        "Load Neomorph X1 Preset (.xml)",
        audioProcessor.getPresetDirectory(),
        "*.xml");

    constexpr int flags = juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectFiles;

    activeFileChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (! file.existsAsFile())
            return;

        const bool isOutsideLibrary = (file.getParentDirectory() != audioProcessor.getPresetDirectory());

        if (audioProcessor.loadPresetFromFile (file, isOutsideLibrary))
        {
            refreshPresetList();
            freeEgControls.canvas.repaint();
            stepSeqControls.canvas.repaint();
            triggerLcdNotification (isOutsideLibrary ? "PRESET IMPORTED" : "PRESET LOADED",
                                    audioProcessor.getCurrentPresetName());
        }
        else
        {
            triggerLcdNotification ("LOAD ERROR", "INVALID XML FILE");
        }
    });
}

void NeomorphX1AudioProcessorEditor::handlePanicClick()
{
    audioProcessor.panicReset();
    smoothedLevelL = smoothedLevelR = 0.0f;
    peakHoldL = peakHoldR = 0.0f;
    stepSeqControls.canvas.repaint();
    triggerLcdNotification ("PANIC // RESET", "ALL VOICES CLEARED");
}

void NeomorphX1AudioProcessorEditor::stepPreset (int delta)
{
    const int numItems = presetBox.getNumItems();
    if (numItems <= 0)
        return;

    int idx = presetBox.getSelectedItemIndex();
    if (idx < 0)
        idx = 0;

    idx = (idx + delta + numItems) % numItems;
    presetBox.setSelectedItemIndex (idx, juce::sendNotificationSync);
}

void NeomorphX1AudioProcessorEditor::timerCallback()
{
    if (presetBox.getText() != audioProcessor.getCurrentPresetName())
        refreshPresetList();

    if (showLcdNotify && lcdNotifyTicks > 0 && ! keypadActive)
    {
        if (--lcdNotifyTicks == 0)
            showLcdNotify = false;
    }

    if (currentTab == ActiveTab::StepSequencer)
    {
        const int curStep = audioProcessor.getCurrentSeqStep (editingScene);
        if (curStep != lastPaintedSeqStep)
        {
            lastPaintedSeqStep = curStep;
            stepSeqControls.canvas.repaint();
        }
    }
    else if (currentTab == ActiveTab::ModMatrix)
    {
        lastPaintedLfo = audioProcessor.getLiveLfoValue (editingScene);
    }

    // Telemetrie für unteres Status-Strip-Panel aktualisieren
    const float rawPeakL = audioProcessor.consumePeakLeft();
    const float rawPeakR = audioProcessor.consumePeakRight();

    smoothedLevelL = (rawPeakL > smoothedLevelL) ? rawPeakL : (smoothedLevelL * 0.84f);
    smoothedLevelR = (rawPeakR > smoothedLevelR) ? rawPeakR : (smoothedLevelR * 0.84f);

    if (rawPeakL >= peakHoldL)
    {
        peakHoldL      = rawPeakL;
        peakHoldTicksL = 36;
    }
    else if (peakHoldTicksL > 0)
    {
        --peakHoldTicksL;
    }
    else
    {
        peakHoldL *= 0.90f;
    }

    if (rawPeakR >= peakHoldR)
    {
        peakHoldR      = rawPeakR;
        peakHoldTicksR = 36;
    }
    else if (peakHoldTicksR > 0)
    {
        --peakHoldTicksR;
    }
    else
    {
        peakHoldR *= 0.90f;
    }

    if (audioProcessor.consumeMidiActivity())
        midiLedHoldTicks = 6;
    else if (midiLedHoldTicks > 0)
        --midiLedHoldTicks;

    audioProcessor.copyScopeSamples (scopeCache);

    if (! bottomStripBounds.isEmpty())
        repaint (bottomStripBounds.toNearestInt().expanded (2));
}

void NeomorphX1AudioProcessorEditor::triggerLcdNotification (const juce::String& title, const juce::String& value)
{
    lcdNotifyTitle = title;
    lcdNotifyValue = value;
    showLcdNotify  = true;
    lcdNotifyTicks = 48;
    repaint();
}

void NeomorphX1AudioProcessorEditor::setActiveTab (ActiveTab newTab)
{
    if (currentTab != newTab)
    {
        currentTab = newTab;
        updateTabVisibility();
        repaint();
    }
}

void NeomorphX1AudioProcessorEditor::setEditingScene (int newScene)
{
    if (editingScene == newScene)
    {
        showLcdNotify = false;
        repaint();
        return;
    }

    editingScene = newScene;
    bindOscillatorAttachments();
    bindFilterAttachments();
    bindEnvelopeAttachments();
    bindFreeEgAttachments();
    bindStepSeqAttachments();
    bindModMatrixAttachments();
    freeEgControls.canvas.repaint();
    stepSeqControls.canvas.repaint();

    showLcdNotify = false;
    repaint();
}

void NeomorphX1AudioProcessorEditor::handleSceneButton (int buttonIndex)
{
    switch (buttonIndex)
    {
        case 0: setEditingScene (1); break;
        case 1: setEditingScene (2); break;

        case 2:
            sceneSolo = ! sceneSolo;
            triggerLcdNotification ("SCENE SOLO", sceneSolo ? "ACTIVE (S" + juce::String (editingScene) + ")" : "OFF");
            break;

        case 3:
            sceneLink = ! sceneLink;
            triggerLcdNotification ("SCENE LINK EDIT", sceneLink ? "ENABLED (S1+S2)" : "OFF");
            break;

        case 4:
        {
            const int targetScene = (editingScene == 1) ? 2 : 1;
            audioProcessor.copySceneParameters (editingScene, targetScene);
            freeEgControls.canvas.repaint();
            stepSeqControls.canvas.repaint();
            triggerLcdNotification ("SCENE COPY",
                                    juce::String::formatted ("S%d -> S%d DONE", editingScene, targetScene));
            break;
        }

        case 5:
            audioProcessor.swapSceneParameters();
            freeEgControls.canvas.repaint();
            stepSeqControls.canvas.repaint();
            triggerLcdNotification ("SCENE SWAP", "S1 <-> S2 SWAPPED");
            break;

        case 6:
            audioProcessor.resetSceneParameters (editingScene);
            freeEgControls.canvas.repaint();
            stepSeqControls.canvas.repaint();
            triggerLcdNotification ("SCENE INIT", "SCENE " + juce::String (editingScene) + " RESET");
            break;

        case 7:
            audioProcessor.randomizeSceneParameters (editingScene);
            freeEgControls.canvas.repaint();
            stepSeqControls.canvas.repaint();
            triggerLcdNotification ("SCENE RANDOM", "SCENE " + juce::String (editingScene) + " MORPHED");
            break;

        default:
            break;
    }
}

void NeomorphX1AudioProcessorEditor::bindOscillatorAttachments()
{
    const auto scenePrefix = "s" + juce::String (editingScene) + "_";

    for (size_t i = 0; i < oscControls.size(); ++i)
    {
        auto& osc = oscControls[i];
        const auto prefix = scenePrefix + "osc" + juce::String (static_cast<int> (i + 1)) + "_";

        osc.waveAttachment.reset();
        osc.octaveAttachment.reset();
        osc.semiAttachment.reset();
        osc.detuneAttachment.reset();
        osc.positionAttachment.reset();
        osc.morphAttachment.reset();
        osc.phaseAttachment.reset();
        osc.panAttachment.reset();
        osc.levelAttachment.reset();

        auto bindSlider = [this] (juce::Slider& slider,
                                  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
                                  const juce::String& paramID)
        {
            if (auto* param = audioProcessor.apvts.getParameter (paramID))
                slider.setDoubleClickReturnValue (true, param->getDefaultValue());

            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                audioProcessor.apvts, paramID, slider);
        };

        osc.waveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            audioProcessor.apvts, prefix + "wave", osc.waveBox);

        bindSlider (osc.octaveKnob,   osc.octaveAttachment,   prefix + "octave");
        bindSlider (osc.semiKnob,     osc.semiAttachment,     prefix + "semi");
        bindSlider (osc.detuneKnob,   osc.detuneAttachment,   prefix + "detune");
        bindSlider (osc.positionKnob, osc.positionAttachment, prefix + "position");
        bindSlider (osc.morphKnob,    osc.morphAttachment,    prefix + "morph");
        bindSlider (osc.phaseKnob,    osc.phaseAttachment,    prefix + "phase");
        bindSlider (osc.panKnob,      osc.panAttachment,      prefix + "pan");
        bindSlider (osc.levelKnob,    osc.levelAttachment,    prefix + "level");
    }
}

void NeomorphX1AudioProcessorEditor::bindFilterAttachments()
{
    const auto prefix = "s" + juce::String (editingScene) + "_flt_";

    filterControls.typeAttachment.reset();
    filterControls.routingAttachment.reset();
    filterControls.cutoffAttachment.reset();
    filterControls.resonanceAttachment.reset();
    filterControls.driveAttachment.reset();
    filterControls.envAmtAttachment.reset();
    filterControls.keytrackAttachment.reset();
    filterControls.velSensAttachment.reset();

    auto bindSlider = [this] (juce::Slider& slider,
                              std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
                              const juce::String& paramID)
    {
        if (auto* param = audioProcessor.apvts.getParameter (paramID))
            slider.setDoubleClickReturnValue (true, param->getDefaultValue());

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, paramID, slider);
    };

    filterControls.typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, prefix + "type", filterControls.typeBox);

    filterControls.routingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, prefix + "routing", filterControls.routingBox);

    bindSlider (filterControls.cutoffKnob,    filterControls.cutoffAttachment,    prefix + "cutoff");
    bindSlider (filterControls.resonanceKnob, filterControls.resonanceAttachment, prefix + "resonance");
    bindSlider (filterControls.driveKnob,     filterControls.driveAttachment,     prefix + "drive");
    bindSlider (filterControls.envAmtKnob,    filterControls.envAmtAttachment,    prefix + "env_amt");
    bindSlider (filterControls.keytrackKnob,  filterControls.keytrackAttachment,  prefix + "keytrack");
    bindSlider (filterControls.velSensKnob,   filterControls.velSensAttachment,   prefix + "vel_sens");
}

void NeomorphX1AudioProcessorEditor::bindEnvelopeAttachments()
{
    const auto scenePrefix = "s" + juce::String (editingScene) + "_";

    for (size_t i = 0; i < envControls.size(); ++i)
    {
        auto& env = envControls[i];
        const auto prefix = scenePrefix + "env" + juce::String (static_cast<int> (i + 1)) + "_";

        env.attackAttachment.reset();
        env.decayAttachment.reset();
        env.sustainAttachment.reset();
        env.releaseAttachment.reset();

        auto bindSlider = [this] (juce::Slider& slider,
                                  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
                                  const juce::String& paramID)
        {
            if (auto* param = audioProcessor.apvts.getParameter (paramID))
                slider.setDoubleClickReturnValue (true, param->getDefaultValue());

            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                audioProcessor.apvts, paramID, slider);
        };

        bindSlider (env.attackKnob,  env.attackAttachment,  prefix + "attack");
        bindSlider (env.decayKnob,   env.decayAttachment,   prefix + "decay");
        bindSlider (env.sustainKnob, env.sustainAttachment, prefix + "sustain");
        bindSlider (env.releaseKnob, env.releaseAttachment, prefix + "release");
    }
}

void NeomorphX1AudioProcessorEditor::bindFreeEgAttachments()
{
    const auto prefix = "s" + juce::String (editingScene) + "_feg_";

    freeEgControls.targetAttachment.reset();
    freeEgControls.modeAttachment.reset();
    freeEgControls.syncAttachment.reset();
    freeEgControls.timeAttachment.reset();
    freeEgControls.amountAttachment.reset();
    freeEgControls.smoothAttachment.reset();

    auto bindSlider = [this] (juce::Slider& slider,
                              std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
                              const juce::String& paramID)
    {
        if (auto* param = audioProcessor.apvts.getParameter (paramID))
            slider.setDoubleClickReturnValue (true, param->getDefaultValue());

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, paramID, slider);
    };

    freeEgControls.targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, prefix + "target", freeEgControls.targetBox);

    freeEgControls.modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, prefix + "mode", freeEgControls.modeBox);

    freeEgControls.syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, prefix + "sync", freeEgControls.syncBox);

    bindSlider (freeEgControls.timeKnob,   freeEgControls.timeAttachment,   prefix + "time");
    bindSlider (freeEgControls.amountKnob, freeEgControls.amountAttachment, prefix + "amount");
    bindSlider (freeEgControls.smoothKnob, freeEgControls.smoothAttachment, prefix + "smooth");
}

void NeomorphX1AudioProcessorEditor::bindStepSeqAttachments()
{
    const auto prefix = "s" + juce::String (editingScene) + "_seq_";

    stepSeqControls.modeAttachment.reset();
    stepSeqControls.lengthAttachment.reset();
    stepSeqControls.rateAttachment.reset();
    stepSeqControls.gateAttachment.reset();
    stepSeqControls.swingAttachment.reset();
    stepSeqControls.amountAttachment.reset();

    auto bindSlider = [this] (juce::Slider& slider,
                              std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
                              const juce::String& paramID)
    {
        if (auto* param = audioProcessor.apvts.getParameter (paramID))
            slider.setDoubleClickReturnValue (true, param->getDefaultValue());

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, paramID, slider);
    };

    stepSeqControls.modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, prefix + "mode", stepSeqControls.modeBox);

    stepSeqControls.lengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, prefix + "length", stepSeqControls.lengthBox);

    stepSeqControls.rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, prefix + "rate", stepSeqControls.rateBox);

    bindSlider (stepSeqControls.gateKnob,   stepSeqControls.gateAttachment,   prefix + "gate");
    bindSlider (stepSeqControls.swingKnob,  stepSeqControls.swingAttachment,  prefix + "swing");
    bindSlider (stepSeqControls.amountKnob, stepSeqControls.amountAttachment, prefix + "amount");
}

void NeomorphX1AudioProcessorEditor::bindModMatrixAttachments()
{
    const auto scenePrefix = "s" + juce::String (editingScene) + "_";

    for (size_t m = 0; m < 4; ++m)
    {
        auto& slot = modMatrixControls.slots[m];
        const auto mp = scenePrefix + "mod" + juce::String (static_cast<int> (m + 1)) + "_";

        slot.srcAttachment.reset();
        slot.dstAttachment.reset();
        slot.amtAttachment.reset();

        slot.srcAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            audioProcessor.apvts, mp + "src", slot.srcBox);

        slot.dstAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            audioProcessor.apvts, mp + "dst", slot.dstBox);

        if (auto* param = audioProcessor.apvts.getParameter (mp + "amt"))
            slot.amtKnob.setDoubleClickReturnValue (true, param->getDefaultValue());

        slot.amtAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, mp + "amt", slot.amtKnob);
    }

    const auto lfop = scenePrefix + "lfo1_";
    modMatrixControls.lfoSyncAttachment.reset();
    modMatrixControls.lfoRateAttachment.reset();
    modMatrixControls.lfoFadeAttachment.reset();
    modMatrixControls.lfoDepthAttachment.reset();

    modMatrixControls.lfoSyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, lfop + "sync", modMatrixControls.lfoSyncBox);

    auto bindLfoSlider = [this] (juce::Slider& slider,
                                 std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
                                 const juce::String& paramID)
    {
        if (auto* param = audioProcessor.apvts.getParameter (paramID))
            slider.setDoubleClickReturnValue (true, param->getDefaultValue());

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, paramID, slider);
    };

    bindLfoSlider (modMatrixControls.lfoRateKnob,  modMatrixControls.lfoRateAttachment,  lfop + "rate");
    bindLfoSlider (modMatrixControls.lfoFadeKnob,  modMatrixControls.lfoFadeAttachment,  lfop + "fade");
    bindLfoSlider (modMatrixControls.lfoDepthKnob, modMatrixControls.lfoDepthAttachment, lfop + "depth");
}

void NeomorphX1AudioProcessorEditor::updateTabVisibility()
{
    const bool showOsc = (currentTab == ActiveTab::Oscillators);
    for (auto& osc : oscControls)
    {
        osc.waveBox.setVisible (showOsc);
        osc.octaveKnob.setVisible (showOsc);
        osc.semiKnob.setVisible (showOsc);
        osc.detuneKnob.setVisible (showOsc);
        osc.positionKnob.setVisible (showOsc);
        osc.morphKnob.setVisible (showOsc);
        osc.phaseKnob.setVisible (showOsc);
        osc.panKnob.setVisible (showOsc);
        osc.levelKnob.setVisible (showOsc);
    }

    const bool showFilter = (currentTab == ActiveTab::Filter);
    filterControls.typeBox.setVisible (showFilter);
    filterControls.routingBox.setVisible (showFilter);
    filterControls.cutoffKnob.setVisible (showFilter);
    filterControls.resonanceKnob.setVisible (showFilter);
    filterControls.driveKnob.setVisible (showFilter);
    filterControls.envAmtKnob.setVisible (showFilter);
    filterControls.keytrackKnob.setVisible (showFilter);
    filterControls.velSensKnob.setVisible (showFilter);

    const bool showEnv = (currentTab == ActiveTab::Envelopes);
    for (auto& env : envControls)
    {
        env.attackKnob.setVisible (showEnv);
        env.decayKnob.setVisible (showEnv);
        env.sustainKnob.setVisible (showEnv);
        env.releaseKnob.setVisible (showEnv);
    }

    const bool showFeg = (currentTab == ActiveTab::FreeEG);
    freeEgControls.canvas.setVisible (showFeg);
    freeEgControls.targetBox.setVisible (showFeg);
    freeEgControls.modeBox.setVisible (showFeg);
    freeEgControls.syncBox.setVisible (showFeg);
    freeEgControls.timeKnob.setVisible (showFeg);
    freeEgControls.amountKnob.setVisible (showFeg);
    freeEgControls.smoothKnob.setVisible (showFeg);

    const bool showSeq = (currentTab == ActiveTab::StepSequencer);
    stepSeqControls.canvas.setVisible (showSeq);
    stepSeqControls.modeBox.setVisible (showSeq);
    stepSeqControls.lengthBox.setVisible (showSeq);
    stepSeqControls.rateBox.setVisible (showSeq);
    stepSeqControls.gateKnob.setVisible (showSeq);
    stepSeqControls.swingKnob.setVisible (showSeq);
    stepSeqControls.amountKnob.setVisible (showSeq);

    const bool showMod = (currentTab == ActiveTab::ModMatrix);
    for (auto& slot : modMatrixControls.slots)
    {
        slot.srcBox.setVisible (showMod);
        slot.dstBox.setVisible (showMod);
        slot.amtKnob.setVisible (showMod);
    }
    modMatrixControls.lfoSyncBox.setVisible (showMod);
    modMatrixControls.lfoRateKnob.setVisible (showMod);
    modMatrixControls.lfoFadeKnob.setVisible (showMod);
    modMatrixControls.lfoDepthKnob.setVisible (showMod);
}

void NeomorphX1AudioProcessorEditor::showSettingsMenu()
{
    juce::PopupMenu menu;

    menu.addSectionHeader ("NEOMORPH X1 // SYSTEM");
    menu.addItem (1, "Init Full Patch (Reset)");
    menu.addItem (5, "Open Presets Folder...");
    menu.addSeparator();
    menu.addItem (2, "Copy Scene 1 -> Scene 2");
    menu.addItem (3, "Copy Scene 2 -> Scene 1");
    menu.addItem (4, "Swap Scene 1 <-> Scene 2");
    menu.addSeparator();

    juce::PopupMenu scaleMenu;
    scaleMenu.addItem (10, "80%  (896 x 504)");
    scaleMenu.addItem (11, "100% (1120 x 630)");
    scaleMenu.addItem (12, "125% (1400 x 788)");
    scaleMenu.addItem (13, "150% (1680 x 945)");
    menu.addSubMenu ("UI Scale", scaleMenu);

    if (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        menu.addSeparator();
        menu.addItem (20, "Standalone Options...");
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton),
                        [this] (int result)
    {
        switch (result)
        {
            case 1:
                audioProcessor.loadInitPatch();
                refreshPresetList();
                freeEgControls.canvas.repaint();
                stepSeqControls.canvas.repaint();
                triggerLcdNotification ("SYSTEM", "INIT PATCH LOADED");
                break;
            case 2:
                audioProcessor.copySceneParameters (1, 2);
                freeEgControls.canvas.repaint();
                stepSeqControls.canvas.repaint();
                triggerLcdNotification ("SCENE COPY", "S1 -> S2 DONE");
                break;
            case 3:
                audioProcessor.copySceneParameters (2, 1);
                freeEgControls.canvas.repaint();
                stepSeqControls.canvas.repaint();
                triggerLcdNotification ("SCENE COPY", "S2 -> S1 DONE");
                break;
            case 4:
                audioProcessor.swapSceneParameters();
                freeEgControls.canvas.repaint();
                stepSeqControls.canvas.repaint();
                triggerLcdNotification ("SCENE SWAP", "S1 <-> S2 SWAPPED");
                break;
            case 5:
                audioProcessor.getPresetDirectory().revealToUser();
                break;
            case 10: setSize (896, 504);   break;
            case 11: setSize (1120, 630);  break;
            case 12: setSize (1400, 788);  break;
            case 13: setSize (1680, 945);  break;
            case 20:
                if (auto* topLevel = getTopLevelComponent())
                {
                    std::function<bool (juce::Component*)> clickOptionsButton = [&] (juce::Component* comp) -> bool
                    {
                        if (auto* btn = dynamic_cast<juce::TextButton*> (comp))
                        {
                            if (btn->getButtonText().containsIgnoreCase ("Options"))
                            {
                                btn->triggerClick();
                                return true;
                            }
                        }

                        for (int i = 0; i < comp->getNumChildComponents(); ++i)
                        {
                            if (clickOptionsButton (comp->getChildComponent (i)))
                                return true;
                        }
                        return false;
                    };

                    clickOptionsButton (topLevel);
                }
                break;
            default:
                break;
        }
    });
}

void NeomorphX1AudioProcessorEditor::drawLcdScreen (juce::Graphics& g, juce::Rectangle<float> lcdBounds)
{
    auto screen = lcdBounds.reduced (7.0f, 6.0f);
    const auto sceneAccent = (editingScene == 1)
                                 ? juce::Colour::fromRGB (0, 240, 255)
                                 : juce::Colour::fromRGB (255, 60, 180);

    g.setColour (juce::Colour::fromRGB (0, 240, 255).withAlpha (0.045f));
    for (float y = screen.getY(); y < screen.getBottom(); y += 3.0f)
        g.drawHorizontalLine (juce::roundToInt (y), screen.getX(), screen.getRight());

    const char* pageNames[4] = { "PATCH", "MORPH", "ASSIGN", "SYSTEM" };
    auto topBar = screen.removeFromTop (18.0f);

    g.setColour (juce::Colour::fromRGB (130, 175, 205));
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText ("MODE: " + juce::String (pageNames[static_cast<int> (currentLcdPage)]),
                topBar, juce::Justification::centredLeft);

    juce::String flags;
    if (sceneSolo) flags += "[SOLO] ";
    if (sceneLink) flags += "[LINK]";

    if (flags.isNotEmpty())
    {
        g.setColour (juce::Colour::fromRGB (255, 210, 60));
        g.drawText (flags.trim(), topBar, juce::Justification::centredRight);
    }
    else
    {
        g.setColour (juce::Colour::fromRGB (0, 240, 255).withAlpha (0.85f));
        g.drawText (audioProcessor.getCurrentPresetName().toUpperCase(),
                    topBar.withTrimmedLeft (topBar.getWidth() * 0.42f),
                    juce::Justification::centredRight, true);
    }

    g.setColour (sceneAccent.withAlpha (0.28f));
    g.drawHorizontalLine (juce::roundToInt (screen.getY() + 1.0f), screen.getX(), screen.getRight());
    screen.removeFromTop (4.0f);

    if (showLcdNotify)
    {
        auto titleArea = screen.removeFromTop (screen.getHeight() * 0.36f);
        g.setColour (juce::Colour::fromRGB (170, 215, 240));
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText (lcdNotifyTitle, titleArea, juce::Justification::centred, true);

        g.setColour (keypadActive ? juce::Colour::fromRGB (255, 210, 60) : sceneAccent);
        g.setFont (juce::FontOptions (21.0f, juce::Font::bold));
        g.drawText (lcdNotifyValue, screen, juce::Justification::centred, true);
        return;
    }

    switch (currentLcdPage)
    {
        case LcdPage::Patch:
        {
            auto mainSceneRect = screen.removeFromTop (screen.getHeight() * 0.62f);

            g.setColour (sceneAccent.withAlpha (0.22f));
            g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
            g.drawText ("SCENE 0" + juce::String (editingScene), mainSceneRect.translated (0.0f, 1.0f), juce::Justification::centred);

            g.setColour (sceneAccent);
            g.setFont (juce::FontOptions (27.0f, juce::Font::bold));
            g.drawText ("SCENE 0" + juce::String (editingScene), mainSceneRect, juce::Justification::centred);

            const auto& osc1Wave = oscControls[0].waveBox.getText();
            const auto& osc2Wave = oscControls[1].waveBox.getText();
            g.setColour (juce::Colour::fromRGB (160, 200, 225));
            g.setFont (juce::FontOptions (12.5f, juce::Font::plain));
            g.drawText (osc1Wave + "  +  " + osc2Wave, screen, juce::Justification::centred);
            break;
        }

        case LcdPage::Morph:
        {
            const float s1    = static_cast<float> (sceneFaders[0].getValue());
            const float morph = static_cast<float> (sceneFaders[1].getValue());
            const float s2    = static_cast<float> (sceneFaders[2].getValue());

            auto textRow = screen.removeFromTop (19.0f);
            g.setColour (juce::Colour::fromRGB (0, 240, 255));
            g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
            g.drawText (juce::String::formatted ("S1: %d%%", juce::roundToInt (s1 * 100.0f)),
                        textRow, juce::Justification::centredLeft);

            g.setColour (juce::Colour::fromRGB (255, 60, 180));
            g.drawText (juce::String::formatted ("S2: %d%%", juce::roundToInt (s2 * 100.0f)),
                        textRow, juce::Justification::centredRight);

            auto barArea = screen.reduced (4.0f, 9.0f);
            g.setColour (juce::Colour::fromRGB (16, 24, 36));
            g.fillRoundedRectangle (barArea, 4.0f);
            g.setColour (juce::Colour::fromRGB (50, 80, 110));
            g.drawRoundedRectangle (barArea, 4.0f, 1.0f);

            const float splitX = barArea.getX() + barArea.getWidth() * (1.0f - morph);
            g.setColour (juce::Colour::fromRGB (0, 240, 255).withAlpha (0.75f));
            g.fillRoundedRectangle (barArea.withRight (splitX), 3.0f);

            g.setColour (juce::Colour::fromRGB (255, 60, 180).withAlpha (0.75f));
            g.fillRoundedRectangle (barArea.withLeft (splitX), 3.0f);

            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (juce::String::formatted ("MORPH %d%%", juce::roundToInt (morph * 100.0f)),
                        barArea, juce::Justification::centred);
            break;
        }

        case LcdPage::Assign:
        {
            const float cellW = screen.getWidth() / 5.0f;
            const float cellH = screen.getHeight() / 2.0f;
            g.setFont (juce::FontOptions (11.5f, juce::Font::bold));

            for (int idx = 0; idx < 10; ++idx)
            {
                const float row = static_cast<float> (idx / 5);
                const float col = static_cast<float> (idx % 5);
                auto cell = juce::Rectangle<float> (screen.getX() + col * cellW,
                                                    screen.getY() + row * cellH,
                                                    cellW, cellH);

                const int pct = juce::roundToInt (matrixKnobs[static_cast<size_t> (idx)].getValue() * 100.0);
                juce::String labelStr;
                if (idx == 0)      labelStr = "VOL:" + juce::String (pct);
                else if (idx == 1) labelStr = "PAN:" + juce::String (pct);
                else               labelStr = "A" + juce::String (idx - 1) + ":" + juce::String (pct);

                g.setColour (idx < 2 ? juce::Colour::fromRGB (255, 60, 180)
                                     : (pct > 0 ? juce::Colour::fromRGB (0, 240, 255)
                                                : juce::Colour::fromRGB (115, 150, 180)));
                g.drawText (labelStr, cell, juce::Justification::centred);
            }
            break;
        }

        case LcdPage::System:
        {
            const int volPct = juce::roundToInt (matrixKnobs[0].getValue() * 100.0);
            const int panPct = juce::roundToInt (matrixKnobs[1].getValue() * 100.0);

            g.setColour (juce::Colour::fromRGB (0, 240, 255));
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText ("KEYPAD TARGET: " + keypadTargetLabel,
                        screen.removeFromTop (21.0f), juce::Justification::centred, true);

            g.setColour (juce::Colour::fromRGB (170, 205, 230));
            g.setFont (juce::FontOptions (12.5f, juce::Font::plain));
            g.drawText (juce::String::formatted ("VOL: %d%%   |   PAN: %d%%", volPct, panPct),
                        screen, juce::Justification::centred);
            break;
        }

        case LcdPage::NumPages:
        default:
            break;
    }
}

// ==============================================================================
// Unteres Status-Strip-Panel (0.2661f, 0.9016f bis 0.9113f, 0.9481f)
// ==============================================================================
void NeomorphX1AudioProcessorEditor::drawBottomStatusStrip (juce::Graphics& g, juce::Rectangle<float> stripBounds)
{
    const auto cyan    = juce::Colour::fromRGB (0, 240, 255);
    const auto magenta = juce::Colour::fromRGB (255, 60, 180);
    const auto amber   = juce::Colour::fromRGB (255, 205, 50);

    g.setColour (cyan.withAlpha (0.035f));
    for (float y = stripBounds.getY() + 1.0f; y < stripBounds.getBottom(); y += 3.0f)
        g.drawHorizontalLine (juce::roundToInt (y), stripBounds.getX() + 2.0f, stripBounds.getRight() - 2.0f);

    auto area = stripBounds.reduced (6.0f, 3.0f);
    const float totalW = area.getWidth();

    auto drawDivider = [&g, cyan] (float x, float top, float bottom)
    {
        g.setColour (cyan.withAlpha (0.22f));
        g.drawVerticalLine (juce::roundToInt (x), top + 2.0f, bottom - 2.0f);
    };

    // Sektion 1 (24%): MIDI Activity, Note/Velocity & 16-Voice-Monitor
    auto sec1 = area.removeFromLeft (totalW * 0.24f);
    drawDivider (sec1.getRight(), area.getY(), area.getBottom());
    sec1.removeFromRight (6.0f);

    auto sec1Top = sec1.removeFromTop (sec1.getHeight() * 0.50f);
    const float ledSize = 6.0f;
    auto ledRect = juce::Rectangle<float> (sec1Top.getX(), sec1Top.getCentreY() - ledSize * 0.5f, ledSize, ledSize);

    if (midiLedHoldTicks > 0)
    {
        g.setColour (magenta.withAlpha (0.35f));
        g.fillEllipse (ledRect.expanded (2.5f));
        g.setColour (magenta);
    }
    else
    {
        g.setColour (juce::Colour::fromRGB (35, 55, 75));
    }
    g.fillEllipse (ledRect);

    const int lastNote = audioProcessor.getLastMidiNote();
    const int lastVel  = audioProcessor.getLastMidiVelocity();
    juce::String midiText = "MIDI // IDLE";
    if (lastNote >= 0)
    {
        const auto noteName = juce::MidiMessage::getMidiNoteName (lastNote, true, true, 4);
        midiText = juce::String::formatted ("MIDI // %s  V%03d", noteName.toRawUTF8(), lastVel);
    }

    g.setColour (lastNote >= 0 ? cyan : juce::Colour::fromRGB (120, 155, 180));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (midiText, sec1Top.withTrimmedLeft (10.0f), juce::Justification::centredLeft, true);

    const int voices = juce::jlimit (0, 16, audioProcessor.getActiveVoiceCount());
    auto voiceLabelRect = sec1.removeFromRight (52.0f);
    g.setColour (voices > 0 ? magenta : juce::Colour::fromRGB (115, 145, 170));
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText (juce::String::formatted ("V %02d/16", voices), voiceLabelRect, juce::Justification::centredRight);

    auto segArea = sec1.reduced (0.0f, 3.0f);
    const float segW = segArea.getWidth() / 16.0f;
    for (int v = 0; v < 16; ++v)
    {
        auto seg = juce::Rectangle<float> (segArea.getX() + static_cast<float> (v) * segW + 1.0f,
                                           segArea.getY(),
                                           juce::jmax (1.5f, segW - 2.0f),
                                           segArea.getHeight());
        g.setColour (v < voices ? (v >= 12 ? amber : cyan)
                                : juce::Colour::fromRGB (22, 34, 48));
        g.fillRoundedRectangle (seg, 1.0f);
    }

    // Sektion 2 (32%): Echtzeit-Oszilloskop / Morph-Vektor (Klick schaltet um)
    auto sec2 = area.removeFromLeft (totalW * 0.32f);
    drawDivider (sec2.getRight(), area.getY(), area.getBottom());
    sec2 = sec2.reduced (6.0f, 0.0f);
    scopeClickArea = sec2;

    g.setColour (juce::Colour::fromRGB (5, 9, 15).withAlpha (0.85f));
    g.fillRoundedRectangle (sec2, 3.0f);
    g.setColour (cyan.withAlpha (0.24f));
    g.drawRoundedRectangle (sec2, 3.0f, 1.0f);

    if (bottomStripMode == BottomStripMode::Oscilloscope)
    {
        const auto inner = sec2.reduced (3.0f, 2.0f);
        g.setColour (cyan.withAlpha (0.14f));
        g.drawHorizontalLine (juce::roundToInt (inner.getCentreY()), inner.getX(), inner.getRight());

        juce::Path wavePath;
        const int numPts = NeomorphX1AudioProcessor::scopeBufferSize;
        for (int i = 0; i < numPts; ++i)
        {
            const float normX = static_cast<float> (i) / static_cast<float> (numPts - 1);
            const float sVal  = juce::jlimit (-1.0f, 1.0f, scopeCache[static_cast<size_t> (i)] * 1.35f);
            const float px    = inner.getX() + normX * inner.getWidth();
            const float py    = inner.getCentreY() - sVal * (inner.getHeight() * 0.46f);

            if (i == 0)
                wavePath.startNewSubPath (px, py);
            else
                wavePath.lineTo (px, py);
        }

        g.setColour (cyan.withAlpha (0.26f));
        g.strokePath (wavePath, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (cyan);
        g.strokePath (wavePath, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (cyan.withAlpha (0.55f));
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.drawText ("SCOPE", inner.reduced (2.0f, 0.0f), juce::Justification::topLeft);
    }
    else
    {
        const float effMorph = audioProcessor.getLiveEffectiveMorph();
        const float s1Lvl    = static_cast<float> (sceneFaders[0].getValue());
        const float s2Lvl    = static_cast<float> (sceneFaders[2].getValue());

        auto inner = sec2.reduced (6.0f, 4.0f);
        auto bar   = inner.removeFromBottom (inner.getHeight() * 0.44f);

        g.setColour (cyan);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (juce::String::formatted ("S1 %.0f%%", s1Lvl * (1.0f - effMorph) * 100.0f),
                    inner, juce::Justification::centredLeft);

        g.setColour (magenta);
        g.drawText (juce::String::formatted ("S2 %.0f%%", s2Lvl * effMorph * 100.0f),
                    inner, juce::Justification::centredRight);

        g.setColour (juce::Colour::fromRGB (140, 180, 210));
        g.drawText (juce::String::formatted ("MORPH %d%%", juce::roundToInt (effMorph * 100.0f)),
                    inner, juce::Justification::centred);

        g.setColour (juce::Colour::fromRGB (16, 24, 36));
        g.fillRoundedRectangle (bar, 2.5f);

        const float splitX = bar.getX() + bar.getWidth() * (1.0f - effMorph);
        g.setColour (cyan.withAlpha (0.80f));
        g.fillRoundedRectangle (bar.withRight (splitX), 2.0f);
        g.setColour (magenta.withAlpha (0.80f));
        g.fillRoundedRectangle (bar.withLeft (splitX), 2.0f);

        g.setColour (juce::Colours::white);
        g.fillRect (splitX - 1.0f, bar.getY(), 2.0f, bar.getHeight());
    }

    // Sektion 3 (26%): Stereo Output Peak Meter L / R
    auto sec3 = area.removeFromLeft (totalW * 0.26f);
    drawDivider (sec3.getRight(), area.getY(), area.getBottom());
    sec3 = sec3.reduced (6.0f, 1.0f);

    auto drawChannelMeter = [&g, cyan, magenta, amber] (juce::Rectangle<float> row,
                                                        const char* chLabel,
                                                        float level,
                                                        float hold)
    {
        auto lblRect = row.removeFromLeft (14.0f);
        g.setColour (juce::Colour::fromRGB (150, 190, 215));
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (chLabel, lblRect, juce::Justification::centredLeft);

        auto meterRect = row.reduced (0.0f, 2.5f);
        g.setColour (juce::Colour::fromRGB (8, 13, 20));
        g.fillRoundedRectangle (meterRect, 2.0f);
        g.setColour (juce::Colour::fromRGB (35, 55, 75).withAlpha (0.7f));
        g.drawRoundedRectangle (meterRect, 2.0f, 1.0f);

        const float clampedLvl  = juce::jlimit (0.0f, 1.0f, level);
        const float clampedHold = juce::jlimit (0.0f, 1.0f, hold);

        if (clampedLvl > 0.002f)
        {
            juce::ColourGradient grad (cyan, meterRect.getX(), meterRect.getCentreY(),
                                       magenta, meterRect.getRight(), meterRect.getCentreY(), false);
            grad.addColour (0.82, amber);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (meterRect.withWidth (meterRect.getWidth() * clampedLvl), 2.0f);
        }

        if (clampedHold > 0.01f)
        {
            const float holdX = meterRect.getX() + meterRect.getWidth() * clampedHold;
            g.setColour (clampedHold > 0.96f ? juce::Colour::fromRGB (255, 50, 70) : juce::Colours::white);
            g.fillRect (holdX - 1.0f, meterRect.getY() + 1.0f, 2.0f, meterRect.getHeight() - 2.0f);
        }
    };

    auto meterTop = sec3.removeFromTop (sec3.getHeight() * 0.50f);
    drawChannelMeter (meterTop, "L", smoothedLevelL, peakHoldL);
    drawChannelMeter (sec3,     "R", smoothedLevelR, peakHoldR);

    // Sektion 4 (Rest ~18%): Host BPM, Sample-Rate & DSP Status
    auto sec4 = area.withTrimmedLeft (6.0f);
    const float bpm   = audioProcessor.getHostBpm();
    const double srKhz = audioProcessor.getCurrentSampleRateHz() / 1000.0;

    auto sec4Top = sec4.removeFromTop (sec4.getHeight() * 0.50f);
    g.setColour (cyan);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (juce::String::formatted ("%.1f BPM // %.1f KHZ", bpm, srKhz),
                sec4Top, juce::Justification::centredRight, true);

    g.setColour (juce::Colour::fromRGB (60, 255, 150));
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("DSP // ONLINE", sec4, juce::Justification::centredRight, true);
}

void NeomorphX1AudioProcessorEditor::drawSceneLeds (juce::Graphics& g, float w, float h)
{
    constexpr float ledX[4] = { 0.4976f, 0.5369f, 0.5762f, 0.6173f };
    const float ledY        = 0.6250f * h;
    const float ledRadius   = 0.0042f * w;

    const bool ledActive[4] = {
        (editingScene == 1),
        (editingScene == 2),
        sceneSolo,
        sceneLink
    };

    const juce::Colour ledColours[4] = {
        juce::Colour::fromRGB (0, 240, 255),
        juce::Colour::fromRGB (255, 60, 180),
        juce::Colour::fromRGB (255, 205, 50),
        juce::Colour::fromRGB (60, 255, 150)
    };

    for (int i = 0; i < 4; ++i)
    {
        const float cx = ledX[i] * w;
        const auto col = ledColours[i];

        if (ledActive[i])
        {
            g.setColour (col.withAlpha (0.35f));
            g.fillEllipse (cx - ledRadius * 2.1f, ledY - ledRadius * 2.1f, ledRadius * 4.2f, ledRadius * 4.2f);

            g.setColour (col);
            g.fillEllipse (cx - ledRadius, ledY - ledRadius, ledRadius * 2.0f, ledRadius * 2.0f);

            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.fillEllipse (cx - ledRadius * 0.45f, ledY - ledRadius * 0.45f, ledRadius * 0.7f, ledRadius * 0.7f);
        }
        else
        {
            g.setColour (juce::Colour::fromRGB (14, 20, 28).withAlpha (0.75f));
            g.fillEllipse (cx - ledRadius, ledY - ledRadius, ledRadius * 2.0f, ledRadius * 2.0f);
        }
    }
}

void NeomorphX1AudioProcessorEditor::drawOscillatorSection (juce::Graphics& g,
                                                            juce::Rectangle<float> panelBounds,
                                                            int oscIndex,
                                                            juce::Colour accentColour)
{
    g.setColour (juce::Colour::fromRGB (10, 15, 24).withAlpha (0.74f));
    g.fillRoundedRectangle (panelBounds, 6.0f);

    g.setColour (accentColour.withAlpha (0.38f));
    g.drawRoundedRectangle (panelBounds, 6.0f, 1.0f);

    auto headerArea = panelBounds.reduced (10.0f, 4.0f).removeFromTop (19.0f);
    g.setColour (accentColour);
    g.setFont (juce::FontOptions (14.5f, juce::Font::bold));
    g.drawText ("S" + juce::String (editingScene) + " // OSCILLATOR " + juce::String (oscIndex + 1),
                headerArea, juce::Justification::centredLeft);

    const auto& osc = oscControls[static_cast<size_t> (oscIndex)];
    const auto boxBounds = osc.waveBox.getBounds().toFloat();
    auto scopeRect = juce::Rectangle<float> (boxBounds.getRight() + 8.0f,
                                             boxBounds.getY() - 6.0f,
                                             panelBounds.getRight() - boxBounds.getRight() - 18.0f,
                                             boxBounds.getHeight() + 12.0f);

    g.setColour (juce::Colour::fromRGB (6, 10, 16));
    g.fillRoundedRectangle (scopeRect, 4.0f);
    g.setColour (juce::Colour::fromRGB (35, 55, 75).withAlpha (0.7f));
    g.drawRoundedRectangle (scopeRect, 4.0f, 1.0f);

    g.setColour (accentColour.withAlpha (0.15f));
    g.drawHorizontalLine (juce::roundToInt (scopeRect.getCentreY()), scopeRect.getX() + 3.0f, scopeRect.getRight() - 3.0f);

    const int waveType    = osc.waveBox.getSelectedItemIndex();
    const float pos       = static_cast<float> (osc.positionKnob.getValue());
    const float morph     = static_cast<float> (osc.morphKnob.getValue());
    const float phaseNorm = static_cast<float> (osc.phaseKnob.getValue()) / 360.0f;
    const auto inner      = scopeRect.reduced (4.0f, 4.0f);

    juce::Path wavePath;
    const int steps = 90;
    for (int s = 0; s <= steps; ++s)
    {
        const float rawT = static_cast<float> (s) / static_cast<float> (steps);
        const float t    = std::fmod (rawT + phaseNorm, 1.0f);
        float sample     = 0.0f;

        switch (waveType)
        {
            case 0:
            {
                const float sine = std::sin (t * juce::MathConstants<float>::twoPi);
                const float tri  = 1.0f - 4.0f * std::abs (std::round (t - 0.25f) - (t - 0.25f));
                const float saw  = 2.0f * t - 1.0f;
                const float sqr  = (t < 0.5f) ? 1.0f : -1.0f;

                if (pos < 0.33f)
                    sample = juce::jmap (pos / 0.33f, sine, tri);
                else if (pos < 0.66f)
                    sample = juce::jmap ((pos - 0.33f) / 0.33f, tri, saw);
                else
                    sample = juce::jmap ((pos - 0.66f) / 0.34f, saw, sqr);
                break;
            }
            case 1:
            {
                const float pw = juce::jmap (pos, 0.08f, 0.92f);
                sample = (t < pw) ? 0.85f : -0.85f;
                break;
            }
            case 2:
            {
                const float harmonic = 2.0f + pos * 7.0f;
                const float window   = std::sin (t * juce::MathConstants<float>::pi);
                sample = std::sin (t * juce::MathConstants<float>::twoPi * harmonic) * window;
                break;
            }
            case 3:
            {
                const float driven = std::sin (t * juce::MathConstants<float>::twoPi) * (1.0f + pos * 3.5f);
                sample = std::sin (driven * juce::MathConstants<float>::halfPi);
                break;
            }
            case 4:
            {
                const float s1 = 2.0f * t - 1.0f;
                const float s2 = 2.0f * std::fmod (t * 1.03f + 0.17f, 1.0f) - 1.0f;
                const float s3 = 2.0f * std::fmod (t * 0.97f + 0.41f, 1.0f) - 1.0f;
                sample = s1 * (1.0f - pos * 0.45f) + (s2 + s3) * (pos * 0.32f);
                break;
            }
            default:
            {
                sample = 0.6f * std::sin (t * juce::MathConstants<float>::twoPi)
                       + 0.4f * std::cos (t * juce::MathConstants<float>::twoPi * (2.0f + pos * 5.0f));
                break;
            }
        }

        if (morph > 0.001f)
            sample = std::sin (sample * (1.0f + morph * 2.2f));

        const float px = inner.getX() + rawT * inner.getWidth();
        const float py = inner.getCentreY() - juce::jlimit (-1.0f, 1.0f, sample) * (inner.getHeight() * 0.48f);

        if (s == 0)
            wavePath.startNewSubPath (px, py);
        else
            wavePath.lineTo (px, py);
    }

    g.setColour (accentColour.withAlpha (0.28f));
    g.strokePath (wavePath, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accentColour);
    g.strokePath (wavePath, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto drawKnobLabel = [&g] (const juce::Slider& knob, const juce::String& text)
    {
        const auto kb = knob.getBounds().toFloat();
        auto labelRect = juce::Rectangle<float> (kb.getX() - 8.0f, kb.getBottom() - 2.0f, kb.getWidth() + 16.0f, 15.0f);
        g.setColour (juce::Colour::fromRGB (170, 200, 225));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (text, labelRect, juce::Justification::centred);
    };

    drawKnobLabel (osc.octaveKnob,   "OCT");
    drawKnobLabel (osc.semiKnob,     "SEMI");
    drawKnobLabel (osc.detuneKnob,   "DETUNE");
    drawKnobLabel (osc.positionKnob, "POS");

    drawKnobLabel (osc.morphKnob,    "MORPH");
    drawKnobLabel (osc.phaseKnob,    "PHASE");
    drawKnobLabel (osc.panKnob,      "PAN");
    drawKnobLabel (osc.levelKnob,    "LEVEL");
}

void NeomorphX1AudioProcessorEditor::drawFilterPanels (juce::Graphics& g,
                                                       juce::Rectangle<float> leftBounds,
                                                       juce::Rectangle<float> rightBounds)
{
    const auto cyanAccent    = juce::Colour::fromRGB (0, 240, 255);
    const auto magentaAccent = juce::Colour::fromRGB (255, 60, 180);

    auto drawKnobLabel = [&g] (const juce::Slider& knob, const juce::String& text)
    {
        const auto kb = knob.getBounds().toFloat();
        auto labelRect = juce::Rectangle<float> (kb.getX() - 10.0f, kb.getBottom() - 2.0f, kb.getWidth() + 20.0f, 15.0f);
        g.setColour (juce::Colour::fromRGB (170, 200, 225));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (text, labelRect, juce::Justification::centred);
    };

    g.setColour (juce::Colour::fromRGB (10, 15, 24).withAlpha (0.74f));
    g.fillRoundedRectangle (leftBounds, 6.0f);
    g.setColour (cyanAccent.withAlpha (0.38f));
    g.drawRoundedRectangle (leftBounds, 6.0f, 1.0f);

    auto leftHeader = leftBounds.reduced (10.0f, 4.0f).removeFromTop (19.0f);
    g.setColour (cyanAccent);
    g.setFont (juce::FontOptions (14.5f, juce::Font::bold));
    g.drawText ("S" + juce::String (editingScene) + " // MULTIMODE FILTER",
                leftHeader, juce::Justification::centredLeft);

    const auto typeBounds    = filterControls.typeBox.getBounds().toFloat();
    const auto routingBounds = filterControls.routingBox.getBounds().toFloat();
    auto scopeRect = juce::Rectangle<float> (typeBounds.getRight() + 8.0f,
                                             typeBounds.getY(),
                                             leftBounds.getRight() - typeBounds.getRight() - 18.0f,
                                             routingBounds.getBottom() - typeBounds.getY());

    g.setColour (juce::Colour::fromRGB (6, 10, 16));
    g.fillRoundedRectangle (scopeRect, 4.0f);
    g.setColour (juce::Colour::fromRGB (35, 55, 75).withAlpha (0.7f));
    g.drawRoundedRectangle (scopeRect, 4.0f, 1.0f);

    const auto inner   = scopeRect.reduced (5.0f, 6.0f);
    const float zeroDbY = inner.getY() + inner.getHeight() * 0.34f;
    g.setColour (cyanAccent.withAlpha (0.14f));
    g.drawHorizontalLine (juce::roundToInt (zeroDbY), inner.getX(), inner.getRight());

    const int filterType = filterControls.typeBox.getSelectedItemIndex();
    const float cutoffHz = static_cast<float> (filterControls.cutoffKnob.getValue());
    const float res      = static_cast<float> (filterControls.resonanceKnob.getValue());
    const float envAmt   = static_cast<float> (filterControls.envAmtKnob.getValue());

    const float cutNorm = std::log (juce::jlimit (20.0f, 20000.0f, cutoffHz) / 20.0f) / std::log (1000.0f);

    if (std::abs (envAmt) > 0.02f)
    {
        const float modNorm = juce::jlimit (0.0f, 1.0f, cutNorm + envAmt * 0.45f);
        const float modX    = inner.getX() + modNorm * inner.getWidth();
        g.setColour (magentaAccent.withAlpha (0.45f));
        g.drawVerticalLine (juce::roundToInt (modX), inner.getY(), inner.getBottom());
    }

    juce::Path respPath;
    const int steps = 90;
    for (int s = 0; s <= steps; ++s)
    {
        const float xNorm = static_cast<float> (s) / static_cast<float> (steps);
        const float dist  = xNorm - cutNorm;
        const float resPeak = res * 0.34f * std::exp (-(dist * dist) / 0.0022f);
        float mag = 0.0f;

        switch (filterType)
        {
            case 0: mag = 1.0f / (1.0f + std::exp (dist * 16.0f)); break;
            case 1: mag = 1.0f / (1.0f + std::exp (dist * 28.0f)); break;
            case 2: mag = std::exp (-(dist * dist) / 0.014f); break;
            case 3: mag = 1.0f / (1.0f + std::exp (-dist * 20.0f)); break;
            case 4: mag = 1.0f - 0.92f * std::exp (-(dist * dist) / 0.0045f); break;
            default: mag = 0.55f + 0.35f * std::cos ((xNorm - cutNorm) * 42.0f); break;
        }

        const float level = juce::jlimit (0.0f, 1.38f, mag + resPeak);
        const float px = inner.getX() + xNorm * inner.getWidth();
        const float py = juce::jmap (level, 0.0f, 1.38f, inner.getBottom(), inner.getY());

        if (s == 0)
            respPath.startNewSubPath (px, py);
        else
            respPath.lineTo (px, py);
    }

    g.setColour (cyanAccent.withAlpha (0.28f));
    g.strokePath (respPath, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cyanAccent);
    g.strokePath (respPath, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    drawKnobLabel (filterControls.cutoffKnob,    "CUTOFF");
    drawKnobLabel (filterControls.resonanceKnob, "RESONANCE");

    g.setColour (juce::Colour::fromRGB (10, 15, 24).withAlpha (0.74f));
    g.fillRoundedRectangle (rightBounds, 6.0f);
    g.setColour (magentaAccent.withAlpha (0.38f));
    g.drawRoundedRectangle (rightBounds, 6.0f, 1.0f);

    auto rightHeader = rightBounds.reduced (10.0f, 4.0f).removeFromTop (19.0f);
    g.setColour (magentaAccent);
    g.setFont (juce::FontOptions (14.5f, juce::Font::bold));
    g.drawText ("S" + juce::String (editingScene) + " // DRIVE & MODULATION",
                rightHeader, juce::Justification::centredLeft);

    drawKnobLabel (filterControls.driveKnob,    "DRIVE");
    drawKnobLabel (filterControls.envAmtKnob,   "ENV AMT");
    drawKnobLabel (filterControls.keytrackKnob, "KEYTRACK");
    drawKnobLabel (filterControls.velSensKnob,  "VELOCITY");
}

void NeomorphX1AudioProcessorEditor::drawEnvelopeSection (juce::Graphics& g,
                                                          juce::Rectangle<float> panelBounds,
                                                          int envIndex,
                                                          juce::Colour accentColour)
{
    g.setColour (juce::Colour::fromRGB (10, 15, 24).withAlpha (0.74f));
    g.fillRoundedRectangle (panelBounds, 6.0f);

    g.setColour (accentColour.withAlpha (0.38f));
    g.drawRoundedRectangle (panelBounds, 6.0f, 1.0f);

    auto headerArea = panelBounds.reduced (10.0f, 4.0f).removeFromTop (19.0f);
    g.setColour (accentColour);
    g.setFont (juce::FontOptions (14.5f, juce::Font::bold));
    const juce::String title = (envIndex == 0) ? "AMP ENVELOPE" : "FILTER / MOD ENVELOPE";
    g.drawText ("S" + juce::String (editingScene) + " // " + title,
                headerArea, juce::Justification::centredLeft);

    const auto& env = envControls[static_cast<size_t> (envIndex)];
    const float displayTop    = panelBounds.getY() + panelBounds.getHeight() * 0.17f;
    const float displayHeight = panelBounds.getHeight() * 0.44f;
    auto adsrRect = juce::Rectangle<float> (panelBounds.getX() + 10.0f,
                                            displayTop,
                                            panelBounds.getWidth() - 20.0f,
                                            displayHeight);

    g.setColour (juce::Colour::fromRGB (6, 10, 16));
    g.fillRoundedRectangle (adsrRect, 4.0f);
    g.setColour (juce::Colour::fromRGB (35, 55, 75).withAlpha (0.7f));
    g.drawRoundedRectangle (adsrRect, 4.0f, 1.0f);

    g.setColour (accentColour.withAlpha (0.10f));
    g.drawHorizontalLine (juce::roundToInt (adsrRect.getCentreY()), adsrRect.getX() + 3.0f, adsrRect.getRight() - 3.0f);
    for (int col = 1; col < 4; ++col)
    {
        const float gx = adsrRect.getX() + (adsrRect.getWidth() * 0.25f) * static_cast<float> (col);
        g.drawVerticalLine (juce::roundToInt (gx), adsrRect.getY() + 3.0f, adsrRect.getBottom() - 3.0f);
    }

    const auto inner = adsrRect.reduced (8.0f, 8.0f);
    const float aNorm = std::pow (juce::jlimit (0.0f, 10.0f, static_cast<float> (env.attackKnob.getValue()))  / 10.0f, 0.42f);
    const float dNorm = std::pow (juce::jlimit (0.0f, 10.0f, static_cast<float> (env.decayKnob.getValue()))   / 10.0f, 0.42f);
    const float sVal  = juce::jlimit (0.0f, 1.0f, static_cast<float> (env.sustainKnob.getValue()));
    const float rNorm = std::pow (juce::jlimit (0.0f, 10.0f, static_cast<float> (env.releaseKnob.getValue())) / 10.0f, 0.42f);

    const float segW    = inner.getWidth() * 0.26f;
    const float startX  = inner.getX();
    const float baseY   = inner.getBottom();
    const float topY    = inner.getY();

    const float atkX    = startX + juce::jmax (4.0f, aNorm * segW);
    const float decX    = atkX   + juce::jmax (4.0f, dNorm * segW);
    const float susY    = baseY  - sVal * inner.getHeight();
    const float susEndX = decX   + inner.getWidth() * 0.20f;
    const float relX    = juce::jmin (inner.getRight(), susEndX + juce::jmax (4.0f, rNorm * segW));

    juce::Path envPath;
    envPath.startNewSubPath (startX, baseY);
    envPath.lineTo (atkX, topY);
    envPath.lineTo (decX, susY);
    envPath.lineTo (susEndX, susY);
    envPath.lineTo (relX, baseY);

    juce::Path fillPath (envPath);
    fillPath.lineTo (startX, baseY);
    fillPath.closeSubPath();

    juce::ColourGradient fillGrad (accentColour.withAlpha (0.24f), startX, topY,
                                   accentColour.withAlpha (0.02f), startX, baseY, false);
    g.setGradientFill (fillGrad);
    g.fillPath (fillPath);

    g.setColour (accentColour.withAlpha (0.30f));
    g.strokePath (envPath, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accentColour);
    g.strokePath (envPath, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto drawNode = [&g, accentColour] (float nx, float ny)
    {
        g.setColour (juce::Colour::fromRGB (10, 15, 24));
        g.fillEllipse (nx - 3.5f, ny - 3.5f, 7.0f, 7.0f);
        g.setColour (accentColour);
        g.drawEllipse (nx - 3.5f, ny - 3.5f, 7.0f, 7.0f, 1.5f);
    };
    drawNode (atkX, topY);
    drawNode (decX, susY);
    drawNode (susEndX, susY);

    auto drawKnobLabel = [&g] (const juce::Slider& knob, const juce::String& text)
    {
        const auto kb = knob.getBounds().toFloat();
        auto labelRect = juce::Rectangle<float> (kb.getX() - 8.0f, kb.getBottom() - 2.0f, kb.getWidth() + 16.0f, 15.0f);
        g.setColour (juce::Colour::fromRGB (170, 200, 225));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (text, labelRect, juce::Justification::centred);
    };

    drawKnobLabel (env.attackKnob,  "ATTACK");
    drawKnobLabel (env.decayKnob,   "DECAY");
    drawKnobLabel (env.sustainKnob, "SUSTAIN");
    drawKnobLabel (env.releaseKnob, "RELEASE");
}

void NeomorphX1AudioProcessorEditor::drawFreeEgPanels (juce::Graphics& g,
                                                       juce::Rectangle<float> leftBounds,
                                                       juce::Rectangle<float> rightBounds)
{
    const auto cyanAccent    = juce::Colour::fromRGB (0, 240, 255);
    const auto magentaAccent = juce::Colour::fromRGB (255, 60, 180);

    g.setColour (juce::Colour::fromRGB (10, 15, 24).withAlpha (0.74f));
    g.fillRoundedRectangle (leftBounds, 6.0f);
    g.setColour (cyanAccent.withAlpha (0.38f));
    g.drawRoundedRectangle (leftBounds, 6.0f, 1.0f);

    auto leftHeader = leftBounds.reduced (10.0f, 4.0f).removeFromTop (19.0f);
    g.setColour (cyanAccent);
    g.setFont (juce::FontOptions (14.5f, juce::Font::bold));
    g.drawText ("S" + juce::String (editingScene) + " // FREE EG CURVE",
                leftHeader, juce::Justification::centredLeft);

    const auto targetBounds = freeEgControls.targetBox.getBounds().toFloat();
    auto targetLabelRect = juce::Rectangle<float> (leftBounds.getX() + 10.0f,
                                                   targetBounds.getY(),
                                                   targetBounds.getX() - leftBounds.getX() - 14.0f,
                                                   targetBounds.getHeight());
    g.setColour (juce::Colour::fromRGB (170, 200, 225));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("TARGET:", targetLabelRect, juce::Justification::centredLeft);

    g.setColour (juce::Colour::fromRGB (10, 15, 24).withAlpha (0.74f));
    g.fillRoundedRectangle (rightBounds, 6.0f);
    g.setColour (magentaAccent.withAlpha (0.38f));
    g.drawRoundedRectangle (rightBounds, 6.0f, 1.0f);

    auto rightHeader = rightBounds.reduced (10.0f, 4.0f).removeFromTop (19.0f);
    g.setColour (magentaAccent);
    g.setFont (juce::FontOptions (14.5f, juce::Font::bold));
    g.drawText ("S" + juce::String (editingScene) + " // TIMING & LOOP",
                rightHeader, juce::Justification::centredLeft);

    auto drawKnobLabel = [&g] (const juce::Slider& knob, const juce::String& text)
    {
        const auto kb = knob.getBounds().toFloat();
        auto labelRect = juce::Rectangle<float> (kb.getX() - 10.0f, kb.getBottom() - 2.0f, kb.getWidth() + 20.0f, 15.0f);
        g.setColour (juce::Colour::fromRGB (170, 200, 225));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (text, labelRect, juce::Justification::centred);
    };

    drawKnobLabel (freeEgControls.timeKnob,   "TIME");
    drawKnobLabel (freeEgControls.amountKnob, "AMOUNT");
    drawKnobLabel (freeEgControls.smoothKnob, "SMOOTH");
}

void NeomorphX1AudioProcessorEditor::drawStepSeqPanels (juce::Graphics& g,
                                                        juce::Rectangle<float> leftBounds,
                                                        juce::Rectangle<float> rightBounds)
{
    const auto cyanAccent    = juce::Colour::fromRGB (0, 240, 255);
    const auto magentaAccent = juce::Colour::fromRGB (255, 60, 180);

    g.setColour (juce::Colour::fromRGB (10, 15, 24).withAlpha (0.74f));
    g.fillRoundedRectangle (leftBounds, 6.0f);
    g.setColour (cyanAccent.withAlpha (0.38f));
    g.drawRoundedRectangle (leftBounds, 6.0f, 1.0f);

    auto leftHeader = leftBounds.reduced (10.0f, 4.0f).removeFromTop (19.0f);
    g.setColour (cyanAccent);
    g.setFont (juce::FontOptions (14.5f, juce::Font::bold));
    g.drawText ("S" + juce::String (editingScene) + " // STEP SEQUENCER",
                leftHeader, juce::Justification::centredLeft);

    g.setColour (juce::Colour::fromRGB (10, 15, 24).withAlpha (0.74f));
    g.fillRoundedRectangle (rightBounds, 6.0f);
    g.setColour (magentaAccent.withAlpha (0.38f));
    g.drawRoundedRectangle (rightBounds, 6.0f, 1.0f);

    auto rightHeader = rightBounds.reduced (10.0f, 4.0f).removeFromTop (19.0f);
    g.setColour (magentaAccent);
    g.setFont (juce::FontOptions (14.5f, juce::Font::bold));
    g.drawText ("S" + juce::String (editingScene) + " // CLOCK & GROOVE",
                rightHeader, juce::Justification::centredLeft);

    const auto rateBounds = stepSeqControls.rateBox.getBounds().toFloat();
    auto rateLabelRect = juce::Rectangle<float> (rightBounds.getX() + 10.0f,
                                                 rateBounds.getY(),
                                                 rateBounds.getX() - rightBounds.getX() - 14.0f,
                                                 rateBounds.getHeight());
    g.setColour (juce::Colour::fromRGB (170, 200, 225));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("RATE:", rateLabelRect, juce::Justification::centredLeft);

    auto drawKnobLabel = [&g] (const juce::Slider& knob, const juce::String& text)
    {
        const auto kb = knob.getBounds().toFloat();
        auto labelRect = juce::Rectangle<float> (kb.getX() - 10.0f, kb.getBottom() - 2.0f, kb.getWidth() + 20.0f, 15.0f);
        g.setColour (juce::Colour::fromRGB (170, 200, 225));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (text, labelRect, juce::Justification::centred);
    };

    drawKnobLabel (stepSeqControls.gateKnob,   "GATE");
    drawKnobLabel (stepSeqControls.swingKnob,  "SWING");
    drawKnobLabel (stepSeqControls.amountKnob, "GLIDE / AMT");
}

void NeomorphX1AudioProcessorEditor::drawModMatrixPanels (juce::Graphics& g,
                                                          juce::Rectangle<float> leftBounds,
                                                          juce::Rectangle<float> rightBounds)
{
    const auto cyanAccent    = juce::Colour::fromRGB (0, 240, 255);
    const auto magentaAccent = juce::Colour::fromRGB (255, 60, 180);

    g.setColour (juce::Colour::fromRGB (10, 15, 24).withAlpha (0.74f));
    g.fillRoundedRectangle (leftBounds, 6.0f);
    g.setColour (cyanAccent.withAlpha (0.38f));
    g.drawRoundedRectangle (leftBounds, 6.0f, 1.0f);

    auto leftHeader = leftBounds.reduced (10.0f, 4.0f).removeFromTop (19.0f);
    g.setColour (cyanAccent);
    g.setFont (juce::FontOptions (14.5f, juce::Font::bold));
    g.drawText ("S" + juce::String (editingScene) + " // MOD MATRIX (SLOTS 1-4)",
                leftHeader, juce::Justification::centredLeft);

    g.setColour (juce::Colour::fromRGB (10, 15, 24).withAlpha (0.74f));
    g.fillRoundedRectangle (rightBounds, 6.0f);
    g.setColour (magentaAccent.withAlpha (0.38f));
    g.drawRoundedRectangle (rightBounds, 6.0f, 1.0f);

    auto rightHeader = rightBounds.reduced (10.0f, 4.0f).removeFromTop (19.0f);
    g.setColour (magentaAccent);
    g.setFont (juce::FontOptions (14.5f, juce::Font::bold));
    g.drawText ("S" + juce::String (editingScene) + " // LFO 1 & MONITOR",
                rightHeader, juce::Justification::centredLeft);

    const auto syncBounds = modMatrixControls.lfoSyncBox.getBounds().toFloat();
    auto syncLabelRect = juce::Rectangle<float> (rightBounds.getX() + 10.0f,
                                                 syncBounds.getY(),
                                                 syncBounds.getX() - rightBounds.getX() - 14.0f,
                                                 syncBounds.getHeight());
    g.setColour (juce::Colour::fromRGB (170, 200, 225));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("LFO SYNC:", syncLabelRect, juce::Justification::centredLeft);

    auto drawKnobLabel = [&g] (const juce::Slider& knob, const juce::String& text)
    {
        const auto kb = knob.getBounds().toFloat();
        auto labelRect = juce::Rectangle<float> (kb.getX() - 10.0f, kb.getBottom() - 3.0f, kb.getWidth() + 20.0f, 14.0f);
        g.setColour (juce::Colour::fromRGB (170, 200, 225));
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (text, labelRect, juce::Justification::centred);
    };

    drawKnobLabel (modMatrixControls.lfoRateKnob,  "RATE");
    drawKnobLabel (modMatrixControls.lfoFadeKnob,  "FADE");
    drawKnobLabel (modMatrixControls.lfoDepthKnob, "DEPTH");

    auto meterBox = juce::Rectangle<float> (rightBounds.getX() + 10.0f,
                                            rightBounds.getBottom() - rightBounds.getHeight() * 0.16f,
                                            rightBounds.getWidth() - 20.0f,
                                            rightBounds.getHeight() * 0.11f);
    g.setColour (juce::Colour::fromRGB (6, 10, 16));
    g.fillRoundedRectangle (meterBox, 4.0f);
    g.setColour (juce::Colour::fromRGB (35, 55, 75).withAlpha (0.7f));
    g.drawRoundedRectangle (meterBox, 4.0f, 1.0f);

    const float midX = meterBox.getCentreX();
    g.setColour (cyanAccent.withAlpha (0.25f));
    g.drawVerticalLine (juce::roundToInt (midX), meterBox.getY() + 2.0f, meterBox.getBottom() - 2.0f);

    const float lfoVal = juce::jlimit (-1.0f, 1.0f, lastPaintedLfo);
    const float barW   = std::abs (lfoVal) * (meterBox.getWidth() * 0.46f);
    const float barX   = (lfoVal >= 0.0f) ? midX : (midX - barW);

    g.setColour (magentaAccent.withAlpha (0.85f));
    g.fillRoundedRectangle (barX, meterBox.getY() + 3.0f, juce::jmax (2.0f, barW), meterBox.getHeight() - 6.0f, 2.0f);

    g.setColour (juce::Colour::fromRGB (190, 220, 240));
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("LFO 1 OUT", meterBox.reduced (6.0f, 0.0f), juce::Justification::centredLeft);
}

void NeomorphX1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (10, 14, 22));

    auto bounds   = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    if (backgroundImage.isValid())
        g.drawImage (backgroundImage, bounds, juce::RectanglePlacement::stretchToFit);

    // Aktiven Tab in der Tab-Leiste dezent hervorheben
    const auto activeTabRect = tabButtons[static_cast<size_t> (currentTab)].getBounds().toFloat();
    if (! activeTabRect.isEmpty())
    {
        const auto cyan = juce::Colour::fromRGB (0, 240, 255);
        g.setColour (cyan.withAlpha (0.14f));
        g.fillRoundedRectangle (activeTabRect.reduced (2.0f, 2.0f), 3.0f);
        g.setColour (cyan);
        g.fillRect (activeTabRect.getX() + 4.0f, activeTabRect.getBottom() - 2.5f,
                    activeTabRect.getWidth() - 8.0f, 2.5f);
    }

    // Exakter LCD-Rahmen (0.0774f, 0.2243f bis 0.2964f, 0.4053f)
    drawLcdScreen (g, juce::Rectangle<float> (0.0774f * w,
                                              0.2243f * h,
                                              (0.2964f - 0.0774f) * w,
                                              (0.4053f - 0.2243f) * h));
    drawSceneLeds (g, w, h);

    if (! bottomStripBounds.isEmpty())
        drawBottomStatusStrip (g, bottomStripBounds);

    // Exakte Haupt-Panels (0.3181f / 0.5662f, y = 0.2673f, w = 0.2380f, h = 0.2700f)
    const auto leftPanel  = juce::Rectangle<float> (0.3181f * w, 0.2673f * h, 0.2380f * w, 0.2700f * h);
    const auto rightPanel = juce::Rectangle<float> (0.5662f * w, 0.2673f * h, 0.2380f * w, 0.2700f * h);

    switch (currentTab)
    {
        case ActiveTab::Oscillators:
            drawOscillatorSection (g, leftPanel,  0, juce::Colour::fromRGB (0, 240, 255));
            drawOscillatorSection (g, rightPanel, 1, juce::Colour::fromRGB (255, 60, 180));
            break;

        case ActiveTab::Filter:
            drawFilterPanels (g, leftPanel, rightPanel);
            break;

        case ActiveTab::Envelopes:
            drawEnvelopeSection (g, leftPanel,  0, juce::Colour::fromRGB (0, 240, 255));
            drawEnvelopeSection (g, rightPanel, 1, juce::Colour::fromRGB (255, 60, 180));
            break;

        case ActiveTab::FreeEG:
            drawFreeEgPanels (g, leftPanel, rightPanel);
            break;

        case ActiveTab::StepSequencer:
            drawStepSeqPanels (g, leftPanel, rightPanel);
            break;

        case ActiveTab::ModMatrix:
            drawModMatrixPanels (g, leftPanel, rightPanel);
            break;

        case ActiveTab::NumTabs:
        default:
            break;
    }
}

void NeomorphX1AudioProcessorEditor::resized()
{
    const float w = static_cast<float> (getWidth());
    const float h = static_cast<float> (getHeight());

    // Unteres Status-Strip-Panel
    bottomStripBounds = juce::Rectangle<float> (0.2661f * w,
                                                0.9016f * h,
                                                (0.9113f - 0.2661f) * w,
                                                (0.9481f - 0.9016f) * h);

    // 0. Top-Bar Preset-Verwaltung (exakte Koordinaten)
    const float presetLeft   = 0.3815f * w;
    const float presetTop    = 0.0150f * h;
    const float presetRight  = 0.5965f * w;
    const float presetBottom = 0.0530f * h;
    presetBox.setBounds (
        juce::roundToInt (presetLeft),
        juce::roundToInt (presetTop),
        juce::roundToInt (presetRight - presetLeft),
        juce::roundToInt (presetBottom - presetTop)
    );

    const float topBtnW = 0.054f * w;
    const float topBtnH = 0.034f * h;

    savePresetButton.setBounds (
        juce::roundToInt (0.1905f * w - topBtnW * 0.5f),
        juce::roundToInt (0.0360f * h - topBtnH * 0.5f),
        juce::roundToInt (topBtnW),
        juce::roundToInt (topBtnH)
    );

    loadPresetButton.setBounds (
        juce::roundToInt (0.2542f * w - topBtnW * 0.5f),
        juce::roundToInt (0.0360f * h - topBtnH * 0.5f),
        juce::roundToInt (topBtnW),
        juce::roundToInt (topBtnH)
    );

    panicButton.setBounds (
        juce::roundToInt (0.3190f * w - topBtnW * 0.5f),
        juce::roundToInt (0.0360f * h - topBtnH * 0.5f),
        juce::roundToInt (topBtnW),
        juce::roundToInt (topBtnH)
    );

    const float navBtnW = 0.024f * w;
    const float navBtnH = 0.034f * h;

    prevPresetButton.setBounds (
        juce::roundToInt (0.6173f * w - navBtnW * 0.5f),
        juce::roundToInt (0.0340f * h - navBtnH * 0.5f),
        juce::roundToInt (navBtnW),
        juce::roundToInt (navBtnH)
    );

    nextPresetButton.setBounds (
        juce::roundToInt (0.6464f * w - navBtnW * 0.5f),
        juce::roundToInt (0.0340f * h - navBtnH * 0.5f),
        juce::roundToInt (navBtnW),
        juce::roundToInt (navBtnH)
    );

    // 1. 6 Tab-Reiter (exakt deine Breiten)
    const float tabStartX    = 0.305f;
    const float tabY         = 0.208f;
    const float tabH         = 0.036f;
    const float tabWidths[6] = { 0.092f, 0.068f, 0.082f, 0.068f, 0.105f, 0.115f };

    float currentX = tabStartX;
    for (size_t i = 0; i < tabButtons.size(); ++i)
    {
        tabButtons[i].setBounds (
            juce::roundToInt (currentX * w),
            juce::roundToInt (tabY * h),
            juce::roundToInt (tabWidths[i] * w),
            juce::roundToInt (tabH * h)
        );
        currentX += tabWidths[i];
    }

    // 2. 8 Scene-Buttons (exakte Mitten)
    constexpr float sceneBtnX[4] = { 0.4970f, 0.5369f, 0.5768f, 0.6170f };
    const float sceneRow1Y       = 0.6720f * h;
    const float sceneRow2Y       = 0.8190f * h;
    const float sceneBtnW        = 0.032f * w;
    const float sceneBtnH        = 0.056f * h;

    for (int col = 0; col < 4; ++col)
    {
        const float cx = sceneBtnX[col] * w;
        sceneHardwareButtons[static_cast<size_t> (col)].setBounds (
            juce::roundToInt (cx - sceneBtnW * 0.5f),
            juce::roundToInt (sceneRow1Y - sceneBtnH * 0.5f),
            juce::roundToInt (sceneBtnW),
            juce::roundToInt (sceneBtnH)
        );

        sceneHardwareButtons[static_cast<size_t> (col + 4)].setBounds (
            juce::roundToInt (cx - sceneBtnW * 0.5f),
            juce::roundToInt (sceneRow2Y - sceneBtnH * 0.5f),
            juce::roundToInt (sceneBtnW),
            juce::roundToInt (sceneBtnH)
        );
    }

    // 3. 4 LCD-Buttons (exakte Mitten)
    constexpr float lcdBtnX[4] = { 0.0988f, 0.1536f, 0.2083f, 0.2637f };
    const float lcdBtnY        = 0.4880f * h;
    const float lcdBtnW        = 0.044f * w;
    const float lcdBtnH        = 0.046f * h;

    for (int i = 0; i < 4; ++i)
    {
        const float cx = lcdBtnX[i] * w;
        lcdPageButtons[static_cast<size_t> (i)].setBounds (
            juce::roundToInt (cx - lcdBtnW * 0.5f),
            juce::roundToInt (lcdBtnY - lcdBtnH * 0.5f),
            juce::roundToInt (lcdBtnW),
            juce::roundToInt (lcdBtnH)
        );
    }

    // 4. Settings-Icon (exakte Mitte)
    const float iconSize = 0.032f * w;
    settingsButton.setBounds (
        juce::roundToInt (0.9515f * w - iconSize * 0.5f),
        juce::roundToInt (0.0343f * h - iconSize * 0.5f),
        juce::roundToInt (iconSize),
        juce::roundToInt (iconSize)
    );

    // 5. 10er-Nummernblock unten rechts (1..9, CLR, 0, ENT)
    constexpr float kpColX[3] = { 0.6880f, 0.7355f, 0.7830f };
    constexpr float kpRowY[4] = { 0.6320f, 0.6970f, 0.7620f, 0.8270f };
    const float kpBtnW        = 0.0370f * w;
    const float kpBtnH        = 0.0480f * h;

    for (size_t i = 0; i < 12; ++i)
    {
        const size_t row = i / 3;
        const size_t col = i % 3;
        const float cx   = kpColX[col] * w;
        const float cy   = kpRowY[row] * h;

        keypadButtons[i].setBounds (
            juce::roundToInt (cx - kpBtnW * 0.5f),
            juce::roundToInt (cy - kpBtnH * 0.5f),
            juce::roundToInt (kpBtnW),
            juce::roundToInt (kpBtnH)
        );
    }

    // 6. 10 rechte Drehregler (Vol, Pan, Assign 1..8) - exakte Mitten
    constexpr std::array<std::pair<float, float>, 10> knobCoords = {{
        { 0.8490f, 0.2054f }, { 0.9175f, 0.2087f },
        { 0.8496f, 0.3413f }, { 0.9181f, 0.3424f },
        { 0.8496f, 0.4750f }, { 0.9187f, 0.4772f },
        { 0.8502f, 0.6217f }, { 0.9181f, 0.6261f },
        { 0.8490f, 0.7750f }, { 0.9175f, 0.7728f }
    }};

    const float knobDiameter = 0.056f * w;

    for (size_t i = 0; i < matrixKnobs.size(); ++i)
    {
        const float cx = knobCoords[i].first  * w;
        const float cy = knobCoords[i].second * h;
        matrixKnobs[i].setBounds (
            juce::roundToInt (cx - knobDiameter * 0.5f),
            juce::roundToInt (cy - knobDiameter * 0.5f),
            juce::roundToInt (knobDiameter),
            juce::roundToInt (knobDiameter)
        );
    }

    // 7. 3 Scene-Fader (exakte Koordinaten)
    constexpr float faderX[3] = { 0.2823f, 0.3542f, 0.4265f };
    const float faderTopY     = 0.6148f * h - 10.0f;
    const float faderBottomY  = 0.8185f * h + 10.0f;
    const float faderHeight   = faderBottomY - faderTopY;
    const float faderWidth    = 0.036f * w;

    for (size_t i = 0; i < sceneFaders.size(); ++i)
    {
        const float cx = faderX[i] * w;
        sceneFaders[i].setBounds (
            juce::roundToInt (cx - faderWidth * 0.5f),
            juce::roundToInt (faderTopY),
            juce::roundToInt (faderWidth),
            juce::roundToInt (faderHeight)
        );
    }

    // 8. Pitch- & Mod-Wheel (exakte Koordinaten)
    const float wheelTopY    = 0.6290f * h;
    const float wheelBottomY = 0.8444f * h;
    const float wheelHeight  = wheelBottomY - wheelTopY;
    const float wheelWidth   = 0.042f * w;

    pitchWheel.setBounds (
        juce::roundToInt (0.1024f * w - wheelWidth * 0.5f),
        juce::roundToInt (wheelTopY),
        juce::roundToInt (wheelWidth),
        juce::roundToInt (wheelHeight)
    );

    modWheel.setBounds (
        juce::roundToInt (0.1728f * w - wheelWidth * 0.5f),
        juce::roundToInt (wheelTopY),
        juce::roundToInt (wheelWidth),
        juce::roundToInt (wheelHeight)
    );

    // 9. Tab 1: Oscillators Layout (exakte Panel-Koordinaten)
    const float oscPanelX[2] = { 0.3181f, 0.5662f };
    const float panelY       = 0.2673f * h;
    const float panelW       = 0.2380f * w;
    const float panelH       = 0.2700f * h;
    const float oscKnobSize  = 0.034f * w;

    for (size_t i = 0; i < oscControls.size(); ++i)
    {
        auto& osc = oscControls[i];
        const float px = oscPanelX[i] * w;

        osc.waveBox.setBounds (
            juce::roundToInt (px + 0.008f * w),
            juce::roundToInt (panelY + 0.040f * h),
            juce::roundToInt (0.112f * w),
            juce::roundToInt (0.032f * h)
        );

        const float row1Y = panelY + 0.120f * h;
        const float row2Y = panelY + 0.208f * h;

        auto placeKnob = [px, panelW, oscKnobSize] (juce::Slider& k, int col, float cy)
        {
            const float cx = px + panelW * (0.14f + static_cast<float> (col) * 0.24f);
            k.setBounds (
                juce::roundToInt (cx - oscKnobSize * 0.5f),
                juce::roundToInt (cy - oscKnobSize * 0.5f),
                juce::roundToInt (oscKnobSize),
                juce::roundToInt (oscKnobSize)
            );
        };

        placeKnob (osc.octaveKnob,   0, row1Y);
        placeKnob (osc.semiKnob,     1, row1Y);
        placeKnob (osc.detuneKnob,   2, row1Y);
        placeKnob (osc.positionKnob, 3, row1Y);

        placeKnob (osc.morphKnob,    0, row2Y);
        placeKnob (osc.phaseKnob,    1, row2Y);
        placeKnob (osc.panKnob,      2, row2Y);
        placeKnob (osc.levelKnob,    3, row2Y);
    }

    // 10. Tab 2: Filter Layout
    {
        const float leftPx  = oscPanelX[0] * w;
        const float rightPx = oscPanelX[1] * w;

        filterControls.typeBox.setBounds (
            juce::roundToInt (leftPx + 0.008f * w),
            juce::roundToInt (panelY + 0.038f * h),
            juce::roundToInt (0.106f * w),
            juce::roundToInt (0.030f * h)
        );

        filterControls.routingBox.setBounds (
            juce::roundToInt (leftPx + 0.008f * w),
            juce::roundToInt (panelY + 0.074f * h),
            juce::roundToInt (0.106f * w),
            juce::roundToInt (0.030f * h)
        );

        const float mainFltKnobSize = 0.048f * w;
        const float mainFltKnobY    = panelY + 0.196f * h;

        filterControls.cutoffKnob.setBounds (
            juce::roundToInt (leftPx + panelW * 0.30f - mainFltKnobSize * 0.5f),
            juce::roundToInt (mainFltKnobY - mainFltKnobSize * 0.5f),
            juce::roundToInt (mainFltKnobSize),
            juce::roundToInt (mainFltKnobSize)
        );

        filterControls.resonanceKnob.setBounds (
            juce::roundToInt (leftPx + panelW * 0.70f - mainFltKnobSize * 0.5f),
            juce::roundToInt (mainFltKnobY - mainFltKnobSize * 0.5f),
            juce::roundToInt (mainFltKnobSize),
            juce::roundToInt (mainFltKnobSize)
        );

        const float modFltKnobSize = 0.038f * w;
        const float modRow1Y       = panelY + 0.095f * h;
        const float modRow2Y       = panelY + 0.200f * h;

        auto placeRightKnob = [rightPx, panelW, modFltKnobSize] (juce::Slider& k, int col, float cy)
        {
            const float cx = rightPx + panelW * (col == 0 ? 0.30f : 0.70f);
            k.setBounds (
                juce::roundToInt (cx - modFltKnobSize * 0.5f),
                juce::roundToInt (cy - modFltKnobSize * 0.5f),
                juce::roundToInt (modFltKnobSize),
                juce::roundToInt (modFltKnobSize)
            );
        };

        placeRightKnob (filterControls.driveKnob,    0, modRow1Y);
        placeRightKnob (filterControls.envAmtKnob,   1, modRow1Y);
        placeRightKnob (filterControls.keytrackKnob, 0, modRow2Y);
        placeRightKnob (filterControls.velSensKnob,  1, modRow2Y);
    }

    // 11. Tab 3: Envelopes Layout
    const float envKnobSize = 0.036f * w;
    const float envKnobY    = panelY + 0.204f * h;

    for (size_t i = 0; i < envControls.size(); ++i)
    {
        auto& env = envControls[i];
        const float px = oscPanelX[i] * w;

        auto placeEnvKnob = [px, panelW, envKnobSize, envKnobY] (juce::Slider& k, int col)
        {
            const float cx = px + panelW * (0.14f + static_cast<float> (col) * 0.24f);
            k.setBounds (
                juce::roundToInt (cx - envKnobSize * 0.5f),
                juce::roundToInt (envKnobY - envKnobSize * 0.5f),
                juce::roundToInt (envKnobSize),
                juce::roundToInt (envKnobSize)
            );
        };

        placeEnvKnob (env.attackKnob,  0);
        placeEnvKnob (env.decayKnob,   1);
        placeEnvKnob (env.sustainKnob, 2);
        placeEnvKnob (env.releaseKnob, 3);
    }

    // 12. Tab 4: Free EG Layout
    {
        const float leftPx  = oscPanelX[0] * w;
        const float rightPx = oscPanelX[1] * w;

        freeEgControls.canvas.setBounds (
            juce::roundToInt (leftPx + 10.0f),
            juce::roundToInt (panelY + 0.044f * h),
            juce::roundToInt (panelW - 20.0f),
            juce::roundToInt (0.168f * h)
        );

        freeEgControls.targetBox.setBounds (
            juce::roundToInt (leftPx + panelW * 0.34f),
            juce::roundToInt (panelY + 0.222f * h),
            juce::roundToInt (panelW * 0.66f - 10.0f),
            juce::roundToInt (0.032f * h)
        );

        const float boxW = (panelW - 26.0f) * 0.5f;
        freeEgControls.modeBox.setBounds (
            juce::roundToInt (rightPx + 10.0f),
            juce::roundToInt (panelY + 0.056f * h),
            juce::roundToInt (boxW),
            juce::roundToInt (0.034f * h)
        );

        freeEgControls.syncBox.setBounds (
            juce::roundToInt (rightPx + 16.0f + boxW),
            juce::roundToInt (panelY + 0.056f * h),
            juce::roundToInt (boxW),
            juce::roundToInt (0.034f * h)
        );

        const float fegKnobSize = 0.040f * w;
        const float fegKnobY    = panelY + 0.186f * h;

        auto placeFegKnob = [rightPx, panelW, fegKnobSize, fegKnobY] (juce::Slider& k, int col)
        {
            const float cx = rightPx + panelW * (0.20f + static_cast<float> (col) * 0.30f);
            k.setBounds (
                juce::roundToInt (cx - fegKnobSize * 0.5f),
                juce::roundToInt (fegKnobY - fegKnobSize * 0.5f),
                juce::roundToInt (fegKnobSize),
                juce::roundToInt (fegKnobSize)
            );
        };

        placeFegKnob (freeEgControls.timeKnob,   0);
        placeFegKnob (freeEgControls.amountKnob, 1);
        placeFegKnob (freeEgControls.smoothKnob, 2);
    }

    // 13. Tab 5: Step Sequencer Layout
    {
        const float leftPx  = oscPanelX[0] * w;
        const float rightPx = oscPanelX[1] * w;

        stepSeqControls.canvas.setBounds (
            juce::roundToInt (leftPx + 10.0f),
            juce::roundToInt (panelY + 0.044f * h),
            juce::roundToInt (panelW - 20.0f),
            juce::roundToInt (0.168f * h)
        );

        const float boxW = (panelW - 26.0f) * 0.5f;
        stepSeqControls.modeBox.setBounds (
            juce::roundToInt (leftPx + 10.0f),
            juce::roundToInt (panelY + 0.222f * h),
            juce::roundToInt (boxW),
            juce::roundToInt (0.032f * h)
        );

        stepSeqControls.lengthBox.setBounds (
            juce::roundToInt (leftPx + 16.0f + boxW),
            juce::roundToInt (panelY + 0.222f * h),
            juce::roundToInt (boxW),
            juce::roundToInt (0.032f * h)
        );

        stepSeqControls.rateBox.setBounds (
            juce::roundToInt (rightPx + panelW * 0.30f),
            juce::roundToInt (panelY + 0.056f * h),
            juce::roundToInt (panelW * 0.70f - 10.0f),
            juce::roundToInt (0.034f * h)
        );

        const float seqKnobSize = 0.040f * w;
        const float seqKnobY    = panelY + 0.186f * h;

        auto placeSeqKnob = [rightPx, panelW, seqKnobSize, seqKnobY] (juce::Slider& k, int col)
        {
            const float cx = rightPx + panelW * (0.20f + static_cast<float> (col) * 0.30f);
            k.setBounds (
                juce::roundToInt (cx - seqKnobSize * 0.5f),
                juce::roundToInt (seqKnobY - seqKnobSize * 0.5f),
                juce::roundToInt (seqKnobSize),
                juce::roundToInt (seqKnobSize)
            );
        };

        placeSeqKnob (stepSeqControls.gateKnob,   0);
        placeSeqKnob (stepSeqControls.swingKnob,  1);
        placeSeqKnob (stepSeqControls.amountKnob, 2);
    }

    // 14. Tab 6: Mod Matrix & LFO 1 Layout
    {
        const float leftPx  = oscPanelX[0] * w;
        const float rightPx = oscPanelX[1] * w;

        const float rowStartY = panelY + 0.045f * h;
        const float rowStepY  = 0.054f * h;
        const float boxH      = 0.031f * h;
        const float boxW      = (panelW - 20.0f - 0.032f * w) * 0.5f;
        const float amtKnobSz = 0.025f * w;

        for (size_t m = 0; m < 4; ++m)
        {
            auto& slot = modMatrixControls.slots[m];
            const float ry = rowStartY + static_cast<float> (m) * rowStepY;

            slot.srcBox.setBounds (
                juce::roundToInt (leftPx + 8.0f),
                juce::roundToInt (ry),
                juce::roundToInt (boxW),
                juce::roundToInt (boxH)
            );

            slot.dstBox.setBounds (
                juce::roundToInt (leftPx + 12.0f + boxW),
                juce::roundToInt (ry),
                juce::roundToInt (boxW),
                juce::roundToInt (boxH)
            );

            const float knobCx = leftPx + panelW - 8.0f - amtKnobSz * 0.5f;
            const float knobCy = ry + boxH * 0.5f;
            slot.amtKnob.setBounds (
                juce::roundToInt (knobCx - amtKnobSz * 0.5f),
                juce::roundToInt (knobCy - amtKnobSz * 0.5f),
                juce::roundToInt (amtKnobSz),
                juce::roundToInt (amtKnobSz)
            );
        }

        modMatrixControls.lfoSyncBox.setBounds (
            juce::roundToInt (rightPx + panelW * 0.36f),
            juce::roundToInt (panelY + 0.050f * h),
            juce::roundToInt (panelW * 0.64f - 10.0f),
            juce::roundToInt (0.032f * h)
        );

        const float lfoKnobSize = 0.036f * w;
        const float lfoKnobY    = panelY + 0.156f * h;

        auto placeLfoKnob = [rightPx, panelW, lfoKnobSize, lfoKnobY] (juce::Slider& k, int col)
        {
            const float cx = rightPx + panelW * (0.20f + static_cast<float> (col) * 0.30f);
            k.setBounds (
                juce::roundToInt (cx - lfoKnobSize * 0.5f),
                juce::roundToInt (lfoKnobY - lfoKnobSize * 0.5f),
                juce::roundToInt (lfoKnobSize),
                juce::roundToInt (lfoKnobSize)
            );
        };

        placeLfoKnob (modMatrixControls.lfoRateKnob,  0);
        placeLfoKnob (modMatrixControls.lfoFadeKnob,  1);
        placeLfoKnob (modMatrixControls.lfoDepthKnob, 2);
    }
}

void NeomorphX1AudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isLeftButtonDown() && scopeClickArea.contains (event.position))
    {
        const int nextMode = (static_cast<int> (bottomStripMode) + 1) % static_cast<int> (BottomStripMode::NumModes);
        bottomStripMode = static_cast<BottomStripMode> (nextMode);
        repaint (bottomStripBounds.toNearestInt().expanded (2));
        return;
    }

    if (event.mods.isRightButtonDown())
    {
        const float relX = event.position.x / static_cast<float> (getWidth());
        const float relY = event.position.y / static_cast<float> (getHeight());

        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            "NeomorphX1 Koordinaten-Helfer",
            juce::String::formatted ("Pixel: %d, %d\nRelativ: %.4ff, %.4ff",
                                     event.x, event.y, relX, relY));
    }
}
