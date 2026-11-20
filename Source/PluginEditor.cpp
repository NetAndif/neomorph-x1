#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// NeomorphX1AudioProcessorEditor Konstruktor
// ==============================================================================
NeomorphX1AudioProcessorEditor::NeomorphX1AudioProcessorEditor (NeomorphX1AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&cyberpunkLookAndFeel);
    setWantsKeyboardFocus (true);
    backgroundImage = juce::ImageCache::getFromMemory (BinaryData::background_main_png, BinaryData::background_main_pngSize);
	tabImages[0] = juce::ImageCache::getFromMemory (BinaryData::tab_oscillators_png, BinaryData::tab_oscillators_pngSize);
	tabImages[1] = juce::ImageCache::getFromMemory (BinaryData::tab_filter_png,      BinaryData::tab_filter_pngSize);
	tabImages[2] = juce::ImageCache::getFromMemory (BinaryData::tab_envelopes_png,   BinaryData::tab_envelopes_pngSize);
	tabImages[3] = juce::ImageCache::getFromMemory (BinaryData::tab_free_eg_png,     BinaryData::tab_free_eg_pngSize);
	tabImages[4] = juce::ImageCache::getFromMemory (BinaryData::tab_step_seq_png,    BinaryData::tab_step_seq_pngSize);
	tabImages[5] = juce::ImageCache::getFromMemory (BinaryData::tab_mod_matrix_png,  BinaryData::tab_mod_matrix_pngSize);

    // --- Top-Bar Preset-Verwaltung ---
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.setTooltip ("Select Preset (Grouped by Factory / User Category)");
    presetBox.onChange = [this]
    {
        if (suppressPresetBoxCallback)
            return;

        const int selectedId = presetBox.getSelectedId();
        const int entryIndex = selectedId - 1;

        if (juce::isPositiveAndBelow (entryIndex, cachedPresetEntries.size()))
        {
            const auto& entry = cachedPresetEntries.getReference (entryIndex);
            if (audioProcessor.loadPresetEntry (entry))
            {
                freeEgControls.canvas.repaint();
                stepSeqControls.canvas.repaint();
                triggerLcdNotification (entry.category.toUpperCase(), entry.name);
            }
        }
        else
        {
            const auto selectedName = presetBox.getText();
            if (selectedName.isNotEmpty() && audioProcessor.loadPresetByName (selectedName))
            {
                freeEgControls.canvas.repaint();
                stepSeqControls.canvas.repaint();
                triggerLcdNotification ("PRESET LOADED", selectedName);
            }
        }
    };
    addAndMakeVisible (presetBox);

    savePresetButton.setAlpha (0.0f);
    savePresetButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    savePresetButton.setTooltip ("Save Preset (Right-Click: Save As / Category...)");
    savePresetButton.onClick = [this] { handleSavePresetClick (false); };
    savePresetButton.onRightClick = [this] { handleSavePresetClick (true); };
    addAndMakeVisible (savePresetButton);

    loadPresetButton.setAlpha (0.0f);
    loadPresetButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    loadPresetButton.setTooltip ("Import External Preset (.xml) (Right-Click: Preset Manager)");
    loadPresetButton.onClick = [this] { handleLoadPresetClick(); };
    loadPresetButton.onRightClick = [this]
    {
        juce::PopupMenu mgr;
        mgr.addSectionHeader ("PRESET MANAGER");
        mgr.addItem (1, "Import External Preset (.xml)...");
        mgr.addSeparator();
        mgr.addItem (2, "Save As / Set Category & Metadata...");
        mgr.addItem (3, "Rename / Move Current User Preset...",
                     audioProcessor.isCurrentPresetSaved() && ! audioProcessor.isCurrentPresetFactory());
        mgr.addItem (4, "Delete Current User Preset...",
                     audioProcessor.isCurrentPresetSaved() && ! audioProcessor.isCurrentPresetFactory());
        mgr.addSeparator();
        mgr.addItem (5, "Restore Factory Presets");
        mgr.addItem (6, "Open Presets Folder...");

        mgr.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&loadPresetButton),
                           [this] (int res)
        {
            switch (res)
            {
                case 1: handleLoadPresetClick(); break;
                case 2: handleSavePresetClick (true); break;
                case 3: handleRenameOrMovePresetClick(); break;
                case 4: handleDeletePresetClick(); break;
                case 5: handleRestoreFactoryPresetsClick(); break;
                case 6: audioProcessor.getPresetDirectory().revealToUser(); break;
                default: break;
            }
        });
    };
    addAndMakeVisible (loadPresetButton);

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
		if (i >= 2)
        {
            const int assignIdx = static_cast<int> (i - 2);
            knob.onRightClick = [this, assignIdx] { showAssignKnobMidiLearnMenu (assignIdx); };
            updateAssignKnobTooltip (assignIdx);
        }
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
	initTabControls();

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
// MIDI-Mapping Logik
// ==============================================================================

