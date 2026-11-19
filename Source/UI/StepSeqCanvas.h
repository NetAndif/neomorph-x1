#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class NeomorphX1AudioProcessorEditor;

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
