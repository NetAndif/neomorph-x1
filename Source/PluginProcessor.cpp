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
       apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
#else
     : apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
}

NeomorphX1AudioProcessor::~NeomorphX1AudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout NeomorphX1AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

	// 1. 10 Matrix-Drehregler (rechts)
    const juce::StringArray knobNames = {
        "Assign 1", "Assign 2",
        "Assign 3", "Assign 4",
        "Assign 5", "Assign 6",
        "Assign 7", "Assign 8",
        "Assign 9", "Assign 10"
    };

    // Individuelle Standardwerte für jeden der 10 Regler (0.0f bis 1.0f)
    constexpr std::array<float, 10> knobDefaults = {
        0.8f, 0.5f,   // Reihe 1 (Assign 1 & 2)
        0.0f, 0.0f,   // Reihe 2 (Assign 3 & 4)
        0.0f, 0.0f,   // Reihe 3 (Assign 5 & 6)
        0.0f, 0.0f,   // Reihe 4 (Assign 7 & 8)
        0.0f, 0.0f    // Reihe 5 (Assign 9 & 10)
    };

    for (int i = 0; i < 10; ++i)
    {
        const auto paramID = "knob_" + juce::String (i + 1);
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { paramID, 1 },
            knobNames[i],
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
            knobDefaults[static_cast<size_t> (i)]
        ));
    }

    // 2. 3 Scene-Fader (unten links/mitte)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "scene_1", 1 }, "Scene 1",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "scene_morph", 1 }, "Scene Morph",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "scene_2", 1 }, "Scene 2",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));

    // 3. Pitch- & Mod-Wheel (unten links)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "pitch_wheel", 1 }, "Pitch Wheel",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mod_wheel", 1 }, "Mod Wheel",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    return { params.begin(), params.end() };
}

const juce::String NeomorphX1AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NeomorphX1AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool NeomorphX1AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool NeomorphX1AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double NeomorphX1AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NeomorphX1AudioProcessor::getNumPrograms()
{
    return 1;
}

int NeomorphX1AudioProcessor::getCurrentProgram()
{
    return 0;
}

void NeomorphX1AudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String NeomorphX1AudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void NeomorphX1AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void NeomorphX1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void NeomorphX1AudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NeomorphX1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void NeomorphX1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();

        if (msg.isPitchWheel())
        {
            const float normPitch = (static_cast<float> (msg.getPitchWheelValue()) - 8192.0f) / 8192.0f;
            if (auto* param = apvts.getParameter ("pitch_wheel"))
                param->setValueNotifyingHost (param->convertTo0to1 (juce::jlimit (-1.0f, 1.0f, normPitch)));
        }
        else if (msg.isController() && msg.getControllerNumber() == 1)
        {
            const float normMod = static_cast<float> (msg.getControllerValue()) / 127.0f;
            if (auto* param = apvts.getParameter ("mod_wheel"))
                param->setValueNotifyingHost (normMod);
        }
    }
}

bool NeomorphX1AudioProcessor::hasEditor() const
{
    return true;
}

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
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NeomorphX1AudioProcessor();
}