void NeomorphX1AudioProcessorEditor::handleSaveMidiMappingClick()
{
    activeFileChooser = std::make_unique<juce::FileChooser> (
        "Save Neomorph X1 MIDI Mapping (.xml)",
        audioProcessor.getMidiMapDirectory().getChildFile ("ControllerMap.xml"),
        "*.xml");

    constexpr int flags = juce::FileBrowserComponent::saveMode
                        | juce::FileBrowserComponent::canSelectFiles
                        | juce::FileBrowserComponent::warnAboutOverwriting;

    activeFileChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();
        if (file == juce::File())
            return;

        if (! file.hasFileExtension (".xml"))
            file = file.withFileExtension (".xml");

        if (audioProcessor.saveMidiMappingToFile (file))
            triggerLcdNotification ("MIDI MAP SAVED", file.getFileNameWithoutExtension().toUpperCase());
        else
            triggerLcdNotification ("MIDI MAP ERROR", "SAVE FAILED");
    });
}

void NeomorphX1AudioProcessorEditor::handleLoadMidiMappingClick()
{
    activeFileChooser = std::make_unique<juce::FileChooser> (
        "Load Neomorph X1 MIDI Mapping (.xml)",
        audioProcessor.getMidiMapDirectory(),
        "*.xml");

    constexpr int flags = juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectFiles;

    activeFileChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (! file.existsAsFile())
            return;

        if (audioProcessor.loadMidiMappingFromFile (file))
        {
            for (int k = 0; k < NeomorphX1AudioProcessor::numAssignKnobs; ++k)
                updateAssignKnobTooltip (k);

            triggerLcdNotification ("MIDI MAP LOADED", file.getFileNameWithoutExtension().toUpperCase());
            repaint();
        }
        else
        {
            triggerLcdNotification ("MIDI MAP ERROR", "INVALID MAP FILE");
        }
    });
}

void NeomorphX1AudioProcessorEditor::updateAssignKnobTooltip (int assignIndex)
{
    auto& knob = matrixKnobs[static_cast<size_t> (assignIndex + 2)];
    const int cc = audioProcessor.getAssignedMidiCC (assignIndex);
    const auto baseName = "Assign " + juce::String (assignIndex + 1);

    if (cc >= 0)
        knob.setTooltip (baseName + " [MIDI CC " + juce::String (cc) + "] (Right-Click: MIDI Learn)");
    else
        knob.setTooltip (baseName + " (Right-Click: MIDI Learn)");
}

