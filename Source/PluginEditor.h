#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "CyberpunkLookAndFeel.h"
#include "BinaryData.h"
#include "UI/CustomControls.h"
#include "UI/FreeEgCanvas.h"
#include "UI/StepSeqCanvas.h"

class NeomorphX1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    enum class ActiveTab
    {
        Oscillators = 0,
        Filter,
        Envelopes,
        FreeEG,
        StepSequencer,
        ModMatrix,
        NumTabs
    };

    enum class LcdPage
    {
        Patch = 0,
        Morph,
        Assign,
        System,
        NumPages
    };

    enum class BottomStripMode
    {
        Oscilloscope = 0,
        MorphVector,
        NumModes
    };

    explicit NeomorphX1AudioProcessorEditor (NeomorphX1AudioProcessor&);
    ~NeomorphX1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    bool keyPressed (const juce::KeyPress& key) override;

    // Zugriffspunkte für ausgelagerte Sub-Komponenten (FreeEgCanvas, StepSeqCanvas)
    NeomorphX1AudioProcessor& getProcessor() noexcept { return audioProcessor; }
    int getEditingScene() const noexcept { return editingScene; }
    bool isSceneLinkEnabled() const noexcept { return sceneLink; }
    int getStepSeqModeIndex() const noexcept { return stepSeqControls.modeBox.getSelectedItemIndex(); }
    int getStepSeqLengthIndex() const noexcept { return stepSeqControls.lengthBox.getSelectedItemIndex(); }
    void triggerLcdNotification (const juce::String& title, const juce::String& value);

