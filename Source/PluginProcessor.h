#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

class NeomorphX1AudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int scopeBufferSize = 256;

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
	
	// MIDI Learn & Soft-Takeover (Abholen) für Assign 1..8
    static constexpr int numAssignKnobs = 8;

    void cancelMidiLearn() noexcept
    {
        midiLearnTargetSlot.store (-1, std::memory_order_release);
    }

    int getMidiLearnTargetSlot() const noexcept
    {
        return midiLearnTargetSlot.load (std::memory_order_acquire);
    }

    int getAssignedMidiCC (int assignIndex) const noexcept
    {
        if (assignIndex >= 0 && assignIndex < numAssignKnobs)
            return assignKnobMidiCC[static_cast<size_t> (assignIndex)].load (std::memory_order_acquire);
        return -1;
    }

    int consumeLastLearnedAssignSlot() noexcept
    {
        return lastLearnedAssignSlot.exchange (-1, std::memory_order_acq_rel);
    }

    void setMidiPickupEnabled (bool enabled) noexcept
    {
        midiPickupEnabled.store (enabled, std::memory_order_release);
        if (enabled)
            resetAllPickupLocks();
    }

    bool isMidiPickupEnabled() const noexcept
    {
        return midiPickupEnabled.load (std::memory_order_acquire);
    }

	bool isAssignWaitingForPickup (int assignIndex) const noexcept
    {
        if (! isMidiPickupEnabled() || assignIndex < 0 || assignIndex >= numAssignKnobs)
            return false;
        if (getAssignMidiCCMode (assignIndex) != MidiCCMode::Absolute)
            return false;
        if (assignKnobMidiCC[static_cast<size_t> (assignIndex)].load (std::memory_order_acquire) < 0)
            return false;
        return ! assignPickupLocked[static_cast<size_t> (assignIndex)].load (std::memory_order_acquire);
    }

    void resetAllPickupLocks() noexcept
    {
        for (int k = 0; k < numAssignKnobs; ++k)
        {
            assignPickupLocked[static_cast<size_t> (k)].store (false, std::memory_order_release);
            lastMidiCCNorm[static_cast<size_t> (k)].store (-1.0f, std::memory_order_release);
        }
    }

    void clearAllMidiMappings() noexcept
    {
        midiLearnTargetSlot.store (-1, std::memory_order_release);
        for (int k = 0; k < numAssignKnobs; ++k)
        {
            assignKnobMidiCC[static_cast<size_t> (k)].store (-1, std::memory_order_release);
            assignPickupLocked[static_cast<size_t> (k)].store (false, std::memory_order_release);
            lastMidiCCNorm[static_cast<size_t> (k)].store (-1.0f, std::memory_order_release);
			assignKnobCCMode[static_cast<size_t> (k)].store (static_cast<int> (MidiCCMode::Absolute), std::memory_order_relaxed);
        }
    }

	enum class MidiCCMode : int
    {
        Absolute = 0,
        RelativeTwosComp = 1,   // +1..+63 = 1..63 | -1..-63 = 127..65 (Novation / Ableton / NI)
        RelativeBinOffset = 2   // +1..+63 = 65..127 | -1..-63 = 63..1 (Arturia / Behringer)
    };

    void setAssignMidiCCMode (int assignIndex, MidiCCMode mode) noexcept
    {
        if (assignIndex >= 0 && assignIndex < numAssignKnobs)
        {
            const auto idx = static_cast<size_t> (assignIndex);
            assignKnobCCMode[idx].store (static_cast<int> (mode), std::memory_order_release);
            assignPickupLocked[idx].store (false, std::memory_order_release);
            lastMidiCCNorm[idx].store (-1.0f, std::memory_order_release);
        }
    }

    MidiCCMode getAssignMidiCCMode (int assignIndex) const noexcept
    {
        if (assignIndex >= 0 && assignIndex < numAssignKnobs)
            return static_cast<MidiCCMode> (assignKnobCCMode[static_cast<size_t> (assignIndex)].load (std::memory_order_acquire));
        return MidiCCMode::Absolute;
    }

    // MIDI-Mappings als eigene XML-Datei speichern & laden
    juce::File getMidiMapDirectory() const;
    bool saveMidiMappingToFile (const juce::File& file) const;
    bool loadMidiMappingFromFile (const juce::File& file);
	
	void startMidiLearnForAssign (int assignIndex) noexcept
    {
        if (assignIndex >= 0 && assignIndex < numAssignKnobs)
            midiLearnTargetSlot.store (assignIndex, std::memory_order_release);
    }

    void clearMidiLearnForAssign (int assignIndex) noexcept
    {
        if (assignIndex >= 0 && assignIndex < numAssignKnobs)
        {
            const auto idx = static_cast<size_t> (assignIndex);
            assignKnobMidiCC[idx].store (-1, std::memory_order_release);
            assignPickupLocked[idx].store (false, std::memory_order_release);
            lastMidiCCNorm[idx].store (-1.0f, std::memory_order_release);
			assignKnobCCMode[idx].store (static_cast<int> (MidiCCMode::Absolute), std::memory_order_relaxed);

            if (midiLearnTargetSlot.load (std::memory_order_acquire) == assignIndex)
                midiLearnTargetSlot.store (-1, std::memory_order_release);
        }
    }

    // Preset-Verwaltung & Engine-Panic
    juce::File getPresetDirectory() const;
    void ensureDefaultPresetExists();
    juce::StringArray getAvailablePresets() const;
    bool savePreset (const juce::String& presetName);
    bool loadPresetByName (const juce::String& presetName);
    bool loadPresetFromFile (const juce::File& file, bool importToLibrary);
    void loadInitPatch();
    void panicReset();

    juce::String getCurrentPresetName() const { return currentPresetName; }
    bool isCurrentPresetSaved() const noexcept { return currentPresetIsSaved; }

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

    struct FreeEgParamPointers
    {
        std::atomic<float>* target = nullptr;
        std::atomic<float>* mode   = nullptr;
        std::atomic<float>* sync   = nullptr;
        std::atomic<float>* time   = nullptr;
        std::atomic<float>* amount = nullptr;
        std::atomic<float>* smooth = nullptr;
        std::array<std::atomic<float>*, 6> ptTime  {};
        std::array<std::atomic<float>*, 6> ptLevel {};
    };

    struct StepSeqParamPointers
    {
        std::atomic<float>* mode   = nullptr;
        std::atomic<float>* length = nullptr;
        std::atomic<float>* rate   = nullptr;
        std::atomic<float>* gate   = nullptr;
        std::atomic<float>* swing  = nullptr;
        std::atomic<float>* amount = nullptr;
        std::array<std::atomic<float>*, 16> stepVal  {};
        std::array<std::atomic<float>*, 16> stepGate {};
    };

    struct LfoParamPointers
    {
        std::atomic<float>* sync  = nullptr;
        std::atomic<float>* rate  = nullptr;
        std::atomic<float>* fade  = nullptr;
        std::atomic<float>* depth = nullptr;
    };

    struct ModSlotParamPointers
    {
        std::atomic<float>* src    = nullptr;
        std::atomic<float>* dst    = nullptr;
        std::atomic<float>* amount = nullptr;
    };

    struct SceneParamPointers
    {
        std::array<OscParamPointers, 2> oscs;
        FilterParamPointers filter;
        std::array<EnvParamPointers, 2> envs;
        FreeEgParamPointers freeEg;
        StepSeqParamPointers stepSeq;
        LfoParamPointers lfo1;
        std::array<ModSlotParamPointers, 4> modSlots;
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
        std::atomic<float> currentBpm { 120.0f };
        mutable std::array<std::atomic<int>, 2> activeSeqStep { -1, -1 };
        mutable std::array<std::atomic<float>, 2> liveLfoOut { 0.0f, 0.0f };
        mutable std::atomic<float> liveEffectiveMorph { 0.5f };
    };

    int getCurrentSeqStep (int scene) const noexcept
    {
        const size_t idx = (scene == 2) ? 1 : 0;
        return paramCache.activeSeqStep[idx].load();
    }

    float getLiveLfoValue (int scene) const noexcept
    {
        const size_t idx = (scene == 2) ? 1 : 0;
        return paramCache.liveLfoOut[idx].load();
    }

    float getLiveEffectiveMorph() const noexcept { return paramCache.liveEffectiveMorph.load(); }
    float getHostBpm() const noexcept            { return paramCache.currentBpm.load(); }
    double getCurrentSampleRateHz() const noexcept { return currentSampleRateHz.load(); }

    // Telemetrie für unteres Status-Panel (thread-safe für 30 Hz UI-Timer)
    float consumePeakLeft() noexcept  { return outputPeakL.exchange (0.0f); }
    float consumePeakRight() noexcept { return outputPeakR.exchange (0.0f); }
    int getActiveVoiceCount() const noexcept { return activeVoiceCount.load(); }
    int getLastMidiNote() const noexcept     { return lastMidiNote.load(); }
    int getLastMidiVelocity() const noexcept { return lastMidiVelocity.load(); }
    bool consumeMidiActivity() noexcept      { return midiActivityFlag.exchange (false); }

    void copyScopeSamples (std::array<float, scopeBufferSize>& dest) const noexcept
    {
        const int writeIdx = scopeWritePos.load (std::memory_order_acquire);
        for (int i = 0; i < scopeBufferSize; ++i)
        {
            const int srcIdx = (writeIdx + i) % scopeBufferSize;
            dest[static_cast<size_t> (i)] = scopeRingBuffer[static_cast<size_t> (srcIdx)].load (std::memory_order_relaxed);
        }
    }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void cacheParameterPointers();

    EngineParamCache paramCache;
    juce::Synthesiser synth;

    juce::String currentPresetName { "Init" };
    bool currentPresetIsSaved = false;
    std::atomic<bool> panicRequested { false };

    // Audio- & MIDI-Telemetrie
    std::atomic<double> currentSampleRateHz { 44100.0 };
    std::atomic<float> outputPeakL { 0.0f };
    std::atomic<float> outputPeakR { 0.0f };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<int> lastMidiNote { -1 };
    std::atomic<int> lastMidiVelocity { 0 };
    std::atomic<bool> midiActivityFlag { false };

    std::array<std::atomic<float>, scopeBufferSize> scopeRingBuffer {};
    std::atomic<int> scopeWritePos { 0 };
    int scopeDecimator = 0;

	std::array<std::atomic<int>, numAssignKnobs> assignKnobMidiCC {};
    std::atomic<int> midiLearnTargetSlot   { -1 };
    std::atomic<int> lastLearnedAssignSlot { -1 };
	std::atomic<bool> midiPickupEnabled { true }; // Standardmäßig aktiv
    std::array<std::atomic<bool>, numAssignKnobs> assignPickupLocked {};
    std::array<std::atomic<float>, numAssignKnobs> lastMidiCCNorm {};
	std::array<std::atomic<int>, numAssignKnobs> assignKnobCCMode {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeomorphX1AudioProcessor)
};
