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

    // 8 unsichtbare Klick-Buttons über der 2x4 Button-Matrix rechts neben den Scene-Fadern
    for (size_t i = 0; i < sceneHardwareButtons.size(); ++i)
    {
        auto& btn = sceneHardwareButtons[i];
        btn.setAlpha (0.0f);
        btn.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        btn.onClick = [this, i] { handleSceneButton (static_cast<int> (i)); };
        addAndMakeVisible (btn);
    }

    // 4 unsichtbare Klick-Buttons unter dem LCD-Display
    for (size_t i = 0; i < lcdPageButtons.size(); ++i)
    {
        auto& btn = lcdPageButtons[i];
        btn.setAlpha (0.0f);
        btn.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        btn.onClick = [this, i]
        {
            currentLcdPage = static_cast<LcdPage> (i);
            showLcdNotify  = false;
            stopTimer();
            repaint();
        };
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
            triggerLcdNotification ("MATRIX ASSIGN " + juce::String (static_cast<int> (i + 1)),
                                    juce::String (pct) + "%");
        };

        addAndMakeVisible (knob);

        knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, paramID, knob);
    }

    // 3 Scene-Fader konfigurieren & an APVTS binden
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

        if (auto* param = audioProcessor.apvts.getParameter (faderParamIDs[i]))
            fader.setDoubleClickReturnValue (true, param->getDefaultValue());

        const juce::String label (faderLabels[i]);
        fader.onValueChange = [this, i, label]
        {
            const int pct = juce::roundToInt (sceneFaders[i].getValue() * 100.0);
            triggerLcdNotification (label, juce::String (pct) + "%");
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
        triggerLcdNotification ("PITCH BEND", juce::String::formatted ("%+d%%", pct));
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
        triggerLcdNotification ("MODULATION WHEEL", juce::String (pct) + "%");
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
        const auto accent = (i == 0) ? juce::Colour::fromRGB (0, 240, 255)
                                     : juce::Colour::fromRGB (255, 60, 180);

        // Wavetable Dropdown
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
                if (sceneLink)
                {
                    const int otherScene = (editingScene == 1) ? 2 : 1;
                    const auto srcID = "s" + juce::String (editingScene) + "_osc" + juce::String (oscNum) + "_" + paramSuffix;
                    const auto dstID = "s" + juce::String (otherScene)   + "_osc" + juce::String (oscNum) + "_" + paramSuffix;
                    if (auto* srcP = audioProcessor.apvts.getParameter (srcID))
                        if (auto* dstP = audioProcessor.apvts.getParameter (dstID))
                            dstP->setValueNotifyingHost (srcP->getValue());
                }

                const double v = slider.getValue();
                triggerLcdNotification (formatTitle (v), formatVal (v));
            };

            addChildComponent (slider);
        };

        // Reihe 1: OCT, SEMI, DETUNE, POSITION
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

        // Reihe 2: MORPH, PHASE, PAN, LEVEL
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

    // Attachments an die aktive Edit-Scene (Scene 1) binden
    bindOscillatorAttachments();
    updateTabVisibility();

    // Standardgröße (16:9 Verhältnis passend zum Artwork) + Skalierbarkeit
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (16.0 / 9.0);
    setResizeLimits (800, 450, 2200, 1238);
    setSize (1120, 630);
}

NeomorphX1AudioProcessorEditor::~NeomorphX1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void NeomorphX1AudioProcessorEditor::timerCallback()
{
    stopTimer();
    showLcdNotify = false;
    repaint();
}

void NeomorphX1AudioProcessorEditor::triggerLcdNotification (const juce::String& title, const juce::String& value)
{
    lcdNotifyTitle = title;
    lcdNotifyValue = value;
    showLcdNotify  = true;
    startTimer (1600); // Nach 1,6 Sekunden wieder auf die Haupt-LCD-Anzeige zurückschalten
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
        // Wenn bereits aktiv, LCD trotzdem kurz zur Bestätigung aktualisieren
        showLcdNotify = false;
        stopTimer();
        repaint();
        return;
    }

    editingScene = newScene;
    bindOscillatorAttachments();

    // Direkt die große SCENE 01 / SCENE 02 Ansicht im LCD zeigen
    showLcdNotify = false;
    stopTimer();
    repaint();
}

