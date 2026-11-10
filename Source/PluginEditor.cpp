#include "PluginProcessor.h"
#include "PluginEditor.h"

NeomorphX1AudioProcessorEditor::NeomorphX1AudioProcessorEditor (NeomorphX1AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (800, 500);
}

NeomorphX1AudioProcessorEditor::~NeomorphX1AudioProcessorEditor()
{
}

void NeomorphX1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (18, 18, 24));

    g.setColour (juce::Colours::cyan);
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawFittedText ("NEOMORPH X1", getLocalBounds().removeFromTop (60), juce::Justification::centred, 1);
}

void NeomorphX1AudioProcessorEditor::resized()
{
}