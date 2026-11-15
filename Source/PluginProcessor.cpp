#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
    // Schneller Stereo-SVF (State Variable Filter) für 12dB und 24dB Kaskade
    struct StereoSVF
    {
        float ic1eqL = 0.0f, ic2eqL = 0.0f;
        float ic1eqR = 0.0f, ic2eqR = 0.0f;

        void reset()
        {
            ic1eqL = ic2eqL = 0.0f;
            ic1eqR = ic2eqR = 0.0f;
        }

        void process (float& left, float& right, float cutoffHz, float resonance, double sampleRate, int mode)
        {
            const float maxCut = static_cast<float> (sampleRate) * 0.45f;
            const float fc     = juce::jlimit (20.0f, maxCut, cutoffHz);
            const float g      = std::tan (juce::MathConstants<float>::pi * fc / static_cast<float> (sampleRate));
            const float k      = 2.0f - 1.92f * juce::jlimit (0.0f, 0.98f, resonance);
            const float a1     = 1.0f / (1.0f + g * (g + k));
            const float a2     = g * a1;
            const float a3     = g * a2;

            auto filterChannel = [k, a1, a2, a3, mode] (float v0, float& ic1, float& ic2) -> float
            {
                const float v3 = v0 - ic2;
                const float v1 = a1 * ic1 + a2 * v3;
                const float v2 = ic2 + a2 * ic1 + a3 * v3;
                ic1 = 2.0f * v1 - ic1;
                ic2 = 2.0f * v2 - ic2;

                switch (mode)
                {
                    case 0: // LP 12dB
                    case 1: // LP 24dB (2. Stufe übernimmt Kaskade)
                        return v2;
                    case 2: // BP 12dB
                        return v1;
                    case 3: // HP 12dB
                        return v0 - k * v1 - v2;
                    case 4: // Notch
                        return v0 - k * v1;
                    default: // Comb / Formant Charakter
                        return 0.65f * v1 + 0.35f * (v0 - v2);
                }
            };

            left  = filterChannel (left,  ic1eqL, ic2eqL);
            right = filterChannel (right, ic1eqR, ic2eqR);
        }
    };

    inline float evaluateWavetable (int waveType, float phase, float pos, float morph)
    {
        const float t = phase - std::floor (phase);
        float sample  = 0.0f;

        switch (waveType)
        {
            case 0: // Basic Analog (Sine -> Tri -> Saw -> Square)
            {
                const float sine = std::sin (t * juce::MathConstants<float>::twoPi);
                const float tri  = 1.0f - 4.0f * std::abs (std::round (t - 0.25f) - (t - 0.25f));
                const float saw  = 2.0f * t - 1.0f;
                const float sqr  = (t < 0.5f) ? 1.0f : -1.0f;

                if (pos < 0.33f)
                    sample = juce::jmap (pos / 0.33f, sine, tri);
                else if (pos < 0.66f)
                    sample = juce::jmap ((pos - 0.33f) / 0.33f, tri, saw);
                else
                    sample = juce::jmap ((pos - 0.66f) / 0.34f, saw, sqr);
                break;
            }
            case 1: // PWM / Pulse
            {
                const float pw = juce::jmap (pos, 0.08f, 0.92f);
                sample = (t < pw) ? 0.85f : -0.85f;
                break;
            }
            case 2: // Formant
            {
                const float harmonic = 2.0f + pos * 7.0f;
                const float window   = std::sin (t * juce::MathConstants<float>::pi);
                sample = std::sin (t * juce::MathConstants<float>::twoPi * harmonic) * window;
                break;
            }
            case 3: // Digital Fold
            {
                const float driven = std::sin (t * juce::MathConstants<float>::twoPi) * (1.0f + pos * 3.5f);
                sample = std::sin (driven * juce::MathConstants<float>::halfPi);
                break;
            }
            case 4: // SuperSaw
            {
                const float s1 = 2.0f * t - 1.0f;
                const float s2 = 2.0f * std::fmod (t * 1.012f + 0.17f, 1.0f) - 1.0f;
                const float s3 = 2.0f * std::fmod (t * 0.988f + 0.41f, 1.0f) - 1.0f;
                sample = s1 * (1.0f - pos * 0.45f) + (s2 + s3) * (pos * 0.34f);
                break;
            }
            default: // Vocal Morph
            {
                sample = 0.6f * std::sin (t * juce::MathConstants<float>::twoPi)
                       + 0.4f * std::cos (t * juce::MathConstants<float>::twoPi * (2.0f + pos * 5.0f));
                break;
            }
        }

        if (morph > 0.001f)
            sample = std::sin (sample * (1.0f + morph * 2.2f));

        return sample;
    }

    class NeomorphSound : public juce::SynthesiserSound
    {
    public:
        bool appliesToNote (int) override    { return true; }
        bool appliesToChannel (int) override { return true; }
    };

    class NeomorphVoice : public juce::SynthesiserVoice
    {
    public:
        explicit NeomorphVoice (const NeomorphX1AudioProcessor::EngineParamCache& cacheRef)
            : cache (cacheRef) {}

        bool canPlaySound (juce::SynthesiserSound* sound) override
        {
            return dynamic_cast<NeomorphSound*> (sound) != nullptr;
        }

        void setCurrentPlaybackSampleRate (double newRate) override
        {
            juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
            if (newRate > 0.0)
            {
                for (auto& s : sceneStates)
                {
                    s.ampEnv.setSampleRate (newRate);
                    s.modEnv.setSampleRate (newRate);
                }
            }
        }

        void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition) override
        {
            currentMidiNote = midiNoteNumber;
            noteVelocity    = velocity;
            pitchWheelNorm  = (static_cast<float> (currentPitchWheelPosition) - 8192.0f) / 8192.0f;

            for (size_t s = 0; s < 2; ++s)
            {
                auto& state = sceneStates[s];
                const auto& scParams = cache.scenes[s];

                for (size_t o = 0; o < 2; ++o)
                {
                    const float phaseDeg = scParams.oscs[o].phase->load();
                    state.oscPhase[o] = phaseDeg / 360.0f;
                }

                state.svfStage1.reset();
                state.svfStage2.reset();

                updateEnvelopeParams (s);
                state.ampEnv.noteOn();
                state.modEnv.noteOn();
            }
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff)
            {
                for (size_t s = 0; s < 2; ++s)
                {
                    updateEnvelopeParams (s);
                    sceneStates[s].ampEnv.noteOff();
                    sceneStates[s].modEnv.noteOff();
                }
            }
            else
            {
                for (auto& s : sceneStates)
                {
                    s.ampEnv.reset();
                    s.modEnv.reset();
                }
                clearCurrentNote();
            }
        }

        void pitchWheelMoved (int newPitchWheelValue) override
        {
            pitchWheelNorm = (static_cast<float> (newPitchWheelValue) - 8192.0f) / 8192.0f;
        }

        void controllerMoved (int, int) override {}

        void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
        {
            if (! isVoiceActive())
                return;

            const double sr = getSampleRate();
            if (sr <= 0.0)
                return;

            for (size_t s = 0; s < 2; ++s)
                updateEnvelopeParams (s);

            const float s1Gain    = cache.scene1Level->load();
            const float morphVal  = cache.sceneMorph->load();
            const float s2Gain    = cache.scene2Level->load();
            const float uiPitch   = cache.pitchWheel->load();
            const float modWheel  = cache.modWheel->load();

            // Kombinierter Pitchbend (MIDI Hardware + UI-Pitchwheel, +/- 2 Halbtöne)
            const float totalPitchSemi = juce::jlimit (-1.0f, 1.0f, pitchWheelNorm + uiPitch) * 2.0f;

            // Crossfade zwischen Scene 1 und Scene 2
            const float weight1 = s1Gain * (1.0f - morphVal);
            const float weight2 = s2Gain * morphVal;

            auto* outL = outputBuffer.getWritePointer (0);
            auto* outR = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer (1) : nullptr;

            for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
            {
                float mixL = 0.0f;
                float mixR = 0.0f;

                for (size_t s = 0; s < 2; ++s)
                {
                    auto& state     = sceneStates[s];
                    const auto& scP = cache.scenes[s];

                    const float ampEnvVal = state.ampEnv.getNextSample();
                    const float modEnvVal = state.modEnv.getNextSample();

                    float osc1L = 0.0f, osc1R = 0.0f;
                    float osc2L = 0.0f, osc2R = 0.0f;

                    for (size_t o = 0; o < 2; ++o)
                    {
                        const auto& oscP = scP.oscs[o];
                        const int wave   = static_cast<int> (oscP.wave->load());
                        const float oct  = oscP.octave->load();
                        const float semi = oscP.semi->load();
                        const float det  = oscP.detune->load() * 0.01f;
                        const float pos  = oscP.position->load();
                        const float mrph = juce::jlimit (0.0f, 1.0f, oscP.morph->load() + modWheel * 0.35f);
                        const float pan  = oscP.pan->load();
                        const float lvl  = oscP.level->load();

                        const float notePitch = static_cast<float> (currentMidiNote)
                                              + (oct * 12.0f) + semi + det + totalPitchSemi;
                        const float freqHz = 440.0f * std::pow (2.0f, (notePitch - 69.0f) / 12.0f);

                        state.oscPhase[o] += freqHz / static_cast<float> (sr);
                        if (state.oscPhase[o] >= 1.0f)
                            state.oscPhase[o] -= std::floor (state.oscPhase[o]);

                        const float raw = evaluateWavetable (wave, state.oscPhase[o], pos, mrph) * lvl;

                        // Constant-Power Stereo-Panning
                        const float panAngle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
                        const float lGain    = std::cos (panAngle);
                        const float rGain    = std::sin (panAngle);

                        if (o == 0)
                        {
                            osc1L = raw * lGain;
                            osc1R = raw * rGain;
                        }
                        else
                        {
                            osc2L = raw * lGain;
                            osc2R = raw * rGain;
                        }
                    }

                    // Filter-Routing & Multimode-Filterung pro Scene
                    const int fltType    = static_cast<int> (scP.filter.type->load());
                    const int fltRouting = static_cast<int> (scP.filter.routing->load());

                    float fltInL = 0.0f, fltInR = 0.0f;
                    float bypassL = 0.0f, bypassR = 0.0f;

                    if (fltRouting == 0) // Osc 1 + 2
                    {
                        fltInL = osc1L + osc2L;
                        fltInR = osc1R + osc2R;
                    }
                    else if (fltRouting == 1) // Osc 1 Only (Osc 2 geht trocken vorbei)
                    {
                        fltInL  = osc1L;
                        fltInR  = osc1R;
                        bypassL = osc2L;
                        bypassR = osc2R;
                    }
                    else // Bypass
                    {
                        bypassL = osc1L + osc2L;
                        bypassR = osc1R + osc2R;
                    }

                    if (fltRouting != 2)
                    {
                        const float baseCut  = scP.filter.cutoff->load();
                        const float res      = scP.filter.resonance->load();
                        const float drive    = scP.filter.drive->load();
                        const float envAmt   = scP.filter.envAmt->load();
                        const float keytrack = scP.filter.keytrack->load();
                        const float velSens  = scP.filter.velSens->load();

                        // Sättigung (Drive) vor dem Filter
                        if (drive > 0.001f)
                        {
                            const float dGain = 1.0f + drive * 4.5f;
                            fltInL = std::tanh (fltInL * dGain) / std::tanh (dGain);
                            fltInR = std::tanh (fltInR * dGain) / std::tanh (dGain);
                        }

                        // Cutoff-Modulation in Oktaven (Envelope, Keytracking, Velocity, ModWheel)
                        const float keyOctaves = ((static_cast<float> (currentMidiNote) - 60.0f) / 12.0f) * keytrack;
                        const float envOctaves = modEnvVal * envAmt * 5.0f;
                        const float velOctaves = (noteVelocity - 0.5f) * velSens * 3.0f;
                        const float modOctaves = modWheel * 2.5f;

                        const float modCut = baseCut * std::pow (2.0f, keyOctaves + envOctaves + velOctaves + modOctaves);

                        state.svfStage1.process (fltInL, fltInR, modCut, res, sr, fltType);
                        if (fltType == 1) // 24 dB Ladder = zweite SVF-Stufe in Serie
                            state.svfStage2.process (fltInL, fltInR, modCut, res * 0.45f, sr, 0);
                    }

                    const float sceneWeight = (s == 0) ? weight1 : weight2;
                    const float voiceGain   = ampEnvVal * noteVelocity * sceneWeight * 0.28f;

                    mixL += (fltInL + bypassL) * voiceGain;
                    mixR += (fltInR + bypassR) * voiceGain;
                }

                // Soft-Limiter für Polyphonie-Headroom
                outL[startSample + sampleIdx] += std::tanh (mixL);
                if (outR != nullptr)
                    outR[startSample + sampleIdx] += std::tanh (mixR);

                if (! sceneStates[0].ampEnv.isActive() && ! sceneStates[1].ampEnv.isActive())
                {
                    clearCurrentNote();
                    break;
                }
            }
        }

    private:
        struct SceneVoiceState
        {
            std::array<float, 2> oscPhase { 0.0f, 0.0f };
            StereoSVF svfStage1;
            StereoSVF svfStage2;
            juce::ADSR ampEnv;
            juce::ADSR modEnv;
        };

        void updateEnvelopeParams (size_t sceneIdx)
        {
            const auto& scP = cache.scenes[sceneIdx];
            auto& state     = sceneStates[sceneIdx];

            juce::ADSR::Parameters ampP;
            ampP.attack  = scP.envs[0].attack->load();
            ampP.decay   = scP.envs[0].decay->load();
            ampP.sustain = scP.envs[0].sustain->load();
            ampP.release = scP.envs[0].release->load();
            state.ampEnv.setParameters (ampP);

            juce::ADSR::Parameters modP;
            modP.attack  = scP.envs[1].attack->load();
            modP.decay   = scP.envs[1].decay->load();
            modP.sustain = scP.envs[1].sustain->load();
            modP.release = scP.envs[1].release->load();
            state.modEnv.setParameters (modP);
        }

        const NeomorphX1AudioProcessor::EngineParamCache& cache;
        std::array<SceneVoiceState, 2> sceneStates;
        int currentMidiNote  = 60;
        float noteVelocity   = 0.0f;
        float pitchWheelNorm = 0.0f;
    };
}

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
    cacheParameterPointers();

    synth.addSound (new NeomorphSound());
    for (int i = 0; i < 16; ++i)
        synth.addVoice (new NeomorphVoice (paramCache));
}