void NeomorphX1AudioProcessorEditor::handleSceneButton (int buttonIndex)
{
    switch (buttonIndex)
    {
        case 0: // Obere Reihe 1: SCENE 1 auswählen
            setEditingScene (1);
            break;

        case 1: // Obere Reihe 2: SCENE 2 auswählen
            setEditingScene (2);
            break;

        case 2: // Obere Reihe 3: SCENE SOLO umschalten
            sceneSolo = ! sceneSolo;
            triggerLcdNotification ("SCENE SOLO", sceneSolo ? "ACTIVE (S" + juce::String (editingScene) + ")" : "OFF");
            break;

        case 3: // Obere Reihe 4: SCENE LINK EDIT umschalten
            sceneLink = ! sceneLink;
            triggerLcdNotification ("SCENE LINK EDIT", sceneLink ? "ENABLED (S1+S2)" : "OFF");
            break;

        case 4: // Untere Reihe 1: COPY aktive Scene -> andere Scene
        {
            const int targetScene = (editingScene == 1) ? 2 : 1;
            audioProcessor.copySceneParameters (editingScene, targetScene);
            triggerLcdNotification ("SCENE COPY",
                                    juce::String::formatted ("S%d -> S%d DONE", editingScene, targetScene));
            break;
        }

        case 5: // Untere Reihe 2: SWAP Scene 1 <-> Scene 2
            audioProcessor.swapSceneParameters();
            triggerLcdNotification ("SCENE SWAP", "S1 <-> S2 SWAPPED");
            break;

        case 6: // Untere Reihe 3: INIT aktive Scene
            audioProcessor.resetSceneParameters (editingScene);
            triggerLcdNotification ("SCENE INIT", "SCENE " + juce::String (editingScene) + " RESET");
            break;

        case 7: // Untere Reihe 4: RANDOMIZE aktive Scene
            audioProcessor.randomizeSceneParameters (editingScene);
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
    menu.addItem (1, "Init Full Patch (Reset)");
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
                triggerLcdNotification ("SYSTEM", "INIT PATCH LOADED");
                break;
            case 2:
                audioProcessor.copySceneParameters (1, 2);
                triggerLcdNotification ("SCENE COPY", "S1 -> S2 DONE");
                break;
            case 3:
                audioProcessor.copySceneParameters (2, 1);
                triggerLcdNotification ("SCENE COPY", "S2 -> S1 DONE");
                break;
            case 4:
                audioProcessor.swapSceneParameters();
                triggerLcdNotification ("SCENE SWAP", "S1 <-> S2 SWAPPED");
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

void NeomorphX1AudioProcessorEditor::drawLcdScreen (juce::Graphics& g, juce::Rectangle<float> lcdBounds)
{
    const auto screen = lcdBounds.reduced (6.0f, 5.0f);
    const auto sceneAccent = (editingScene == 1)
                                 ? juce::Colour::fromRGB (0, 240, 255)
                                 : juce::Colour::fromRGB (255, 60, 180);

    // 1. Subtile LCD-Scanlines für Cyberpunk-Display-Optik
    g.setColour (juce::Colour::fromRGB (0, 240, 255).withAlpha (0.045f));
    for (float y = screen.getY(); y < screen.getBottom(); y += 3.0f)
        g.drawHorizontalLine (juce::roundToInt (y), screen.getX(), screen.getRight());

    // 2. Obere Statuszeile im LCD (Page-Name links, Status-Flags rechts)
    const char* pageNames[4] = { "PATCH", "MORPH", "ASSIGN", "SYSTEM" };
    auto topBar = screen.removeFromTop (15.0f);

    g.setColour (juce::Colour::fromRGB (130, 175, 205));
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
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

    // Trennlinie unter Top-Bar
    g.setColour (sceneAccent.withAlpha (0.28f));
    g.drawHorizontalLine (juce::roundToInt (screen.getY() + 1.0f), screen.getX(), screen.getRight());
    screen.removeFromTop (4.0f);

    // 3. Wenn gerade ein Regler bewegt wurde: temporäre Wert-Einblendung im LCD
    if (showLcdNotify)
    {
        auto titleArea = screen.removeFromTop (screen.getHeight() * 0.38f);
        g.setColour (juce::Colour::fromRGB (170, 215, 240));
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (lcdNotifyTitle, titleArea, juce::Justification::centred);

        g.setColour (sceneAccent);
        g.setFont (juce::FontOptions (21.0f, juce::Font::bold));
        g.drawText (lcdNotifyValue, screen, juce::Justification::centred);
        return;
    }

    // 4. Reguläre LCD-Pages (über die 4 Buttons unter dem LCD umschaltbar)
    switch (currentLcdPage)
    {
        case LcdPage::Patch:
        {
            // Große Anzeige der aktiven Edit-Scene
            auto mainSceneRect = screen.removeFromTop (screen.getHeight() * 0.62f);

            // Leichter Glow hinter dem Text
            g.setColour (sceneAccent.withAlpha (0.22f));
            g.setFont (juce::FontOptions (27.0f, juce::Font::bold));
            g.drawText ("SCENE 0" + juce::String (editingScene), mainSceneRect.translated (0.0f, 1.0f), juce::Justification::centred);

            g.setColour (sceneAccent);
            g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
            g.drawText ("SCENE 0" + juce::String (editingScene), mainSceneRect, juce::Justification::centred);

            // Untere Info-Zeile: aktive Oszillator-Wavetables der aktuellen Scene
            const auto& osc1Wave = oscControls[0].waveBox.getText();
            const auto& osc2Wave = oscControls[1].waveBox.getText();
            g.setColour (juce::Colour::fromRGB (160, 200, 225));
            g.setFont (juce::FontOptions (10.0f, juce::Font::plain));
            g.drawText (osc1Wave + "  +  " + osc2Wave, screen, juce::Justification::centred);
            break;
        }

        case LcdPage::Morph:
        {
            const float s1    = static_cast<float> (sceneFaders[0].getValue());
            const float morph = static_cast<float> (sceneFaders[1].getValue());
            const float s2    = static_cast<float> (sceneFaders[2].getValue());

            auto textRow = screen.removeFromTop (18.0f);
            g.setColour (juce::Colour::fromRGB (0, 240, 255));
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (juce::String::formatted ("S1: %d%%", juce::roundToInt (s1 * 100.0f)),
                        textRow, juce::Justification::centredLeft);

            g.setColour (juce::Colour::fromRGB (255, 60, 180));
            g.drawText (juce::String::formatted ("S2: %d%%", juce::roundToInt (s2 * 100.0f)),
                        textRow, juce::Justification::centredRight);

            // Morph-Crossfade-Balken in der Mitte
            auto barArea = screen.reduced (4.0f, 10.0f);
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
            g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
            g.drawText (juce::String::formatted ("MORPH %d%%", juce::roundToInt (morph * 100.0f)),
                        barArea, juce::Justification::centred);
            break;
        }

        case LcdPage::Assign:
        {
            // 2 Zeilen x 5 Spalten für die 10 Matrix-Knobs
            const float cellW = screen.getWidth() / 5.0f;
            const float cellH = screen.getHeight() / 2.0f;
            g.setFont (juce::FontOptions (9.5f, juce::Font::bold));

            for (int idx = 0; idx < 10; ++idx)
            {
                const int row = idx / 5;
                const int col = idx % 5;
                auto cell = juce::Rectangle<float> (screen.getX() + col * cellW,
                                                    screen.getY() + row * cellH,
                                                    cellW, cellH);

                const int pct = juce::roundToInt (matrixKnobs[static_cast<size_t> (idx)].getValue() * 100.0);
                g.setColour (pct > 0 ? juce::Colour::fromRGB (0, 240, 255)
                                     : juce::Colour::fromRGB (95, 125, 150));
                g.drawText ("A" + juce::String (idx + 1) + ":" + juce::String (pct),
                            cell, juce::Justification::centred);
            }
            break;
        }

        case LcdPage::System:
        {
            g.setColour (juce::Colour::fromRGB (0, 240, 255));
            g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
            g.drawText ("NEOMORPH X1 // DUAL-SCENE", screen.removeFromTop (20.0f), juce::Justification::centred);

            g.setColour (juce::Colour::fromRGB (170, 205, 230));
            g.setFont (juce::FontOptions (10.5f, juce::Font::plain));
            g.drawText ("EDITING: SCENE 0" + juce::String (editingScene) + "   |   16 VOICES",
                        screen, juce::Justification::centred);
            break;
        }

        default:
            break;
    }
}

void NeomorphX1AudioProcessorEditor::drawSceneLeds (juce::Graphics& g, float w, float h)
{
    // Die 4 LEDs über den oberen 4 Scene-Buttons
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
        juce::Colour::fromRGB (0, 240, 255),   // LED 1: Scene 1 (Cyan)
        juce::Colour::fromRGB (255, 60, 180),  // LED 2: Scene 2 (Magenta)
        juce::Colour::fromRGB (255, 205, 50),  // LED 3: Solo (Amber)
        juce::Colour::fromRGB (60, 255, 150)   // LED 4: Link Edit (Neon-Grün)
    };

    for (int i = 0; i < 4; ++i)
    {
        const float cx = ledX[i] * w;
        const auto col = ledColours[i];

        if (ledActive[i])
        {
            // Äußerer LED-Glow
            g.setColour (col.withAlpha (0.35f));
            g.fillEllipse (cx - ledRadius * 2.1f, ledY - ledRadius * 2.1f, ledRadius * 4.2f, ledRadius * 4.2f);

            // Leuchtender LED-Kern
            g.setColour (col);
            g.fillEllipse (cx - ledRadius, ledY - ledRadius, ledRadius * 2.0f, ledRadius * 2.0f);

            // Kleiner Glanzpunkt
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.fillEllipse (cx - ledRadius * 0.45f, ledY - ledRadius * 0.45f, ledRadius * 0.7f, ledRadius * 0.7f);
        }
        else
        {
            // Ausgeschaltete LED leicht abdunkeln
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

    auto headerArea = panelBounds.reduced (10.0f, 5.0f).removeFromTop (16.0f);
    g.setColour (accentColour);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
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
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // 1. Haupt-Gehäuse zeichnen
    if (backgroundImage.isValid())
        g.drawImage (backgroundImage, bounds, juce::RectanglePlacement::stretchToFit);

    // 2. Aktives Tab-Overlay im Mittelfenster zeichnen
    const auto& activeOverlay = tabImages[static_cast<size_t> (currentTab)];
    if (activeOverlay.isValid())
        g.drawImage (activeOverlay, bounds, juce::RectanglePlacement::stretchToFit);

    // 3. LCD-Display oben links zeichnen ({ 0.0774f, 0.2243f } .. { 0.2964f, 0.4053f })
    drawLcdScreen (g, juce::Rectangle<float> (0.0774f * w,
                                              0.2243f * h,
                                              (0.2964f - 0.0774f) * w,
                                              (0.4053f - 0.2243f) * h));

    // 4. Status-LEDs über den 4 oberen Scene-Buttons zeichnen
    drawSceneLeds (g, w, h);

    // 5. Dynamische Tab-Grafiken (Oscillator-Sektionen & Live-Scopes)
    if (currentTab == ActiveTab::Oscillators)
    {
        drawOscillatorSection (g,
                               juce::Rectangle<float> (0.3181f * w, 0.2673f * h, 0.2380f * w, 0.2700f * h),
                               0,
                               juce::Colour::fromRGB (0, 240, 255));

        drawOscillatorSection (g,
                               juce::Rectangle<float> (0.5662f * w, 0.2673f * h, 0.2380f * w, 0.2700f * h),
                               1,
                               juce::Colour::fromRGB (255, 60, 180));
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

    // 2. 8 Klick-Buttons über den Scene-Buttons rechts neben den Fadern
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

    // 3. 4 Klick-Buttons unter dem LCD-Display
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

    // 4. Settings-Icon oben rechts
    const float iconSize = 0.032f * w;
    settingsButton.setBounds (
        juce::roundToInt (0.9515f * w - iconSize * 0.5f),
        juce::roundToInt (0.0343f * h - iconSize * 0.5f),
        juce::roundToInt (iconSize),
        juce::roundToInt (iconSize)
    );

    // 5. Die 10 Matrix-Drehregler
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

    // 6. Die 3 Scene-Fader
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

    // 7. Pitch- & Mod-Wheel
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

    // 8. Tab 1: Oscillators Layout (symmetrisch zwischen X = 0.3080f und X = 0.8143f)
    const float oscPanelX[2] = { 0.3181f, 0.5662f };
    const float panelY       = 0.2673f * h;
    const float panelW       = 0.2380f * w;
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
}

void NeomorphX1AudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        const float relX = static_cast<float> (event.x) / static_cast<float> (getWidth());
        const float relY = static_cast<float> (event.y) / static_cast<float> (getHeight());

        const auto coordPair = juce::String::formatted ("{ %.4ff, %.4ff },", relX, relY);
        juce::SystemClipboard::copyTextToClipboard (coordPair);

        triggerLcdNotification ("COORDINATE COPIED", coordPair);
    }
}