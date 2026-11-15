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

   #if JUCE_LINUX
    setSize (1400, 788); // 125 % Startgröße unter Linux für optimale Lesbarkeit
   #else
    setSize (1120, 630);
   #endif
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
        showLcdNotify = false;
        stopTimer();
        repaint();
        return;
    }

    editingScene = newScene;
    bindOscillatorAttachments();

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
        bindSlider (osc.panKnob,      osc.panAttachment,