#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class NeomorphX1AudioProcessorEditor;

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
