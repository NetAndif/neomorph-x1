#include <array>
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

    // 4. Dual-Scene Parameter (s1_ & s2_ für SCENE 1 und SCENE 2)
    const juce::StringArray waveforms = {
        "Basic Analog", "PWM / Pulse", "Formant", "Digital Fold", "SuperSaw", "Vocal Morph"
    };

    for (int scene = 1; scene <= 2; ++scene)
    {
        const auto scenePrefix = "s" + juce::String (scene) + "_";
        const auto sceneLabel  = "S" + juce::String (scene) + " ";

        // 4a. Oscillators Tab (osc1 & osc2)
        for (int osc = 1; osc <= 2; ++osc)
        {
            const auto prefix = scenePrefix + "osc" + juce::String (osc) + "_";
            const auto label  = sceneLabel + "Osc " + juce::String (osc) + " ";

            int defaultWave = 0;
            if (scene == 1)
                defaultWave = (osc == 1) ? 0 : 1; // Scene 1: Basic Analog + PWM
            else
                defaultWave = (osc == 1) ? 2 : 5; // Scene 2: Formant + Vocal Morph

            params.push_back (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { prefix + "wave", 1 }, label + "Wavetable",
                waveforms, defaultWave));

            params.push_back (std::make_unique<juce::AudioParameterInt> (
                juce::ParameterID { prefix + "octave", 1 }, label + "Octave",
                -2, 2, (scene == 2 && osc == 1) ? 1 : 0));

            params.push_back (std::make_unique<juce::AudioParameterInt> (
                juce::ParameterID { prefix + "semi", 1 }, label + "Semitone",
                -12, 12, 0));

            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { prefix + "detune", 1 }, label + "Detune",
                juce::NormalisableRange<float> (-50.0f, 50.0f, 0.1f),
                (osc == 2) ? 7.0f : 0.0f));

            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { prefix + "position", 1 }, label + "Position",
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                (scene == 1) ? 0.25f : 0.65f));

            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { prefix + "morph", 1 }, label + "Morph",
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { prefix + "phase", 1 }, label + "Phase",
                juce::NormalisableRange<float> (0.0f, 360.0f, 1.0f), 0.0f));

            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { prefix + "pan", 1 }, label + "Pan",
                juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));

            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { prefix + "level", 1 }, label + "Level",
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                osc == 1 ? 0.85f : 0.65f));
        }

        // 4b. Envelopes Tab (env1 = Amp EG, env2 = Filter/Mod EG)
        const char* envNames[2] = { "Amp Env ", "Filter Env " };
        for (int env = 1; env <= 2; ++env)
        {
            const auto prefix = scenePrefix + "env" + juce::String (env) + "_";
            const auto label  = sceneLabel + envNames[env - 1];

            // Logarithmische Skew-Kurve (0.35f) für feine Auflösung im Millisekunden-Bereich
            juce::NormalisableRange<float> timeRange (0.001f, 10.0f, 0.001f, 0.35f);

            const float defaultAttack  = (env == 1) ? 0.012f : 0.045f;
            const float defaultDecay   = (env == 1) ? 0.350f : 0.600f;
            const float defaultSustain = (env == 1) ? 0.750f : 0.350f;
            const float defaultRelease = (env == 1) ? 0.400f : 0.550f;

            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { prefix + "attack", 1 }, label + "Attack",
                timeRange, defaultAttack));

            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { prefix + "decay", 1 }, label + "Decay",
                timeRange, defaultDecay));

            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { prefix + "sustain", 1 }, label + "Sustain",
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), defaultSustain));

            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { prefix + "release", 1 }, label + "Release",
                timeRange, defaultRelease));
        }
    }

    return { params.begin(), params.end() };
}

void NeomorphX1AudioProcessor::copySceneParameters (int sourceScene, int targetScene)
{
    if (sourceScene == targetScene)
        return;

    const auto srcPrefix = "s" + juce::String (sourceScene) + "_";
    const auto dstPrefix = "s" + juce::String (targetScene) + "_";

    for (auto* param : getParameters())
    {
        if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto id = rangedParam->getParameterID();
            if (id.startsWith (srcPrefix))
            {
                const auto targetID = dstPrefix + id.substring (srcPrefix.length());
                if (auto* targetParam = apvts.getParameter (targetID))
                    targetParam->setValueNotifyingHost (rangedParam->getValue());
            }
        }
    }
}

void NeomorphX1AudioProcessor::swapSceneParameters()
{
    const juce::String s1Prefix = "s1_";
    const juce::String s2Prefix = "s2_";

    for (auto* param : getParameters())
    {
        if (auto* p1 = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto id = p1->getParameterID();
            if (id.startsWith (s1Prefix))
            {
                const auto targetID = s2Prefix + id.substring (s1Prefix.length());
                if (auto* p2 = apvts.getParameter (targetID))
                {
                    const float val1 = p1->getValue();
                    const float val2 = p2->getValue();
                    p1->setValueNotifyingHost (val2);
                    p2->setValueNotifyingHost (val1);
                }
            }
        }
    }
}

void NeomorphX1AudioProcessor::resetSceneParameters (int scene)
{
    const auto prefix = "s" + juce::String (scene) + "_";

    for (auto* param : getParameters())
    {
        if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            if (rangedParam->getParameterID().startsWith (prefix))
                rangedParam->setValueNotifyingHost (rangedParam->getDefaultValue());
        }
    }
}

void NeomorphX1AudioProcessor::randomizeSceneParameters (int scene)
{
    const auto prefix = "s" + juce::String (scene) + "_";
    auto& rng = juce::Random::getSystemRandom();

    for (auto* param : getParameters())
    {
        if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto id = rangedParam->getParameterID();
            if (id.startsWith (prefix))
            {
                if (id.endsWith ("_level"))
                    rangedParam->setValueNotifyingHost (rng.nextFloat() * 0.45f + 0.50f);
                else if (id.endsWith ("_octave"))
                    rangedParam->setValueNotifyingHost (rangedParam->convertTo0to1 (static_cast<float> (rng.nextInt ({ -1, 2 }))));
                else if (id.endsWith ("_semi"))
                    rangedParam->setValueNotifyingHost (0.5f);
                else if (id.endsWith ("_attack"))
                    rangedParam->setValueNotifyingHost (rng.nextFloat() * 0.45f);
                else
                    rangedParam->setValueNotifyingHost (rng.nextFloat());
            }
        }
    }
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
