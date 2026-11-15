#include "PluginProcessor.h"
#include "PluginEditor.h"

NeomorphX1AudioProcessorEditor::NeomorphX1AudioProcessorEditor (NeomorphX1AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&cyberpunkLookAndFeel);

    // Haupt-Hintergrund aus BinaryData laden
    backgroundImage = juce::ImageCache::getFromMemory (BinaryData::background_main_png,
                                                       BinaryData::background_main_pngSize);

    // Die 6 Tab-Overlays laden
    tabImages[0] = juce::ImageCache::getFromMemory (BinaryData::tab_oscillators_png, BinaryData::tab_oscillators_pngSize);
    tabImages[1] = juce::ImageCache::getFromMemory (BinaryData::tab_filter_png,      BinaryData::tab_filter_pngSize);
    tabImages[2] = juce::ImageCache::getFromMemory (BinaryData::tab_envelopes_png,   BinaryData::tab_envelopes_pngSize);
    tabImages[3] = juce::ImageCache::getFromMemory (BinaryData::tab_free_eg_png,     BinaryData::tab_free_eg_pngSize);
    tabImages[4] = juce::ImageCache::getFromMemory (BinaryData::tab_step_seq_png,    BinaryData::tab_step_seq_pngSize);
    tabImages[5] = juce::ImageCache::getFromMemory (BinaryData::tab_mod_matrix_png,  BinaryData::tab_mod_matrix_pngSize);

    // Unsichtbare Klick-Buttons über die 6 Tab-Reiter im Bild legen
    for (size_t i = 0; i < tabButtons.size(); ++i)
    {
        auto& btn = tabButtons[i];
        btn.setAlpha (0.0f);
        btn.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        btn.onClick = [this, i] { setActiveTab (static_cast<ActiveTab> (i)); };
        addAndMakeVisible (btn);
    }

    // Unsichtbarer Klick-Button über dem Settings-/Menü-Icon oben rechts
    settingsButton.setAlpha (0.0f);
    settingsButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    settingsButton.onClick = [this] { showSettingsMenu(); };
    addAndMakeVisible (settingsButton);

    // 10 Matrix-Drehregler konfigurieren & an APVTS binden
    for (size_t i = 0; i < matrixKnobs.size(); ++i)
    {
        auto& knob = matrixKnobs[i];
        knob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour::fromRGB (0, 240, 255));

        const auto paramID = "knob_" + juce::String (static_cast<int> (i + 1));
        if (auto* param = audioProcessor.apvts.getParameter (paramID))
            knob.setDoubleClickReturnValue (true, param->getDefaultValue());

        knob.onValueChange = [this, i]
        {
            const int pct = juce::roundToInt (matrixKnobs[i].getValue() * 100.0);
            hudStatusText = juce::String::formatted ("ASSIGN %d: %d%%", static_cast<int> (i + 1), pct);
            repaint();
        };

        addAndMakeVisible (knob);

        knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, paramID, knob);
    }

    // 3 Scene-Fader konfigurieren & an APVTS binden
    const char* faderParamIDs[3] = { "scene_1", "scene_morph", "scene_2" };
    const char* faderLabels[3]   = { "SCENE 1", "MORPH", "SCENE 2" };
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

        if (auto* param = audioProcessor.apvts.getParameter (faderParamIDs[i]))
            fader.setDoubleClickReturnValue (true, param->getDefaultValue());

        const juce::String label (faderLabels[i]);
        fader.onValueChange = [this, i, label]
        {
            const int pct = juce::roundToInt (sceneFaders[i].getValue() * 100.0);
            hudStatusText = label + ": " + juce::String (pct) + "%";
            repaint();
        };

        addAndMakeVisible (fader);

        faderAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, faderParamIDs[i], fader);
    }

    // Pitch-Wheel (bipolar -1.0 .. +1.0, federt zur Mitte zurück)
    pitchWheel.setSliderStyle (juce::Slider::LinearVertical);
    pitchWheel.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pitchWheel.getProperties().set ("isWheel", true);
    pitchWheel.setColour (juce::Slider::thumbColourId, juce::Colour::fromRGB (0, 240, 255));
    pitchWheel.springBackEnabled = true;
    pitchWheel.restingValue      = 0.0;
    pitchWheel.onValueChange = [this]
    {
        const int pct = juce::roundToInt (pitchWheel.getValue() * 100.0);
        hudStatusText = juce::String::formatted ("PITCH BEND: %+d%%", pct);
        repaint();
    };
    addAndMakeVisible (pitchWheel);
    pitchWheelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, "pitch_wheel", pitchWheel);

    // Mod-Wheel (unipolar 0.0 .. 1.0, bleibt stehen)
    modWheel.setSliderStyle (juce::Slider::LinearVertical);
    modWheel.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    modWheel.getProperties().set ("isWheel", true);
    modWheel.setColour (juce::Slider::thumbColourId, juce::Colour::fromRGB (255, 60, 180));
    modWheel.setDoubleClickReturnValue (true, 0.0);
    modWheel.onValueChange = [this]
    {
        const int pct = juce::roundToInt (modWheel.getValue() * 100.0);
        hudStatusText = juce::String::formatted ("MOD WHEEL: %d%%", pct);
        repaint();
    };
    addAndMakeVisible (modWheel);
    modWheelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, "mod_wheel", modWheel);

    // --- Tab 1: Oscillators Controls initialisieren (OSC 1 links, OSC 2 rechts) ---
    const juce::StringArray waveformItems = {
        "Basic Analog", "PWM / Pulse", "Formant", "Digital Fold", "SuperSaw", "Vocal Morph"
    };

    for (size_t i = 0; i < oscControls.size(); ++i)
    {
        auto& osc = oscControls[i];
        const int oscNum  = static_cast<int> (i + 1);
        const auto prefix = "osc" + juce::String (oscNum) + "_";
        const auto accent = (i == 0) ? juce::Colour::fromRGB (0, 240, 255)
                                     : juce::Colour::fromRGB (255, 60, 180);

        // Wavetable Dropdown
        osc.waveBox.addItemList (waveformItems, 1);
        osc.waveBox.setJustificationType (juce::Justification::centredLeft);
        osc.waveBox.onChange = [this, oscNum, i]
        {
            hudStatusText = "OSC " + juce::String (oscNum) + " TABLE: " + oscControls[i].waveBox.getText();
            repaint();
        };
        addChildComponent (osc.waveBox);
        osc.waveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            audioProcessor.apvts, prefix + "wave", osc.waveBox);

        auto setupOscKnob = [this, accent] (juce::Slider& slider, const juce::String& paramID,
                                            bool bipolar, std::function<juce::String (double)> formatHud)
        {
            slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
            slider.getProperties().set ("isBipolar", bipolar);

            if (auto* param = audioProcessor.apvts.getParameter (paramID))
                slider.setDoubleClickReturnValue (true, param->getDefaultValue());

            slider.onValueChange = [this, &slider, formatHud]
            {
                hudStatusText = formatHud (slider.getValue());
                repaint();
            };

            addChildComponent (slider);
        };

        // Reihe 1: OCT, SEMI, DETUNE, POSITION
        setupOscKnob (osc.octaveKnob, prefix + "octave", true, [oscNum] (double v) {
            return juce::String::formatted ("OSC %d OCTAVE: %+d", oscNum, juce::roundToInt (v));
        });
        osc.octaveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, prefix + "octave", osc.octaveKnob);

        setupOscKnob (osc.semiKnob, prefix + "semi", true, [oscNum] (double v) {
            return juce::String::formatted ("OSC %d SEMI: %+d st", oscNum, juce::roundToInt (v));
        });
        osc.semiAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, prefix + "semi", osc.semiKnob);

        setupOscKnob (osc.detuneKnob, prefix + "detune", true, [oscNum] (double v) {
            return juce::String::formatted ("OSC %d DETUNE: %+.1f ct", oscNum, v);
        });
        osc.detuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, prefix + "detune", osc.detuneKnob);

        setupOscKnob (osc.positionKnob, prefix + "position", false, [oscNum] (double v) {
            return juce::String::formatted ("OSC %d POSITION: %d%%", oscNum, juce::roundToInt (v * 100.0));
        });
        osc.positionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, prefix + "position", osc.positionKnob);

        // Reihe 2: MORPH, PHASE, PAN, LEVEL
        setupOscKnob (osc.morphKnob, prefix + "morph", false, [oscNum] (double v) {
            return juce::String::formatted ("OSC %d MORPH: %d%%", oscNum, juce::roundToInt (v * 100.0));
        });
        osc.morphAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, prefix + "morph", osc.morphKnob);

        setupOscKnob (osc.phaseKnob, prefix + "phase", false, [oscNum] (double v) {
            return juce::String::formatted ("OSC %d PHASE: %d deg", oscNum, juce::roundToInt (v));
        });
        osc.phaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, prefix + "phase", osc.phaseKnob);

        setupOscKnob (osc.panKnob, prefix + "pan", true, [oscNum] (double v) {
            const int pct = juce::roundToInt (v * 100.0);
            if (pct == 0) return juce::String::formatted ("OSC %d PAN: CENTER", oscNum);
            return juce::String::formatted ("OSC %d PAN: %d%% %s", oscNum, std::abs (pct), pct < 0 ? "L" : "R");
        });
        osc.panAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, prefix + "pan", osc.panKnob);

        setupOscKnob (osc.levelKnob, prefix + "level", false, [oscNum] (double v) {
            return juce::String::formatted ("OSC %d LEVEL: %d%%", oscNum, juce::roundToInt (v * 100.0));
        });
        osc.levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, prefix + "level", osc.levelKnob);
    }

    updateTabVisibility();

    // Standardgröße (16:9 Verhältnis passend zum Artwork) + Skalierbarkeit
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (16.0 / 9.0);
    setResizeLimits (800, 450, 2200, 1238);
    setSize (1120, 630);
}

