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

    void timerCallback() override;
    void triggerLcdNotification (const juce::String& title, const juce::String& value);

    void setActiveTab (ActiveTab newTab);
    void setEditingScene (int newScene);
    void handleSceneButton (int buttonIndex);
    void bindOscillatorAttachments();
    void updateTabVisibility();
    void showSettingsMenu();

    void drawLcdScreen (juce::Graphics& g, juce::Rectangle<float> lcdBounds);
    void drawSceneLeds (juce::Graphics& g, float w, float h);
    void drawOscillatorSection (juce::Graphics& g, juce::Rectangle<float> panelBounds,
                                int oscIndex, juce::Colour accentColour);

    NeomorphX1AudioProcessor& audioProcessor;
    CyberpunkLookAndFeel cyberpunkLookAndFeel;

    juce::Image backgroundImage;
    std::array<juce::Image, static_cast<size_t> (ActiveTab::NumTabs)> tabImages;
    std::array<juce::TextButton, static_cast<size_t> (ActiveTab::NumTabs)> tabButtons;
    juce::TextButton settingsButton;

    // 8 unsichtbare Klick-Buttons über den Gehäuse-Buttons rechts neben den Scene-Fadern
    std::array<juce::TextButton, 8> sceneHardwareButtons;
    int editingScene  = 1;     // 1 = Scene 1, 2 = Scene 2
    bool sceneSolo    = false; // Solo für aktive Scene
    bool sceneLink    = false; // Link-Edit (spiegelt Änderungen auf beide Scenes)

    // 4 unsichtbare Klick-Buttons unter dem LCD-Display
    std::array<juce::TextButton, 4> lcdPageButtons;
    LcdPage currentLcdPage = LcdPage::Patch;

    // 10 Matrix-Drehregler + APVTS-Attachments
    std::array<juce::Slider, 10> matrixKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 10> knobAttachments;

    // 3 Scene-Fader + APVTS-Attachments
    std::array<juce::Slider, 3> sceneFaders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 3> faderAttachments;

    // Pitch- & Mod-Wheel + APVTS-Attachments
    SpringBackSlider pitchWheel;
    SpringBackSlider modWheel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchWheelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modWheelAttachment;

    // Tab 1: Oscillators (links OSC 1, rechts OSC 2)
    std::array<OscillatorControls, 2> oscControls;

    ActiveTab currentTab = ActiveTab::Oscillators;

    // Temporäre Parameter-Einblendung im LCD-Display
    juce::String lcdNotifyTitle;
    juce::String lcdNotifyValue;
    bool showLcdNotify = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeomorphX1AudioProcessorEditor)
};