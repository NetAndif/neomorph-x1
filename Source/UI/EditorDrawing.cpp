#include "../PluginEditor.h"
#include "../PluginProcessor.h"

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
                else { const int cc = audioProcessor.getAssignedMidiCC (idx - 2); labelStr = (cc >= 0) ? ("A" + juce::String (idx - 1) + ":CC" + juce::String (cc)) : ("A" + juce::String (idx - 1) + ":" + juce::String (pct)); }

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
    const float ledY        = 0.6180f * h;
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

void NeomorphX1AudioProcessorEditor::drawAssignMidiLearnBadges (juce::Graphics& g)
{
    const int learningSlot = audioProcessor.getMidiLearnTargetSlot();
    const auto amber = juce::Colour::fromRGB (255, 205, 50);
    const auto cyan  = juce::Colour::fromRGB (0, 240, 255);

    for (int assignIdx = 0; assignIdx < NeomorphX1AudioProcessor::numAssignKnobs; ++assignIdx)
    {
        const auto& knob = matrixKnobs[static_cast<size_t> (assignIdx + 2)];
        const auto kb    = knob.getBounds().toFloat();
        const int cc     = audioProcessor.getAssignedMidiCC (assignIdx);

        if (learningSlot == assignIdx)
        {
            // Pulsierender Rahmen im MIDI-Learn-Modus
            g.setColour (amber.withAlpha (0.85f));
            g.drawEllipse (kb.reduced (1.0f), 2.0f);

            auto badge = juce::Rectangle<float> (kb.getCentreX() - 22.0f, kb.getBottom() - 12.0f, 44.0f, 12.0f);
            g.setColour (juce::Colour::fromRGB (10, 14, 22).withAlpha (0.92f));
            g.fillRoundedRectangle (badge, 3.0f);
            g.setColour (amber);
            g.drawRoundedRectangle (badge, 3.0f, 1.0f);
            g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
            g.drawText ("LEARN...", badge, juce::Justification::centred);
        }
		else if (cc >= 0)
        {
            const bool waitingPickup = audioProcessor.isAssignWaitingForPickup (assignIdx);
            const auto badgeCol      = waitingPickup ? amber : cyan;

            auto badge = juce::Rectangle<float> (kb.getCentreX() - 22.0f, kb.getBottom() - 11.0f, 44.0f, 11.0f);
            g.setColour (juce::Colour::fromRGB (10, 14, 22).withAlpha (0.88f));
            g.fillRoundedRectangle (badge, 2.5f);
            g.setColour (badgeCol.withAlpha (0.80f));
            g.drawRoundedRectangle (badge, 2.5f, 0.9f);
            g.setFont (juce::FontOptions (9.0f, juce::Font::bold));

            const auto text = waitingPickup ? ("CC" + juce::String (cc) + " PK")
                                            : ("CC " + juce::String (cc));
            g.drawText (text, badge, juce::Justification::centred);
        }
    }
}
