#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

class NeomorphX1AudioProcessor : public juce::AudioProcessor
{
public:
    NeomorphX1AudioProcessor();
    ~NeomorphX1AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void copySceneParameters (int sourceScene, int targetScene);
    void swapSceneParameters();
    void resetSceneParameters (int scene);
    void randomizeSceneParameters (int scene);

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // --- Echtzeit-Parameter-Cache für lockfreien Audio-Thread ---
    struct OscParamPointers
    {
        std::atomic<float>* wave     = nullptr;
        std::atomic<float>* octave   = nullptr;
        std::atomic<float>* semi     = nullptr;
        std::atomic<float>* detune   = nullptr;
        std::atomic<float>* position = nullptr;
        std::atomic<float>* morph    = nullptr;
        std::atomic<float>* phase    = nullptr;
        std::atomic<float>* pan      = nullptr;
        std::atomic<float>* level    = nullptr;
    };

    struct FilterParamPointers
    {
        std::atomic<float>* type      = nullptr;
        std::atomic<float>* routing   = nullptr;
        std::atomic<float>* cutoff    = nullptr;
        std::atomic<float>* resonance = nullptr;
        std::atomic<float>* drive     = nullptr;
        std::atomic<float>* envAmt    = nullptr;
        std::atomic<float>* keytrack  = nullptr;
        std::atomic<float>* velSens   = nullptr;
    };

    struct EnvParamPointers
    {
        std::atomic<float>* attack  = nullptr;
        std::atomic<float>* decay   = nullptr;
        std::atomic<float>* sustain = nullptr;
        std::atomic<float>* release = nullptr;
    };

    struct SceneParamPointers
    {
        std::array<OscParamPointers, 2> oscs;
        FilterParamPointers filter;
        std::array<EnvParamPointers, 2> envs; // 0 = Amp EG, 1 = Filter/Mod EG
    };

    struct EngineParamCache
    {
        std::array<SceneParamPointers, 2> scenes;
        std::atomic<float>* scene1Level = nullptr;
        std::atomic<float>* sceneMorph  = nullptr;
        std::atomic<float>* scene2Level = nullptr;
        std::atomic<float>* pitchWheel  = nullptr;
        std::atomic<float>* modWheel    = nullptr;
        std::array<std::atomic<float>*, 10> matrixKnobs {};
    };

    void cacheParameterPointers();

    EngineParamCache paramCache;
    juce::Synthesiser synth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeomorphX1AudioProcessor)
};
