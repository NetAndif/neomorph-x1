#include "../PluginEditor.h"
#include "../PluginProcessor.h"

void NeomorphX1AudioProcessorEditor::initTabControls()
{
	const auto cyanAccent    = juce::Colour::fromRGB (0, 240, 255);
    const auto magentaAccent = juce::Colour::fromRGB (255, 60, 180);

    // --- Tab 1: Oscillators Controls ---
    const juce::StringArray waveformItems = {
        "Basic Analog", "PWM / Pulse", "Formant", "Digital Fold", "SuperSaw", "Vocal Morph",
        "FM Bell / Metallic", "Sub + Harmonics", "Bitcrush / LoFi", "Sync Lead"
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
        "Scene Morph", "LFO 1 Rate", "LFO 1 Depth",
            "Osc 1 Pan", "Osc 2 Pan", "Osc 1+2 Pan" // <-- Indizes 14, 15, 16
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