NeomorphX1AudioProcessor::~NeomorphX1AudioProcessor()
{
}

void NeomorphX1AudioProcessor::cacheParameterPointers()
{
    paramCache.scene1Level = apvts.getRawParameterValue ("scene_1");
    paramCache.sceneMorph  = apvts.getRawParameterValue ("scene_morph");
    paramCache.scene2Level = apvts.getRawParameterValue ("scene_2");
    paramCache.pitchWheel  = apvts.getRawParameterValue ("pitch_wheel");
    paramCache.modWheel    = apvts.getRawParameterValue ("mod_wheel");

    for (size_t i = 0; i < 10; ++i)
        paramCache.matrixKnobs[i] = apvts.getRawParameterValue ("knob_" + juce::String (static_cast<int> (i + 1)));

    for (size_t s = 0; s < 2; ++s)
    {
        const auto scenePrefix = "s" + juce::String (static_cast<int> (s + 1)) + "_";
        auto& sc = paramCache.scenes[s];

        for (size_t o = 0; o < 2; ++o)
        {
            const auto p = scenePrefix + "osc" + juce::String (static_cast<int> (o + 1)) + "_";
            sc.oscs[o].wave     = apvts.getRawParameterValue (p + "wave");
            sc.oscs[o].octave   = apvts.getRawParameterValue (p + "octave");
            sc.oscs[o].semi     = apvts.getRawParameterValue (p + "semi");
            sc.oscs[o].detune   = apvts.getRawParameterValue (p + "detune");
            sc.oscs[o].position = apvts.getRawParameterValue (p + "position");
            sc.oscs[o].morph    = apvts.getRawParameterValue (p + "morph");
            sc.oscs[o].phase    = apvts.getRawParameterValue (p + "phase");
            sc.oscs[o].pan      = apvts.getRawParameterValue (p + "pan");
            sc.oscs[o].level    = apvts.getRawParameterValue (p + "level");
        }

        const auto fp = scenePrefix + "flt_";
        sc.filter.type      = apvts.getRawParameterValue (fp + "type");
        sc.filter.routing   = apvts.getRawParameterValue (fp + "routing");
        sc.filter.cutoff    = apvts.getRawParameterValue (fp + "cutoff");
        sc.filter.resonance = apvts.getRawParameterValue (fp + "resonance");
        sc.filter.drive     = apvts.getRawParameterValue (fp + "drive");
        sc.filter.envAmt    = apvts.getRawParameterValue (fp + "env_amt");
        sc.filter.keytrack  = apvts.getRawParameterValue (fp + "keytrack");
        sc.filter.velSens   = apvts.getRawParameterValue (fp + "vel_sens");

        for (size_t e = 0; e < 2; ++e)
        {
            const auto ep = scenePrefix + "env" + juce::String (static_cast<int> (e + 1)) + "_";
            sc.envs[e].attack  = apvts.getRawParameterValue (ep + "attack");
            sc.envs[e].decay   = apvts.getRawParameterValue (ep + "decay");
            sc.envs[e].sustain = apvts.getRawParameterValue (ep + "sustain");
            sc.envs[e].release = apvts.getRawParameterValue (ep + "release");
        }
    }
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
        0.8f, 0.5f,
        0.0f, 0.0f,
        0.0f, 0.0f,
        0.0f, 0.0f,
        0.0f, 0.0f
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

        // 4c. Filter Tab (s1_flt_* & s2_flt_*)
        const auto fltPrefix = scenePrefix + "flt_";
        const auto fltLabel  = sceneLabel + "Filter ";

        const juce::StringArray filterTypes = {
            "LP 12dB", "LP 24dB Ladder", "BP 12dB", "HP 12dB", "Notch", "Comb / Formant"
        };

        const juce::StringArray filterRoutings = {
            "Osc 1 + 2", "Osc 1 Only", "Bypass"
        };

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { fltPrefix + "type", 1 }, fltLabel + "Type",
            filterTypes, (scene == 1) ? 1 : 2));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { fltPrefix + "routing", 1 }, fltLabel + "Routing",
            filterRoutings, 0));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { fltPrefix + "cutoff", 1 }, fltLabel + "Cutoff",
            juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.25f),
            (scene == 1) ? 4200.0f : 1800.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { fltPrefix + "resonance", 1 }, fltLabel + "Resonance",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
            (scene == 1) ? 0.22f : 0.45f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { fltPrefix + "drive", 1 }, fltLabel + "Drive",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.15f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { fltPrefix + "env_amt", 1 }, fltLabel + "Env Amount",
            juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f),
            (scene == 1) ? 0.35f : -0.25f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { fltPrefix + "keytrack", 1 }, fltLabel + "Keytrack",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.50f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { fltPrefix + "vel_sens", 1 }, fltLabel + "Velocity",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f));
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
    juce::ignoreUnused (samplesPerBlock);
    synth.setCurrentPlaybackSampleRate (sampleRate);
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

    // UI-Wheels mit eingehenden Hardware-MIDI-Wheels synchronisieren
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

    // 16-stimmige Dual-Scene-Synthese rendern
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
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