void NeomorphX1AudioProcessorEditor::showAssignKnobMidiLearnMenu (int assignIndex)
{
    const int currentCC   = audioProcessor.getAssignedMidiCC (assignIndex);
    const bool isLearning = (audioProcessor.getMidiLearnTargetSlot() == assignIndex);

    juce::PopupMenu menu;
    menu.addSectionHeader ("ASSIGN " + juce::String (assignIndex + 1) + " // MIDI LEARN");
	
	const auto currentMode = audioProcessor.getAssignMidiCCMode (assignIndex);

    juce::PopupMenu modeMenu;
    modeMenu.addItem (10, "Absolute (0-127)",                     true, currentMode == NeomorphX1AudioProcessor::MidiCCMode::Absolute);
    modeMenu.addItem (11, "Relative (2's Comp: 1..63 / 127..65)", true, currentMode == NeomorphX1AudioProcessor::MidiCCMode::RelativeTwosComp);
    modeMenu.addItem (12, "Relative (Bin Offset: 65..127 / 63..1)", true, currentMode == NeomorphX1AudioProcessor::MidiCCMode::RelativeBinOffset);
    menu.addSubMenu ("CC Mode", modeMenu);

    if (isLearning)
        menu.addItem (2, "Cancel MIDI Learn");
    else
        menu.addItem (1, "MIDI Learn (Move Hardware Controller...)");

    menu.addItem (3,
                  currentCC >= 0 ? ("Clear MIDI CC (" + juce::String (currentCC) + ")") : "Clear MIDI CC",
                  currentCC >= 0);

    auto& targetKnob = matrixKnobs[static_cast<size_t> (assignIndex + 2)];
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&targetKnob),
                        [this, assignIndex] (int result)
    {
        if (result == 1)
        {
            audioProcessor.startMidiLearnForAssign (assignIndex);
            triggerLcdNotification ("MIDI LEARN // ASSIGN " + juce::String (assignIndex + 1),
                                    "MOVE MIDI CC...");
            repaint();
        }
        else if (result == 2)
        {
            audioProcessor.cancelMidiLearn();
            triggerLcdNotification ("MIDI LEARN", "CANCELLED");
            repaint();
        }
        else if (result == 3)
        {
            audioProcessor.clearMidiLearnForAssign (assignIndex);
            updateAssignKnobTooltip (assignIndex);
            triggerLcdNotification ("ASSIGN " + juce::String (assignIndex + 1), "MIDI CC CLEARED");
            repaint();
        }
		else if (result >= 10 && result <= 12)
        {
            const auto newMode = static_cast<NeomorphX1AudioProcessor::MidiCCMode> (result - 10);
            audioProcessor.setAssignMidiCCMode (assignIndex, newMode);
            updateAssignKnobTooltip (assignIndex);

            const char* modeNames[3] = { "ABSOLUTE", "REL (2'S COMP)", "REL (BIN OFFSET)" };
            triggerLcdNotification ("ASSIGN " + juce::String (assignIndex + 1) + " MODE", modeNames[result - 10]);
            repaint();
        }
    });
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
// Preset-Verwaltung UI-Logik (Hierarchische Kategorien & Preset-Manager)
// ==============================================================================
void NeomorphX1AudioProcessorEditor::refreshPresetList()
{
    suppressPresetBoxCallback = true;

    presetBox.clear (juce::dontSendNotification);
    cachedPresetEntries = audioProcessor.getAllPresetEntries();

    const auto currentName     = audioProcessor.getCurrentPresetName();
    const auto currentCategory = audioProcessor.getCurrentPresetCategory();

    auto* rootMenu = presetBox.getRootMenu();
    rootMenu->clear();

    // 1. "Init" ganz oben als direkter Schnellzugriff
    int matchId = 1;
    for (int i = 0; i < cachedPresetEntries.size(); ++i)
    {
        const auto& entry = cachedPresetEntries.getReference (i);
        const int itemId  = i + 1;

        if (entry.name.equalsIgnoreCase ("Init") && entry.category.isEmpty())
        {
            const bool isCurrent = currentName.equalsIgnoreCase ("Init");
            if (isCurrent)
                matchId = itemId;

            rootMenu->addItem (itemId, "Init", true, isCurrent);
            rootMenu->addSeparator();
            break;
        }
    }

    // Hilfsfunktion: Baut aus einer Teilmenge von Entries einen Baum aus juce::PopupMenu-Untermenüs
    struct MenuNode
    {
        juce::String label;
        std::map<juce::String, std::unique_ptr<MenuNode>> children;
        std::vector<int> entryIndices;
    };

    auto buildHierarchyMenu = [&] (const juce::String& rootFolderTitle, bool wantFactory)
    {
        MenuNode rootNode;
        rootNode.label = rootFolderTitle;
        int count = 0;

        for (int i = 0; i < cachedPresetEntries.size(); ++i)
        {
            const auto& entry = cachedPresetEntries.getReference (i);
            if (entry.name.equalsIgnoreCase ("Init") && entry.category.isEmpty())
                continue;

            if (entry.isFactory != wantFactory)
                continue;

            ++count;

            // Kategorie-Pfad aufteilen (z.B. "Factory/Bass" -> ["Bass"] oder "User/Synthwave/Leads" -> ["Synthwave", "Leads"])
            juce::String relCat = entry.category.replaceCharacter ('\\', '/');
            if (relCat.startsWithIgnoreCase (rootFolderTitle + "/"))
                relCat = relCat.substring (rootFolderTitle.length() + 1);
            else if (relCat.equalsIgnoreCase (rootFolderTitle))
                relCat.clear();

            juce::StringArray parts;
            if (relCat.isNotEmpty())
                parts.addTokens (relCat, "/", "");
            parts.removeEmptyStrings();

            MenuNode* cursor = &rootNode;
            for (const auto& part : parts)
            {
                auto& child = cursor->children[part];
                if (child == nullptr)
                {
                    child = std::make_unique<MenuNode>();
                    child->label = part;
                }
                cursor = child.get();
            }

            cursor->entryIndices.push_back (i);
        }

        if (count == 0)
            return;

        std::function<void (const MenuNode&, juce::PopupMenu&)> populateMenu =
            [&] (const MenuNode& node, juce::PopupMenu& targetMenu)
        {
            // Zuerst Unterordner alphabetisch
            for (const auto& kv : node.children)
            {
                juce::PopupMenu sub;
                populateMenu (*kv.second, sub);
                targetMenu.addSubMenu (kv.first, sub);
            }

            if (! node.children.empty() && ! node.entryIndices.empty())
                targetMenu.addSeparator();

            // Danach Presets in diesem Ordner
            for (int idx : node.entryIndices)
            {
                const auto& entry = cachedPresetEntries.getReference (idx);
                const int itemId  = idx + 1;
                const bool isCurrent = entry.name.equalsIgnoreCase (currentName)
                                    && (currentCategory.isEmpty() || entry.category.equalsIgnoreCase (currentCategory));
                if (isCurrent)
                    matchId = itemId;

                targetMenu.addItem (itemId, entry.name, true, isCurrent);
            }
        };

        juce::PopupMenu topSubMenu;
        populateMenu (rootNode, topSubMenu);
        rootMenu->addSubMenu (rootFolderTitle, topSubMenu);
    };

    buildHierarchyMenu ("Factory", true);
    buildHierarchyMenu ("User",    false);

    presetBox.setSelectedId (matchId, juce::dontSendNotification);
    if (presetBox.getText().isEmpty())
        presetBox.setText (currentName, juce::dontSendNotification);

    suppressPresetBoxCallback = false;
}