private:
    struct OscillatorControls
    {
        juce::ComboBox waveBox;
        juce::Slider octaveKnob, semiKnob, detuneKnob, positionKnob;
        juce::Slider morphKnob, phaseKnob, panKnob, levelKnob;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octaveAttachment, semiAttachment, detuneAttachment, positionAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttachment, phaseAttachment, panAttachment, levelAttachment;
    };

    struct FilterControls
    {
        juce::ComboBox typeBox, routingBox;
        juce::Slider cutoffKnob, resonanceKnob;
        juce::Slider driveKnob, envAmtKnob, keytrackKnob, velSensKnob;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment, routingAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment, resonanceAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment, envAmtAttachment, keytrackAttachment, velSensAttachment;
    };

    struct EnvelopeControls
    {
        juce::Slider attackKnob, decayKnob, sustainKnob, releaseKnob;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment, decayAttachment, sustainAttachment, releaseAttachment;
    };

    struct FreeEgControls
    {
        FreeEgCanvas canvas;
        juce::ComboBox targetBox, modeBox, syncBox;
        juce::Slider timeKnob, amountKnob, smoothKnob;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAttachment, modeAttachment, syncAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timeAttachment, amountAttachment, smoothAttachment;

        explicit FreeEgControls (NeomorphX1AudioProcessorEditor& owner) : canvas (owner) {}
    };

    struct StepSeqControls
    {
        StepSeqCanvas canvas;
        juce::ComboBox modeBox, lengthBox, rateBox;
        juce::Slider gateKnob, swingKnob, amountKnob;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment, lengthAttachment, rateAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAttachment, swingAttachment, amountAttachment;

        explicit StepSeqControls (NeomorphX1AudioProcessorEditor& owner) : canvas (owner) {}
    };

    struct ModSlotControls
    {
        juce::ComboBox srcBox, dstBox;
        juce::Slider amtKnob;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAttachment, dstAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAttachment;
    };

    struct ModMatrixControls
    {
        std::array<ModSlotControls, 4> slots;
        juce::ComboBox lfoSyncBox;
        juce::Slider lfoRateKnob, lfoFadeKnob, lfoDepthKnob;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoSyncAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoRateAttachment, lfoFadeAttachment, lfoDepthAttachment;
    };

    void timerCallback() override;

    // MIDI Learn Menü für Assign 1..8
    void showAssignKnobMidiLearnMenu (int assignIndex);
    void updateAssignKnobTooltip (int assignIndex);
	
	// Save + Load MIDI Assignments
	void handleSaveMidiMappingClick();
	void handleLoadMidiMappingClick();

    // Keypad
    void setKeypadTarget (const juce::String& paramID, const juce::String& displayName);
    void handleKeypadDigit (int digit);
    void handleKeypadClear();
    void handleKeypadEnter();
    void handleKeypadToggleSign();

    // Presets
    void refreshPresetList();
    void handleSavePresetClick (bool forceSaveAsDialog);
    void handleLoadPresetClick();
    void handlePanicClick();
    void stepPreset (int delta);

    // Tabs & Attachments (implementiert in UI/EditorTabs.cpp)
    void initTabControls();
    void setActiveTab (ActiveTab newTab);
    void setEditingScene (int newScene);
    void handleSceneButton (int buttonIndex);
    void bindOscillatorAttachments();
    void bindFilterAttachments();
    void bindEnvelopeAttachments();
    void bindFreeEgAttachments();
    void bindStepSeqAttachments();
    void bindModMatrixAttachments();
    void updateTabVisibility();
    void showSettingsMenu();

    // Rendering (implementiert in UI/EditorDrawing.cpp)
    void drawLcdScreen (juce::Graphics& g, juce::Rectangle<float> lcdBounds);
    void drawBottomStatusStrip (juce::Graphics& g, juce::Rectangle<float> stripBounds);
    void drawSceneLeds (juce::Graphics& g, float w, float h);
    void drawAssignMidiLearnBadges (juce::Graphics& g);
    void drawOscillatorSection (juce::Graphics& g, juce::Rectangle<float> panelBounds, int oscIndex, juce::Colour accentColour);
    void drawFilterPanels (juce::Graphics& g, juce::Rectangle<float> leftBounds, juce::Rectangle<float> rightBounds);
    void drawEnvelopeSection (juce::Graphics& g, juce::Rectangle<float> panelBounds, int envIndex, juce::Colour accentColour);
    void drawFreeEgPanels (juce::Graphics& g, juce::Rectangle<float> leftBounds, juce::Rectangle<float> rightBounds);
    void drawStepSeqPanels (juce::Graphics& g, juce::Rectangle<float> leftBounds, juce::Rectangle<float> rightBounds);
    void drawModMatrixPanels (juce::Graphics& g, juce::Rectangle<float> leftBounds, juce::Rectangle<float> rightBounds);

    NeomorphX1AudioProcessor& audioProcessor;
    CyberpunkLookAndFeel cyberpunkLookAndFeel;

    juce::Image backgroundImage;
    std::array<juce::Image, static_cast<size_t> (ActiveTab::NumTabs)> tabImages;
    std::array<juce::TextButton, static_cast<size_t> (ActiveTab::NumTabs)> tabButtons;
    juce::TextButton settingsButton;

    // Top-Bar Preset-Verwaltung
    juce::ComboBox presetBox;
    TopBarPresetButton savePresetButton;
    TopBarPresetButton loadPresetButton;
    TopBarPresetButton panicButton;
    juce::TextButton prevPresetButton;
    juce::TextButton nextPresetButton;
    std::unique_ptr<juce::FileChooser> activeFileChooser;
    bool suppressPresetBoxCallback = false;

    // 10er-Nummernblock
    std::array<juce::TextButton, 12> keypadButtons;
    juce::String keypadTargetParamID { "knob_1" };
    juce::String keypadTargetLabel   { "MASTER VOLUME" };
    juce::String keypadBuffer;
    bool keypadNegative = false;
    bool keypadActive   = false;

    std::array<juce::TextButton, 8> sceneHardwareButtons;
    int editingScene  = 1;
    bool sceneSolo    = false;
    bool sceneLink    = false;

    std::array<juce::TextButton, 4> lcdPageButtons;
    LcdPage currentLcdPage = LcdPage::Patch;

    // [0] = Master Vol, [1] = Master Pan, [2..9] = Assign 1..8 (mit MIDI Learn)
    std::array<AssignableKnobSlider, 10> matrixKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 10> knobAttachments;

    std::array<juce::Slider, 3> sceneFaders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 3> faderAttachments;

    SpringBackSlider pitchWheel;
    SpringBackSlider modWheel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchWheelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modWheelAttachment;

    std::array<OscillatorControls, 2> oscControls;
    FilterControls filterControls;
    std::array<EnvelopeControls, 2> envControls;
    FreeEgControls freeEgControls { *this };
    StepSeqControls stepSeqControls { *this };
    ModMatrixControls modMatrixControls;

    ActiveTab currentTab = ActiveTab::Oscillators;

    juce::String lcdNotifyTitle;
    juce::String lcdNotifyValue;
    bool showLcdNotify = false;
    int lcdNotifyTicks = 0;
    int lastPaintedSeqStep = -1;
    float lastPaintedLfo   = 0.0f;

    BottomStripMode bottomStripMode = BottomStripMode::Oscilloscope;
    juce::Rectangle<float> bottomStripBounds;
    juce::Rectangle<float> scopeClickArea;
    std::array<float, NeomorphX1AudioProcessor::scopeBufferSize> scopeCache {};
    float smoothedLevelL   = 0.0f;
    float smoothedLevelR   = 0.0f;
    float peakHoldL        = 0.0f;
    float peakHoldR        = 0.0f;
    int peakHoldTicksL     = 0;
    int peakHoldTicksR     = 0;
    int midiLedHoldTicks   = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeomorphX1AudioProcessorEditor)
};