NeomorphX1AudioProcessorEditor::~NeomorphX1AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
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
}

void NeomorphX1AudioProcessorEditor::showSettingsMenu()
{
    juce::PopupMenu menu;

    menu.addSectionHeader ("NEOMORPH X1 // SYSTEM");
    menu.addItem (1, "Init Patch (Reset)");
    menu.addSeparator();

    juce::PopupMenu scaleMenu;
    scaleMenu.addItem (10, "80%  (896 x 504)");
    scaleMenu.addItem (11, "100% (1120 x 630)");
    scaleMenu.addItem (12, "125% (1400 x 788)");
    scaleMenu.addItem (13, "150% (1680 x 945)");
    menu.addSubMenu ("UI Scale", scaleMenu);

    if (juce::StandalonePluginHolder::getInstance() != nullptr)
    {
        menu.addSeparator();
        menu.addItem (20, "Audio / MIDI Settings...");
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton),
                        [this] (int result)
    {
        switch (result)
        {
            case 1:
                for (auto* param : audioProcessor.getParameters())
                    param->setValueNotifyingHost (param->getDefaultValue());
                hudStatusText = "INIT PATCH LOADED";
                repaint();
                break;
            case 10: setSize (896, 504);   break;
            case 11: setSize (1120, 630);  break;
            case 12: setSize (1400, 788);  break;
            case 13: setSize (1680, 945);  break;
            case 20:
                if (auto* holder = juce::StandalonePluginHolder::getInstance())
                    holder->showAudioSettingsDialog();
                break;
            default:
                break;
        }
    });
}