void NeomorphX1AudioProcessorEditor::handleSavePresetClick (bool forceSaveAsDialog)
{
    // Direktspeichern nur für bereits existierende User-Presets (Factory-Presets öffnen immer "Save As")
    if (! forceSaveAsDialog && audioProcessor.isCurrentPresetSaved() && ! audioProcessor.isCurrentPresetFactory())
    {
        const auto name = audioProcessor.getCurrentPresetName();
        const auto cat  = audioProcessor.getCurrentPresetCategory();
        if (audioProcessor.savePreset (name, cat))
            triggerLcdNotification ("PRESET SAVED", name);
        return;
    }

    auto* alert = new juce::AlertWindow ("NEOMORPH X1 // SAVE USER PRESET",
                                         audioProcessor.isCurrentPresetFactory()
                                             ? "Factory presets are read-only. Save a copy to your User library:"
                                             : "Configure preset name, category folder, and metadata:",
                                         juce::MessageBoxIconType::NoIcon);

    const auto suggestedName = audioProcessor.isCurrentPresetSaved()
                                   ? (audioProcessor.getCurrentPresetName() + (audioProcessor.isCurrentPresetFactory() ? " (User)" : " Copy"))
                                   : "Neomorph Patch 01";

    alert->addTextEditor ("presetName", suggestedName, "Preset Name:");

    // Kategorie-Auswahl + freie Unterordner-Eingabe
    juce::StringArray categorySuggestions = {
        "User",
        "User/Bass",
        "User/Leads",
        "User/Pads",
        "User/Keys",
        "User/Plucks",
        "User/Arp & Seq",
        "User/FX & Drones"
    };
    for (const auto& existingCat : audioProcessor.getPresetCategories())
    {
        if (! existingCat.startsWithIgnoreCase ("Factory") && ! categorySuggestions.contains (existingCat, true))
            categorySuggestions.add (existingCat);
    }

    juce::String defaultCat = audioProcessor.getCurrentPresetCategory();
    if (defaultCat.isEmpty() || defaultCat.startsWithIgnoreCase ("Factory"))
        defaultCat = "User";

    alert->addComboBox ("categoryBox", categorySuggestions, "Category Preset:");
    if (auto* cb = alert->getComboBoxComponent ("categoryBox"))
    {
        cb->setEditableText (true);
        cb->setText (defaultCat, juce::dontSendNotification);
    }

    alert->addTextEditor ("authorName", audioProcessor.getCurrentPresetAuthor(), "Author (optional):");
    alert->addTextEditor ("notesText",  audioProcessor.getCurrentPresetNotes(),  "Notes (optional):");

    alert->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    alert->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, alert] (int result)
        {
            if (result == 1)
            {
                const auto newName = alert->getTextEditorContents ("presetName").trim();
                juce::String category = "User";
                if (auto* cb = alert->getComboBoxComponent ("categoryBox"))
                    category = cb->getText().trim();

                const auto author = alert->getTextEditorContents ("authorName").trim();
                const auto notes  = alert->getTextEditorContents ("notesText").trim();

                if (newName.isNotEmpty() && ! newName.equalsIgnoreCase ("Init"))
                {
                    if (audioProcessor.savePreset (newName, category, author, notes))
                    {
                        refreshPresetList();
                        triggerLcdNotification ("SAVED [" + category.toUpperCase() + "]", newName);
                    }
                    else
                    {
                        triggerLcdNotification ("SAVE ERROR", "WRITE FAILED");
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
        "Import Neomorph X1 Preset (.xml)",
        audioProcessor.getPresetDirectory(),
        "*.xml");

    constexpr int flags = juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectFiles;

    activeFileChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (! file.existsAsFile())
            return;

        const bool isOutsideLibrary = ! file.isAChildOf (audioProcessor.getPresetDirectory());

        if (audioProcessor.loadPresetFromFile (file, isOutsideLibrary, "User"))
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

void NeomorphX1AudioProcessorEditor::handleRenameOrMovePresetClick()
{
    if (! audioProcessor.isCurrentPresetSaved() || audioProcessor.isCurrentPresetFactory())
    {
        triggerLcdNotification ("PROTECTED", "FACTORY / INIT LOCKED");
        return;
    }

    auto* alert = new juce::AlertWindow ("NEOMORPH X1 // RENAME OR MOVE PRESET",
                                         "Edit preset name or target User category folder:",
                                         juce::MessageBoxIconType::NoIcon);

    alert->addTextEditor ("presetName", audioProcessor.getCurrentPresetName(), "New Preset Name:");

    juce::StringArray categorySuggestions = {
        "User", "User/Bass", "User/Leads", "User/Pads",
        "User/Keys", "User/Plucks", "User/Arp & Seq", "User/FX & Drones"
    };
    for (const auto& existingCat : audioProcessor.getPresetCategories())
    {
        if (! existingCat.startsWithIgnoreCase ("Factory") && ! categorySuggestions.contains (existingCat, true))
            categorySuggestions.add (existingCat);
    }

    alert->addComboBox ("categoryBox", categorySuggestions, "Target Category:");
    if (auto* cb = alert->getComboBoxComponent ("categoryBox"))
    {
        cb->setEditableText (true);
        cb->setText (audioProcessor.getCurrentPresetCategory(), juce::dontSendNotification);
    }

    alert->addButton ("Apply", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    alert->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, alert] (int result)
        {
            if (result == 1)
            {
                const auto newName = alert->getTextEditorContents ("presetName").trim();
                juce::String newCat = "User";
                if (auto* cb = alert->getComboBoxComponent ("categoryBox"))
                    newCat = cb->getText().trim();

                if (newName.isNotEmpty() && ! newName.equalsIgnoreCase ("Init"))
                {
                    if (audioProcessor.renameOrMoveCurrentUserPreset (newName, newCat))
                    {
                        refreshPresetList();
                        triggerLcdNotification ("MOVED [" + newCat.toUpperCase() + "]", newName);
                    }
                    else
                    {
                        triggerLcdNotification ("MOVE ERROR", "OPERATION FAILED");
                    }
                }
            }
        }), true);
}

void NeomorphX1AudioProcessorEditor::handleDeletePresetClick()
{
    if (! audioProcessor.isCurrentPresetSaved() || audioProcessor.isCurrentPresetFactory())
    {
        triggerLcdNotification ("PROTECTED", "CANNOT DELETE FACTORY");
        return;
    }

    const auto nameToDelete = audioProcessor.getCurrentPresetName();
    auto* alert = new juce::AlertWindow ("NEOMORPH X1 // DELETE USER PRESET",
                                         "Permanently delete \"" + nameToDelete + "\"?",
                                         juce::MessageBoxIconType::WarningIcon);

    alert->addButton ("Delete", 1);
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    alert->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, nameToDelete] (int result)
        {
            if (result == 1)
            {
                if (audioProcessor.deleteCurrentUserPreset())
                {
                    refreshPresetList();
                    freeEgControls.canvas.repaint();
                    stepSeqControls.canvas.repaint();
                    triggerLcdNotification ("PRESET DELETED", nameToDelete);
                }
            }
        }), true);
}

