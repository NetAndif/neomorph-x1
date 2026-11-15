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
            case 12: setSize (1400, 78