void NeomorphX1AudioProcessorEditor::drawOscillatorSection (juce::Graphics& g,
                                                            juce::Rectangle<float> panelBounds,
                                                            int oscIndex,
                                                            juce::Colour accentColour)
{
    // 1. Halbtransparenter Sektions-Kasten
    g.setColour (juce::Colour::fromRGB (10, 15, 24).withAlpha (0.74f));
    g.fillRoundedRectangle (panelBounds, 6.0f);

    g.setColour (accentColour.withAlpha (0.38f));
    g.drawRoundedRectangle (panelBounds, 6.0f, 1.0f);

    // 2. Sektions-Überschrift oben links
    auto headerArea = panelBounds.reduced (10.0f, 5.0f).removeFromTop (16.0f);
    g.setColour (accentColour);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("OSCILLATOR " + juce::String (oscIndex + 1), headerArea, juce::Justification::centredLeft);

    // 3. Live-Wavetable-Scope rechts neben dem Dropdown
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

    // Wellenform mit Position (Wavetable-Scan), Morph & Phase berechnen
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
        const float t    = std::fmod (rawT + phaseNorm, 1.0f); // Startphase verschieben
        float sample     = 0.0f;

        switch (waveType)
        {
            case 0: // Basic Analog: Position morpht stufenlos Sine -> Triangle -> Saw -> Square
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
            case 1: // PWM / Pulse: Position steuert Pulsbreite
            {
                const float pw = juce::jmap (pos, 0.08f, 0.92f);
                sample = (t < pw) ? 0.85f : -0.85f;
                break;
            }
            case 2: // Formant: Position verschiebt Resonanz-Obertöne
            {
                const float harmonic = 2.0f + pos * 7.0f;
                const float window   = std::sin (t * juce::MathConstants<float>::pi);
                sample = std::sin (t * juce::MathConstants<float>::twoPi * harmonic) * window;
                break;
            }
            case 3: // Digital Fold: Position steuert Wavefolder-Intensität
            {
                const float driven = std::sin (t * juce::MathConstants<float>::twoPi) * (1.0f + pos * 3.5f);
                sample = std::sin (driven * juce::MathConstants<float>::halfPi);
                break;
            }
            case 4: // SuperSaw: Position blendet verstimmte Sägezähne ein
            {
                const float s1 = 2.0f * t - 1.0f;
                const float s2 = 2.0f * std::fmod (t * 1.03f + 0.17f, 1.0f) - 1.0f;
                const float s3 = 2.0f * std::fmod (t * 0.97f + 0.41f, 1.0f) - 1.0f;
                sample = s1 * (1.0f - pos * 0.45f) + (s2 + s3) * (pos * 0.32f);
                break;
            }
            default: // Vocal Morph
            {
                sample = 0.6f * std::sin (t * juce::MathConstants<float>::twoPi)
                       + 0.4f * std::cos (t * juce::MathConstants<float>::twoPi * (2.0f + pos * 5.0f));
                break;
            }
        }

        // Morph-Regler wendet zusätzliches Harmonic-Folding / Sync-Shaping an
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

    // 4. Regler-Labels unter allen 8 Knobs zeichnen
    auto drawKnobLabel = [&g] (const juce::Slider& knob, const juce::String& text)
    {
        const auto kb = knob.getBounds().toFloat();
        auto labelRect = juce::Rectangle<float> (kb.getX() - 6.0f, kb.getBottom() - 2.0f, kb.getWidth() + 12.0f, 12.0f);
        g.setColour (juce::Colour::fromRGB (170, 200, 225));
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
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

void NeomorphX1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (10, 14, 22));

    auto bounds = getLocalBounds().toFloat();

    // 1. Haupt-Gehäuse zeichnen
    if (backgroundImage.isValid())
        g.drawImage (backgroundImage, bounds, juce::RectanglePlacement::stretchToFit);

    // 2. Aktives Tab-Overlay im Mittelfenster zeichnen
    const auto& activeOverlay = tabImages[static_cast<size_t> (currentTab)];
    if (activeOverlay.isValid())
        g.drawImage (activeOverlay, bounds, juce::RectanglePlacement::stretchToFit);

    // 3. Dynamische Tab-Grafiken (Oscillator-Sektionen & Live-Scopes)
    if (currentTab == ActiveTab::Oscillators)
    {
        const float w = bounds.getWidth();
        const float h = bounds.getHeight();

        // Links: OSC 1 (Cyan), Rechts: OSC 2 (Neon-Magenta)
        drawOscillatorSection (g,
                               juce::Rectangle<float> (0.255f * w, 0.268f * h, 0.252f * w, 0.285f * h),
                               0,
                               juce::Colour::fromRGB (0, 240, 255));

        drawOscillatorSection (g,
                               juce::Rectangle<float> (0.520f * w, 0.268f * h, 0.252f * w, 0.285f * h),
                               1,
                               juce::Colour::fromRGB (255, 60, 180));
    }

    // 4. HUD-Status / Koordinaten-Anzeige unten links
    if (hudStatusText.isNotEmpty())
    {
        auto infoBox = juce::Rectangle<float> (12.0f, bounds.getHeight() - 32.0f, 340.0f, 22.0f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.fillRoundedRectangle (infoBox, 4.0f);
        g.setColour (juce::Colour::fromRGB (0, 240, 255));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (hudStatusText, infoBox, juce::Justification::centred);
    }
}

void NeomorphX1AudioProcessorEditor::resized()
{
    const float w = static_cast<float> (getWidth());
    const float h = static_cast<float> (getHeight());

    // 1. Relative Positionen der 6 Tab-Reiter
    const float tabStartX = 0.305f;
    const float tabY      = 0.208f;
    const float tabH      = 0.036f;
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

    // 2. Settings-Icon oben rechts
    const float iconSize = 0.032f * w;
    settingsButton.setBounds (
        juce::roundToInt (0.9515f * w - iconSize * 0.5f),
        juce::roundToInt (0.0343f * h - iconSize * 0.5f),
        juce::roundToInt (iconSize),
        juce::roundToInt (iconSize)
    );

    // 3. Die 10 Matrix-Drehregler
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

    // 4. Die 3 Scene-Fader
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

    // 5. Pitch- & Mod-Wheel
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

    // 6. Tab 1: Oscillators Layout (links OSC 1 ab X = 0.255f, rechts OSC 2 ab X = 0.520f)
    const float oscPanelX[2] = { 0.255f, 0.520f };
    const float panelY       = 0.268f * h;
    const float panelW       = 0.252f * w;
    const float oscKnobSize  = 0.036f * w;

    for (size_t i = 0; i < oscControls.size(); ++i)
    {
        auto& osc = oscControls[i];
        const float px = oscPanelX[i] * w;

        // Oben links im Panel: Wavetable-Dropdown
        osc.waveBox.setBounds (
            juce::roundToInt (px + 0.010f * w),
            juce::roundToInt (panelY + 0.044f * h),
            juce::roundToInt (0.118f * w),
            juce::roundToInt (0.034f * h)
        );

        // Darunter: 2 Reihen à 4 Regler (OCT, SEMI, DETUNE, POS / MORPH, PHASE, PAN, LEVEL)
        const float row1Y = panelY + 0.128f * h;
        const float row2Y = panelY + 0.222f * h;

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
}

void NeomorphX1AudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        const float relX = static_cast<float> (event.x) / static_cast<float> (getWidth());
        const float relY = static_cast<float> (event.y) / static_cast<float> (getHeight());

        const auto coordPair = juce::String::formatted ("{ %.4ff, %.4ff },", relX, relY);
        juce::SystemClipboard::copyTextToClipboard (coordPair);

        hudStatusText = "Kopiert: " + coordPair;
        repaint();
    }
}
