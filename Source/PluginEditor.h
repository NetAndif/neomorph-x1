#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class NeomorphX1AudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    NeomorphX1AudioProcessorEditor (NeomorphX1AudioProcessor&);
    ~NeomorphX1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    NeomorphX1AudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeomorphX1AudioProcessorEditor)
};