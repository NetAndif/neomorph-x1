#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "PluginProcessor.h"
#include "BinaryData.h"

class NeomorphX1AudioProcessorEditor : public juce::AudioProcessorEditor
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

    explicit NeomorphX1AudioProcessorEditor (NeomorphX1AudioProcessor&);
    ~NeomorphX1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;

private:
    void setActiveTab (ActiveTab newTab);
    void showSettingsMenu();

    NeomorphX1AudioProcessor& audioProcessor;

    juce::Image backgroundImage;
    std::array<juce::Image, static_cast<size_t> (ActiveTab::NumTabs)> tabImages;
    std::array<juce::TextButton, static_cast<size_t> (ActiveTab::NumTabs)> tabButtons;
    juce::TextButton settingsButton;

    ActiveTab currentTab = ActiveTab::Envelopes;

    // Dev-Helfer zum Auslesen relativer Positionen per Rechtsklick
    juce::String lastClickedCoords;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeomorphX1AudioProcessorEditor)
};
