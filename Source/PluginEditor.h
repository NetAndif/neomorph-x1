#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "CyberpunkLookAndFeel.h"
#include "BinaryData.h"

class SpringBackSlider : public juce::Slider
{
public:
    bool springBackEnabled = false;
    double restingValue    = 0.0;

    void mouseUp (const juce::MouseEvent& event) override
    {
        juce::Slider::mouseUp (event);
        if (springBackEnabled)
            setValue (restingValue, juce::sendNotificationSync);
    }
};

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

    explicit NeomorphX1AudioProcessorEditor (NeomorphX1AudioProcessor&);
    ~NeomorphX1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;

private:
    class FreeEgCanvas : public juce::Component
    {
    public:
        explicit FreeEgCanvas (NeomorphX1AudioProcessorEditor& ownerEditor);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseMove (const juce::MouseEvent& e) override;

    private:
        juce::Point<float> getPointPosition (int idx, juce::Rectangle<float> inner) const;
        int findPointAt (juce::Point<float> pos, juce::Rectangle<float> inner) const;

        NeomorphX1AudioProcessorEditor& owner;
        int draggedPoint = -1;
        int hoveredPoint = -1;
    };

    class StepSeqCanvas : public juce::Component
    {
    public:
        explicit StepSeqCanvas (NeomorphX1AudioProcessorEditor& ownerEditor);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;

    private:
        void applyMouseEdit (const juce::MouseEvent& e, bool toggleGateOnly);
        NeomorphX1AudioProcessorEditor& owner;
        int lastEditedStep = -1;
    };

    struct OscillatorControls
    {
        juce::ComboBox waveBox;

        juce::Slider octaveKnob;
        juce::Slider semiKnob;
        juce::Slider detuneKnob;
        juce::Slider positionKnob;

        juce::Slider morphKnob;
        juce::Slider phaseKnob;
        juce::Slider panKnob;
        juce::Slider levelKnob;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttachment;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   octaveAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   semiAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   detuneAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   positionAttachment;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   morphAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   phaseAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   panAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   levelAttachment;
    };

    struct FilterControls
    {
        juce::ComboBox typeBox;
        juce::ComboBox routingBox;

        juce::Slider cutoffKnob;
        juce::Slider resonanceKnob;

        juce::Slider driveKnob;
        juce::Slider envAmtKnob;
        juce::Slider keytrackKnob;
        juce::Slider velSensKnob;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> routingAttachment;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   cutoffAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   resonanceAttachment;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   driveAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   envAmtAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   keytrackAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   velSensAttachment;
    };

    struct EnvelopeControls
    {
        juce::Slider attackKnob;
        juce::Slider decayKnob;
        juce::Slider sustainKnob;
        juce::Slider releaseKnob;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    };

    struct FreeEgControls
    {
        FreeEgCanvas canvas;

        juce::ComboBox targetBox;
        juce::ComboBox modeBox;
        juce::ComboBox syncBox;

        juce::Slider timeKnob;
        juce::Slider amountKnob;
        juce::Slider smoothKnob;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncAttachment;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   timeAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   amountAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   smoothAttachment;

        explicit FreeEgControls (NeomorphX1AudioProcessorEditor& owner) : canvas (owner) {}
    };

    struct StepSeqControls
    {
        StepSeqCanvas canvas;

        juce::ComboBox modeBox;
        juce::ComboBox lengthBox;
        juce::ComboBox rateBox;

        juce::Slider gateKnob;
        juce::Slider swingKnob;
        juce::Slider amountKnob;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lengthAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rateAttachment;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   gateAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   swingAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   amountAttachment;

        explicit StepSeqControls (NeomorphX1AudioProcessorEditor& owner) : canvas (owner) {}
    };

    void timerCallback() override;
    void triggerLcdNotification (const juce::String& title, const juce::String& value);

    void setActiveTab (ActiveTab newTab);
    void setEditingScene (int newScene);
    void handleSceneButton (int buttonIndex);
    void bindOscillatorAttachments();
    void bindFilterAttachments();
    void bindEnvelopeAttachments();
    void bindFreeEgAttachments();
    void bindStepSeqAttachments();
    void updateTabVisibility();
    void showSettingsMenu();

    void drawLcdScreen (juce::Graphics& g, juce::Rectangle<float> lcdBounds);
    void drawSceneLeds (juce::Graphics& g, float w, float h);
    void drawOscillatorSection (juce::Graphics& g, juce::Rectangle<float> panelBounds,
                                int oscIndex, juce::Colour accentColour);
    void drawFilterPanels (juce::Graphics& g, juce::Rectangle<float> leftBounds,
                           juce::Rectangle<float> rightBounds);
    void drawEnvelopeSection (juce::Graphics& g, juce::Rectangle<float> panelBounds,
                              int envIndex, juce::Colour accentColour);
    void drawFreeEgPanels (juce::Graphics& g, juce::Rectangle<float> leftBounds,
                           juce::Rectangle<float> rightBounds);
    void drawStepSeqPanels (juce::Graphics& g, juce::Rectangle<float> leftBounds,
                            juce::Rectangle<float> rightBounds);

    NeomorphX1AudioProcessor& audioProcessor;
    CyberpunkLookAndFeel cyberpunkLookAndFeel;

    juce::Image backgroundImage;
    std::array<juce::Image, static_cast<size_t> (ActiveTab::NumTabs)> tabImages;
    std::array<juce::TextButton, static_cast<size_t> (ActiveTab::NumTabs)> tabButtons;
    juce::TextButton settingsButton;

    std::array<juce::TextButton, 8> sceneHardwareButtons;
    int editingScene  = 1;
    bool sceneSolo    = false;
    bool sceneLink    = false;

    std::array<juce::TextButton, 4> lcdPageButtons;
    LcdPage currentLcdPage = LcdPage::Patch;

    std::array<juce::Slider, 10> matrixKnobs;
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

    ActiveTab currentTab = ActiveTab::Oscillators;

    juce::String lcdNotifyTitle;
    juce::String lcdNotifyValue;
    bool showLcdNotify = false;
    int lcdNotifyTicks = 0;
    int lastPaintedSeqStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeomorphX1AudioProcessorEditor)
};
