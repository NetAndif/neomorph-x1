#include "PluginProcessor.h"
#include "PluginEditor.h"

NeomorphX1AudioProcessor::NeomorphX1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "Parameters", createParameterLayout())
#endif
{
}

NeomorphX1AudioProcessor::~NeomorphX1AudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout NeomorphX1AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // --- Master & Performance ---
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "volume", 1 }, "Volume", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "portamento", 1 }, "Portamento", juce::NormalisableRange<float> (0.0f, 500.0f, 1.0f), 0.0f));

    // --- Filter Section ---
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "cutoff", 1 }, "Cutoff", juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f), 2000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "resonance", 1 }, "Resonance", juce::NormalisableRange<float> (0.1f, 10.0f, 0.05f), 1.0f));

    // --- Global ADSR ---
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "main_attack", 1 }, "Attack", juce::NormalisableRange<float> (0.0f, 5000.0f, 1.0f, 0.4f), 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "main_decay", 1 }, "Decay", juce::NormalisableRange<float> (0.0f, 5000.0f, 1.0f, 0.4f), 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "main_sustain", 1 }, "Sustain", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.7f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "main_release", 1 }, "Release", juce::NormalisableRange<float> (0.0f, 5000.0f, 1.0f, 0.4f), 200.0f));

    // --- LFO Section ---
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lfo_rate", 1 }, "LFO Rate", juce::NormalisableRange<float> (0.1f, 30.0f, 0.1f, 0.5f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lfo_depth", 1 }, "LFO Depth", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    // --- Scene Morphing ---
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "scene_1_val", 1 }, "Scene 1", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "scene_morph", 1 }, "Scene Morph", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "scene_2_val", 1 }, "Scene 2", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    // --- Ribbon Controller ---
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ribbon_pos", 1 }, "Ribbon", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    return { params.begin(), params.end() };
}

const juce::String NeomorphX1AudioProcessor::getName() const { return JucePlugin_Name; }
bool NeomorphX1AudioProcessor::acceptsMidi() const { return true; }
bool NeomorphX1AudioProcessor::producesMidi() const { return false; }
bool NeomorphX1AudioProcessor::isMidiEffect() const { return false; }
double NeomorphX1AudioProcessor::getTailLengthSeconds() const { return 0.0; }

int NeomorphX1AudioProcessor::getNumPrograms() { return 1; }
int NeomorphX1AudioProcessor::getCurrentProgram() { return 0; }
void NeomorphX1AudioProcessor::setCurrentProgram (int index) {}
const juce::String NeomorphX1AudioProcessor::getProgramName (int index) { return {}; }
void NeomorphX1AudioProcessor::changeProgramName (int index, const juce::String& newName) {}

void NeomorphX1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {}
void NeomorphX1AudioProcessor::releaseResources() {}

bool NeomorphX1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void NeomorphX1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
}

bool NeomorphX1AudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* NeomorphX1AudioProcessor::createEditor()
{
    return new NeomorphX1AudioProcessorEditor (*this);
}

void NeomorphX1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NeomorphX1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NeomorphX1AudioProcessor();
}