void NeomorphX1AudioProcessorEditor::handleRestoreFactoryPresetsClick()
{
    audioProcessor.restoreFactoryPresets();
    refreshPresetList();
    triggerLcdNotification ("FACTORY BANK", "PRESETS RESTORED");
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
    const int total = cachedPresetEntries.size();
    if (total <= 0)
        return;

    int currentIdx = presetBox.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow (currentIdx, total))
        currentIdx = 0;

    const int nextIdx = (currentIdx + delta + total) % total;
    presetBox.setSelectedId (nextIdx + 1, juce::sendNotificationSync);
}

void NeomorphX1AudioProcessorEditor::timerCallback()
{
	const int learnedSlot = audioProcessor.consumeLastLearnedAssignSlot();
    if (learnedSlot >= 0)
    {
        const int cc = audioProcessor.getAssignedMidiCC (learnedSlot);
        updateAssignKnobTooltip (learnedSlot);
        triggerLcdNotification ("ASSIGN " + juce::String (learnedSlot + 1) + " LEARNED",
                                "MIDI CC " + juce::String (cc));
        repaint();
    }

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

void NeomorphX1AudioProcessorEditor::showSettingsMenu()
{
    juce::PopupMenu menu;

    menu.addSectionHeader ("NEOMORPH X1 // SYSTEM");
    menu.addItem (1, "Init Full Patch (Reset)");

    juce::PopupMenu presetLibMenu;
    presetLibMenu.addItem (50, "Save Preset As / Set Category...");
    presetLibMenu.addItem (51, "Import External Preset (.xml)...");
    presetLibMenu.addSeparator();
    presetLibMenu.addItem (52, "Rename / Move Current User Preset...",
                           audioProcessor.isCurrentPresetSaved() && ! audioProcessor.isCurrentPresetFactory());
    presetLibMenu.addItem (53, "Delete Current User Preset...",
                           audioProcessor.isCurrentPresetSaved() && ! audioProcessor.isCurrentPresetFactory());
    presetLibMenu.addSeparator();
    presetLibMenu.addItem (54, "Restore Factory Presets");
    presetLibMenu.addItem (5,  "Open Presets Folder...");
    menu.addSubMenu ("Preset Library", presetLibMenu);

    menu.addSeparator();
    menu.addItem (2, "Copy Scene 1 -> Scene 2");
    menu.addItem (3, "Copy Scene 2 -> Scene 1");
    menu.addItem (4, "Swap Scene 1 <-> Scene 2");
    menu.addSeparator();
	
	juce::PopupMenu midiMenu;
    midiMenu.addItem (30, "Soft-Takeover (Pickup)", true, audioProcessor.isMidiPickupEnabled());
    midiMenu.addSeparator();
    midiMenu.addItem (31, "Save MIDI Mappings...");
    midiMenu.addItem (32, "Load MIDI Mappings...");
    midiMenu.addSeparator();
    midiMenu.addItem (33, "Clear All MIDI Mappings");
    menu.addSubMenu ("MIDI Mappings", midiMenu);
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
			case 30:
            {
                const bool newState = ! audioProcessor.isMidiPickupEnabled();
                audioProcessor.setMidiPickupEnabled (newState);
                triggerLcdNotification ("MIDI PICKUP", newState ? "ENABLED (SOFT)" : "DISABLED (JUMP)");
                repaint();
                break;
            }
            case 31:
                handleSaveMidiMappingClick();
                break;
            case 32:
                handleLoadMidiMappingClick();
                break;
            case 33:
                audioProcessor.clearAllMidiMappings();
                for (int k = 0; k < NeomorphX1AudioProcessor::numAssignKnobs; ++k)
                    updateAssignKnobTooltip (k);
                triggerLcdNotification ("MIDI MAPPINGS", "ALL CLEARED");
                repaint();
                break;
            case 50: handleSavePresetClick (true); break;
            case 51: handleLoadPresetClick(); break;
            case 52: handleRenameOrMovePresetClick(); break;
            case 53: handleDeletePresetClick(); break;
            case 54: handleRestoreFactoryPresetsClick(); break;
            default:
                break;
        }
    });
}

void NeomorphX1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (10, 14, 22));

    auto bounds   = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

if (backgroundImage.isValid())
        g.drawImage (backgroundImage, bounds, juce::RectanglePlacement::stretchToFit);

    const auto& activeOverlay = tabImages[static_cast<size_t> (currentTab)];
    if (activeOverlay.isValid())
        g.drawImage (activeOverlay, bounds, juce::RectanglePlacement::stretchToFit);

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
	drawAssignMidiLearnBadges (g);
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
    const float presetLeft   = 0.3600f * w;
    const float presetTop    = 0.0110f * h;
    const float presetRight  = 0.5965f * w;
    const float presetBottom = 0.0490f * h;
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
