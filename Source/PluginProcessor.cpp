#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
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
                    case 0:
                    case 1:
                        return v2;
                    case 2:
                        return v1;
                    case 3:
                        return v0 - k * v1 - v2;
                    case 4:
                        return v0 - k * v1;
                    default:
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
            case 0:
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
            case 1:
            {
                const float pw = juce::jmap (pos, 0.08f, 0.92f);
                sample = (t < pw) ? 0.85f : -0.85f;
                break;
            }
            case 2:
            {
                const float harmonic = 2.0f + pos * 7.0f;
                const float window   = std::sin (t * juce::MathConstants<float>::pi);
                sample = std::sin (t * juce::MathConstants<float>::twoPi * harmonic) * window;
                break;
            }
            case 3:
            {
                const float driven = std::sin (t * juce::MathConstants<float>::twoPi) * (1.0f + pos * 3.5f);
                sample = std::sin (driven * juce::MathConstants<float>::halfPi);
                break;
            }
            case 4:
            {
                const float s1 = 2.0f * t - 1.0f;
                const float s2 = 2.0f * std::fmod (t * 1.012f + 0.17f, 1.0f) - 1.0f;
                const float s3 = 2.0f * std::fmod (t * 0.988f + 0.41f, 1.0f) - 1.0f;
                sample = s1 * (1.0f - pos * 0.45f) + (s2 + s3) * (pos * 0.34f);
                break;
            }
            default:
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

    inline float evaluateFreeEg (const NeomorphX1AudioProcessor::FreeEgParamPointers& feg,
                                 float& phase, float& smoothed, bool released, int& dir,
                                 double sampleRate, float hostBpm)
    {
        const int target = static_cast<int> (feg.target->load());
        const float amt  = feg.amount->load();
        if (target == 0 && std::abs (amt) < 0.0001f)
            return 0.0f;

        const int mode = static_cast<int> (feg.mode->load());
        const int sync = static_cast<int> (feg.sync->load());

        float totalSec = feg.time->load();
        if (sync > 0 && hostBpm > 1.0f)
        {
            constexpr float beats[6] = { 1.0f, 4.0f, 2.0f, 1.0f, 0.5f, 0.25f };
            totalSec = (60.0f / hostBpm) * beats[juce::jlimit (0, 5, sync)];
        }

        const float step     = 1.0f / juce::jmax (1.0f, totalSec * static_cast<float> (sampleRate));
        const float sustainT = feg.ptTime[4]->load();

        if (! released)
        {
            if (mode == 0)
                phase = juce::jmin (1.0f, phase + step);
            else if (mode == 1)
                phase = juce::jmin (sustainT, phase + step);
            else if (mode == 2)
            {
                phase += step;
                if (sustainT > 0.02f && phase >= sustainT)
                    phase = std::fmod (phase, sustainT);
            }
            else
            {
                phase += step * static_cast<float> (dir);
                if (phase >= sustainT)      { phase = sustainT; dir = -1; }
                else if (phase <= 0.0f)     { phase = 0.0f;     dir = 1;  }
            }
        }
        else
        {
            phase = juce::jmin (1.0f, phase + step);
        }

        float rawVal = feg.ptLevel[5]->load();
        for (size_t i = 0; i < 5; ++i)
        {
            const float t0 = feg.ptTime[i]->load();
            const float t1 = feg.ptTime[i + 1]->load();
            if (phase <= t1 || i == 4)
            {
                const float span = juce::jmax (0.0001f, t1 - t0);
                const float frac = juce::jlimit (0.0f, 1.0f, (phase - t0) / span);
                const float v0   = feg.ptLevel[i]->load();
                const float v1   = feg.ptLevel[i + 1]->load();
                rawVal = v0 + frac * (v1 - v0);
                break;
            }
        }

        const float smooth = feg.smooth->load();
        const float coeff  = 1.0f - std::pow (smooth, 0.25f) * 0.995f;
        smoothed += coeff * (rawVal - smoothed);

        return smoothed * amt;
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
            noteIsHeld      = true;

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

                state.freeEgPhase    = 0.0f;
                state.freeEgSmoothed = scParams.freeEg.ptLevel[0]->load();
                state.freeEgReleased = false;
                state.freeEgDir      = 1;

                state.seqPhase       = 0.0f;
                state.seqCurrentStep = 0;
                state.seqGateOpen    = (scParams.stepSeq.stepGate[0]->load() > 0.5f);
                state.seqSmoothedVal = scParams.stepSeq.stepVal[0]->load();

                state.lfoPhase    = 0.0f;
                state.lfoFadeGain = (scParams.lfo1.fade->load() <= 0.005f) ? 1.0f : 0.0f;

                cache.activeSeqStep[s].store (0);

                updateEnvelopeParams (s);
                state.ampEnv.noteOn();
                state.modEnv.noteOn();
            }
        }

        void stopNote (float, bool allowTailOff) override
        {
            noteIsHeld = false;

            if (allowTailOff)
            {
                for (size_t s = 0; s < 2; ++s)
                {
                    updateEnvelopeParams (s);
                    sceneStates[s].ampEnv.noteOff();
                    sceneStates[s].modEnv.noteOff();
                    sceneStates[s].freeEgReleased = true;
                }
            }
            else
            {
                for (size_t s = 0; s < 2; ++s)
                {
                    sceneStates[s].svfStage1.reset();
                    sceneStates[s].svfStage2.reset();
                    sceneStates[s].ampEnv.reset();
                    sceneStates[s].modEnv.reset();
                    sceneStates[s].freeEgReleased = true;
                    cache.activeSeqStep[s].store (-1);
                    cache.liveLfoOut[s].store (0.0f);
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
            const float baseMorph = cache.sceneMorph->load();
            const float s2Gain    = cache.scene2Level->load();
            const float uiPitch   = cache.pitchWheel->load();
            const float modWheel  = cache.modWheel->load();
            const float hostBpm   = juce::jmax (20.0f, cache.currentBpm.load());

            const float totalPitchSemi = juce::jlimit (-1.0f, 1.0f, pitchWheelNorm + uiPitch) * 2.0f;

            auto* outL = outputBuffer.getWritePointer (0);
            auto* outR = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer (1) : nullptr;

            constexpr int   seqLengths[3]  = { 8, 12, 16 };
            constexpr float seqBeatDivs[6] = { 1.0f, 0.5f, 0.75f, 0.25f, 1.0f / 6.0f, 0.125f };
            constexpr float lfoBeatDivs[6] = { 1.0f, 4.0f, 2.0f, 1.0f, 0.5f, 0.25f };

            for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
            {
				std::array<float, 2> ampEnvArr { 0.0f, 0.0f };
                std::array<float, 2> modEnvArr { 0.0f, 0.0f };
                std::array<float, 2> fegRawVal { 0.0f, 0.0f };
                std::array<int, 2>   fegTarget { 0, 0 };
                std::array<float, 2> seqRawVal { 0.0f, 0.0f };
                std::array<int, 2>   seqMode   { 0, 0 };
                std::array<float, 2> lfoSine   { 0.0f, 0.0f };
                std::array<float, 2> lfoTri    { 0.0f, 0.0f };
                std::array<float, 2> lfoSaw    { 0.0f, 0.0f };

                float morphModOffset = 0.0f;
                const float keyNorm  = (static_cast<float> (currentMidiNote) - 60.0f) / 36.0f;

                // 1. Envelopes, Free EG & Step Sequencer pro Scene auswerten
                for (size_t s = 0; s < 2; ++s)
                {
                    auto& state     = sceneStates[s];
                    const auto& scP = cache.scenes[s];

                    ampEnvArr[s] = state.ampEnv.getNextSample();
                    modEnvArr[s] = state.modEnv.getNextSample();

                    fegTarget[s] = static_cast<int> (scP.freeEg.target->load());
                    fegRawVal[s] = evaluateFreeEg (scP.freeEg,
                                                   state.freeEgPhase,
                                                   state.freeEgSmoothed,
                                                   state.freeEgReleased,
                                                   state.freeEgDir,
                                                   sr, hostBpm);
                    if (fegTarget[s] == 5)
                        morphModOffset += fegRawVal[s] * 0.5f;

                    seqMode[s] = static_cast<int> (scP.stepSeq.mode->load());
                    if (seqMode[s] > 0)
                    {
                        const int numSteps  = seqLengths[juce::jlimit (0, 2, static_cast<int> (scP.stepSeq.length->load()))];
                        const int rateIdx   = juce::jlimit (0, 5, static_cast<int> (scP.stepSeq.rate->load()));
                        const float gateLen = scP.stepSeq.gate->load();
                        const float swing   = scP.stepSeq.swing->load();
                        const float amt     = scP.stepSeq.amount->load();

                        const float stepSec = (60.0f / hostBpm) * seqBeatDivs[rateIdx];
                        state.seqPhase += 1.0f / juce::jmax (1.0f, stepSec * static_cast<float> (sr));

                        const float swingOffset = ((state.seqCurrentStep % 2) == 1) ? (swing * 0.35f) : 0.0f;
                        const float effectivePhase = state.seqPhase - swingOffset;

                        if (state.seqPhase >= 1.0f)
                        {
                            state.seqPhase -= 1.0f;
                            state.seqCurrentStep = (state.seqCurrentStep + 1) % numSteps;
                            cache.activeSeqStep[s].store (state.seqCurrentStep);

                            const bool stepActive = (scP.stepSeq.stepGate[static_cast<size_t> (state.seqCurrentStep)]->load() > 0.5f);
                            if (noteIsHeld && stepActive)
                            {
                                state.seqGateOpen = true;
                                if (seqMode[s] == 1)
                                {
                                    state.ampEnv.noteOn();
                                    state.modEnv.noteOn();
                                }
                            }
                            else
                            {
                                state.seqGateOpen = false;
                            }
                        }

                        if (seqMode[s] == 1 && state.seqGateOpen && effectivePhase >= gateLen)
                        {
                            state.seqGateOpen = false;
                            state.ampEnv.noteOff();
                            state.modEnv.noteOff();
                        }

                        const size_t stIdx = static_cast<size_t> (juce::jlimit (0, 15, state.seqCurrentStep));
                        const bool active  = (scP.stepSeq.stepGate[stIdx]->load() > 0.5f);
                        const float targetStepVal = active ? scP.stepSeq.stepVal[stIdx]->load() : 0.0f;

                        const float glideCoeff = (seqMode[s] == 1)
                                                     ? (1.0f - std::pow (amt, 0.25f) * 0.996f)
                                                     : 0.02f;
                        state.seqSmoothedVal += glideCoeff * (targetStepVal - state.seqSmoothedVal);

                        seqRawVal[s] = (seqMode[s] == 1) ? state.seqSmoothedVal
                                                         : (state.seqSmoothedVal * amt);

                        if (seqMode[s] == 4)
                            morphModOffset += seqRawVal[s] * 0.5f;
                    }
                }

                // 2. LFO 1 Rate & Depth Modulationen aus der Matrix sammeln und LFO 1 berechnen
                for (size_t s = 0; s < 2; ++s)
                {
                    auto& state     = sceneStates[s];
                    const auto& scP = cache.scenes[s];

                    float lfoRateMod  = 0.0f;
                    float lfoDepthMod = 0.0f;

                    for (size_t m = 0; m < 4; ++m)
                    {
                        const int dstIdx = static_cast<int> (scP.modSlots[m].dst->load());
                        if (dstIdx != 12 && dstIdx != 13)
                            continue;

                        const int srcIdx = static_cast<int> (scP.modSlots[m].src->load());
                        const float amt  = scP.modSlots[m].amount->load();
                        if (srcIdx == 0 || std::abs (amt) < 0.0001f)
                            continue;

                        float srcVal = 0.0f;
                        switch (srcIdx)
                        {
                            case 4:  srcVal = modWheel;     break;
                            case 5:  srcVal = noteVelocity; break;
                            case 6:  srcVal = keyNorm;      break;
                            case 7:  srcVal = ampEnvArr[s]; break;
                            case 8:  srcVal = modEnvArr[s]; break;
                            case 9:  srcVal = fegRawVal[s]; break;
                            case 10: srcVal = seqRawVal[s]; break;
                            default:
                                if (srcIdx >= 11 && srcIdx <= 18)
                                    srcVal = cache.matrixKnobs[static_cast<size_t> (srcIdx - 9)]->load();
                                break;
                        }

                        if (dstIdx == 12)
                            lfoRateMod += srcVal * amt;
                        else if (dstIdx == 13)
                            lfoDepthMod += srcVal * amt;
                    }

                    const int lfoSync  = static_cast<int> (scP.lfo1.sync->load());
                    const float fadeT  = scP.lfo1.fade->load();
                    const float depth  = juce::jlimit (0.0f, 1.0f, scP.lfo1.depth->load() + lfoDepthMod);

                    float lfoFreqHz = scP.lfo1.rate->load() * std::pow (2.0f, lfoRateMod * 3.0f);
                    if (lfoSync > 0 && hostBpm > 1.0f)
                    {
                        const float periodSec = (60.0f / hostBpm) * lfoBeatDivs[juce::jlimit (0, 5, lfoSync)];
                        lfoFreqHz = 1.0f / juce::jmax (0.005f, periodSec);
                    }

                    state.lfoPhase += lfoFreqHz / static_cast<float> (sr);
                    if (state.lfoPhase >= 1.0f)
                        state.lfoPhase -= std::floor (state.lfoPhase);

                    if (fadeT > 0.005f)
                        state.lfoFadeGain = juce::jmin (1.0f, state.lfoFadeGain + 1.0f / (fadeT * static_cast<float> (sr)));
                    else
                        state.lfoFadeGain = 1.0f;

                    const float envLfo = state.lfoFadeGain * depth;
                    const float p      = state.lfoPhase;

                    lfoSine[s] = std::sin (p * juce::MathConstants<float>::twoPi) * envLfo;
                    lfoTri[s]  = (1.0f - 4.0f * std::abs (std::round (p - 0.25f) - (p - 0.25f))) * envLfo;
                    lfoSaw[s]  = (2.0f * p - 1.0f) * envLfo;

                    if (sampleIdx == 0)
                        cache.liveLfoOut[s].store (lfoSine[s]);
                }

                struct SceneModAccum
                {
                    float cutoffOct = 0.0f;
                    float resonance = 0.0f;
                    float osc1Semi  = 0.0f;
                    float osc2Semi  = 0.0f;
                    float osc1Pos   = 0.0f;
                    float osc2Pos   = 0.0f;
                    float oscMorph  = 0.0f;
                    float osc1Lvl   = 0.0f;
                    float osc2Lvl   = 0.0f;
                };
                std::array<SceneModAccum, 2> matMod {};

                for (size_t s = 0; s < 2; ++s)
                {
                    const auto& scP = cache.scenes[s];
                    for (size_t m = 0; m < 4; ++m)
                    {
                        const int srcIdx = static_cast<int> (scP.modSlots[m].src->load());
                        const int dstIdx = static_cast<int> (scP.modSlots[m].dst->load());
                        const float amt  = scP.modSlots[m].amount->load();

                        if (srcIdx == 0 || dstIdx == 0 || std::abs (amt) < 0.0001f)
                            continue;

                        float srcVal = 0.0f;
                        switch (srcIdx)
                        {
                            case 1:  srcVal = lfoSine[s]; break;
                            case 2:  srcVal = lfoTri[s];  break;
                            case 3:  srcVal = lfoSaw[s];  break;
                            case 4:  srcVal = modWheel;   break;
                            case 5:  srcVal = noteVelocity; break;
                            case 6:  srcVal = keyNorm;    break;
                            case 7:  srcVal = ampEnvArr[s]; break;
                            case 8:  srcVal = modEnvArr[s]; break;
                            case 9:  srcVal = fegRawVal[s]; break;
                            case 10: srcVal = seqRawVal[s]; break;
                            default:
                                if (srcIdx >= 11 && srcIdx <= 18)
                                    srcVal = cache.matrixKnobs[static_cast<size_t> (srcIdx - 9)]->load();
                                break;
                        }

                        const float modVal = srcVal * amt;
                        auto& acc = matMod[s];

                        switch (dstIdx)
                        {
                            case 1:  acc.cutoffOct += modVal * 4.5f;  break;
                            case 2:  acc.resonance += modVal * 0.85f; break;
                            case 3:  acc.osc1Semi  += modVal * 12.0f; break;
                            case 4:  acc.osc2Semi  += modVal * 12.0f; break;
                            case 5:  acc.osc1Semi  += modVal * 12.0f;
                                     acc.osc2Semi  += modVal * 12.0f; break;
                            case 6:  acc.osc1Pos   += modVal;         break;
                            case 7:  acc.osc2Pos   += modVal;         break;
                            case 8:  acc.oscMorph  += modVal;         break;
                            case 9:  acc.osc1Lvl   += modVal;         break;
                            case 10: acc.osc2Lvl   += modVal;         break;
                            case 11: morphModOffset += modVal * 0.5f; break;
                            default: break;
                        }
                    }
                }

                const float effectiveMorph = juce::jlimit (0.0f, 1.0f, baseMorph + morphModOffset);
                const float weight1 = s1Gain * (1.0f - effectiveMorph);
                const float weight2 = s2Gain * effectiveMorph;

                float mixL = 0.0f;
                float mixR = 0.0f;

                for (size_t s = 0; s < 2; ++s)
                {
                    auto& state     = sceneStates[s];
                    const auto& scP = cache.scenes[s];
                    const auto& acc = matMod[s];

                    const float fegPosMod   = (fegTarget[s] == 2) ? fegRawVal[s] : 0.0f;
                    const float fegMorphMod = (fegTarget[s] == 3) ? fegRawVal[s] : 0.0f;
                    const float fegPitchMod = (fegTarget[s] == 4) ? (fegRawVal[s] * 12.0f) : 0.0f;
                    const float fegCutMod   = (fegTarget[s] == 1) ? (fegRawVal[s] * 4.0f)  : 0.0f;

                    const float seqPitchMod = (seqMode[s] == 1) ? (seqRawVal[s] * 12.0f) : 0.0f;
                    const float seqCutMod   = (seqMode[s] == 2) ? (seqRawVal[s] * 4.0f)  : 0.0f;
                    const float seqPosMod   = (seqMode[s] == 3) ? seqRawVal[s]           : 0.0f;

                    float osc1L = 0.0f, osc1R = 0.0f;
                    float osc2L = 0.0f, osc2R = 0.0f;

                    for (size_t o = 0; o < 2; ++o)
                    {
                        const auto& oscP = scP.oscs[o];
                        const int wave   = static_cast<int> (oscP.wave->load());
                        const float oct  = oscP.octave->load();
                        const float semi = oscP.semi->load();
                        const float det  = oscP.detune->load() * 0.01f;

                        const float slotPosMod   = (o == 0) ? acc.osc1Pos  : acc.osc2Pos;
                        const float slotPitchMod = (o == 0) ? acc.osc1Semi : acc.osc2Semi;
                        const float slotLvlMod   = (o == 0) ? acc.osc1Lvl  : acc.osc2Lvl;

                        const float pos  = juce::jlimit (0.0f, 1.0f, oscP.position->load() + fegPosMod + seqPosMod + slotPosMod);
                        const float mrph = juce::jlimit (0.0f, 1.0f, oscP.morph->load() + fegMorphMod + acc.oscMorph);
                        const float pan  = oscP.pan->load();
                        const float lvl  = juce::jlimit (0.0f, 1.25f, oscP.level->load() + slotLvlMod);

                        const float notePitch = static_cast<float> (currentMidiNote)
                                              + (oct * 12.0f) + semi + det + totalPitchSemi
                                              + fegPitchMod + seqPitchMod + slotPitchMod;
                        const float freqHz = 440.0f * std::pow (2.0f, (notePitch - 69.0f) / 12.0f);

                        state.oscPhase[o] += freqHz / static_cast<float> (sr);
                        if (state.oscPhase[o] >= 1.0f)
                            state.oscPhase[o] -= std::floor (state.oscPhase[o]);

                        const float raw = evaluateWavetable (wave, state.oscPhase[o], pos, mrph) * lvl;

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

                    const int fltType    = static_cast<int> (scP.filter.type->load());
                    const int fltRouting = static_cast<int> (scP.filter.routing->load());

                    float fltInL = 0.0f, fltInR = 0.0f;
                    float bypassL = 0.0f, bypassR = 0.0f;

                    if (fltRouting == 0)
                    {
                        fltInL = osc1L + osc2L;
                        fltInR = osc1R + osc2R;
                    }
                    else if (fltRouting == 1)
                    {
                        fltInL  = osc1L;
                        fltInR  = osc1R;
                        bypassL = osc2L;
                        bypassR = osc2R;
                    }
                    else
                    {
                        bypassL = osc1L + osc2L;
                        bypassR = osc1R + osc2R;
                    }

                    if (fltRouting != 2)
                    {
                        const float baseCut  = scP.filter.cutoff->load();
                        const float res      = juce::jlimit (0.0f, 0.98f, scP.filter.resonance->load() + acc.resonance);
                        const float drive    = scP.filter.drive->load();
                        const float envAmt   = scP.filter.envAmt->load();
                        const float keytrack = scP.filter.keytrack->load();
                        const float velSens  = scP.filter.velSens->load();

                        if (drive > 0.001f)
                        {
                            const float dGain = 1.0f + drive * 4.5f;
                            fltInL = std::tanh (fltInL * dGain) / std::tanh (dGain);
                            fltInR = std::tanh (fltInR * dGain) / std::tanh (dGain);
                        }

                        const float keyOctaves = ((static_cast<float> (currentMidiNote) - 60.0f) / 12.0f) * keytrack;
                        const float envOctaves = modEnvArr[s] * envAmt * 5.0f;
                        const float velOctaves = (noteVelocity - 0.5f) * velSens * 3.0f;
                        const float modOctaves = fegCutMod + seqCutMod + acc.cutoffOct;

                        const float modCut = baseCut * std::pow (2.0f, keyOctaves + envOctaves + velOctaves + modOctaves);

                        state.svfStage1.process (fltInL, fltInR, modCut, res, sr, fltType);
                        if (fltType == 1)
                            state.svfStage2.process (fltInL, fltInR, modCut, res * 0.45f, sr, 0);
                    }

                    const float sceneWeight = (s == 0) ? weight1 : weight2;
                    const float voiceGain   = ampEnvArr[s] * noteVelocity * sceneWeight * 0.34f;

                    mixL += (fltInL + bypassL) * voiceGain;
                    mixR += (fltInR + bypassR) * voiceGain;
                }

                outL[startSample + sampleIdx] += std::tanh (mixL);
                if (outR != nullptr)
                    outR[startSample + sampleIdx] += std::tanh (mixR);

                const bool seqKeepingAlive = noteIsHeld && (seqMode[0] == 1 || seqMode[1] == 1);
                if (! seqKeepingAlive && ! sceneStates[0].ampEnv.isActive() && ! sceneStates[1].ampEnv.isActive())
                {
                    cache.activeSeqStep[0].store (-1);
                    cache.activeSeqStep[1].store (-1);
                    cache.liveLfoOut[0].store (0.0f);
                    cache.liveLfoOut[1].store (0.0f);
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

            float freeEgPhase    = 0.0f;
            float freeEgSmoothed = 0.0f;
            bool  freeEgReleased = false;
            int   freeEgDir      = 1;

            float seqPhase       = 0.0f;
            int   seqCurrentStep = 0;
            bool  seqGateOpen    = true;
            float seqSmoothedVal = 0.0f;

            float lfoPhase    = 0.0f;
            float lfoFadeGain = 1.0f;
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
        bool noteIsHeld      = false;
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

    ensureDefaultPresetExists();
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

        const auto fegp = scenePrefix + "feg_";
        sc.freeEg.target = apvts.getRawParameterValue (fegp + "target");
        sc.freeEg.mode   = apvts.getRawParameterValue (fegp + "mode");
        sc.freeEg.sync   = apvts.getRawParameterValue (fegp + "sync");
        sc.freeEg.time   = apvts.getRawParameterValue (fegp + "time");
        sc.freeEg.amount = apvts.getRawParameterValue (fegp + "amount");
        sc.freeEg.smooth = apvts.getRawParameterValue (fegp + "smooth");

        for (size_t pt = 0; pt < 6; ++pt)
        {
            const auto ptStr = juce::String (static_cast<int> (pt + 1));
            sc.freeEg.ptTime[pt]  = apvts.getRawParameterValue (fegp + "pt" + ptStr + "_t");
            sc.freeEg.ptLevel[pt] = apvts.getRawParameterValue (fegp + "pt" + ptStr + "_v");
        }

        const auto seqp = scenePrefix + "seq_";
        sc.stepSeq.mode   = apvts.getRawParameterValue (seqp + "mode");
        sc.stepSeq.length = apvts.getRawParameterValue (seqp + "length");
        sc.stepSeq.rate   = apvts.getRawParameterValue (seqp + "rate");
        sc.stepSeq.gate   = apvts.getRawParameterValue (seqp + "gate");
        sc.stepSeq.swing  = apvts.getRawParameterValue (seqp + "swing");
        sc.stepSeq.amount = apvts.getRawParameterValue (seqp + "amount");

        for (size_t st = 0; st < 16; ++st)
        {
            const auto stStr = juce::String (static_cast<int> (st + 1));
            sc.stepSeq.stepVal[st]  = apvts.getRawParameterValue (seqp + "st" + stStr + "_v");
            sc.stepSeq.stepGate[st] = apvts.getRawParameterValue (seqp + "st" + stStr + "_g");
        }

        const auto lfop = scenePrefix + "lfo1_";
        sc.lfo1.sync  = apvts.getRawParameterValue (lfop + "sync");
        sc.lfo1.rate  = apvts.getRawParameterValue (lfop + "rate");
        sc.lfo1.fade  = apvts.getRawParameterValue (lfop + "fade");
        sc.lfo1.depth = apvts.getRawParameterValue (lfop + "depth");

        for (size_t m = 0; m < 4; ++m)
        {
            const auto mp = scenePrefix + "mod" + juce::String (static_cast<int> (m + 1)) + "_";
            sc.modSlots[m].src    = apvts.getRawParameterValue (mp + "src");
            sc.modSlots[m].dst    = apvts.getRawParameterValue (mp + "dst");
            sc.modSlots[m].amount = apvts.getRawParameterValue (mp + "amt");
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout NeomorphX1AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    const juce::StringArray knobNames = {
        "Master Volume", "Master Pan",
        "Assign 1", "Assign 2",
        "Assign 3", "Assign 4",
        "Assign 5", "Assign 6",
        "Assign 7", "Assign 8"
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

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "scene_1", 1 }, "Scene 1",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "scene_morph", 1 }, "Scene Morph",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "scene_2", 1 }, "Scene 2",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "pitch_wheel", 1 }, "Pitch Wheel",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mod_wheel", 1 }, "Mod Wheel",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    const juce::StringArray waveforms = {
        "Basic Analog", "PWM / Pulse", "Formant", "Digital Fold", "SuperSaw", "Vocal Morph"
    };

    for (int scene = 1; scene <= 2; ++scene)
    {
        const auto scenePrefix = "s" + juce::String (scene) + "_";
        const auto sceneLabel  = "S" + juce::String (scene) + " ";

        for (int osc = 1; osc <= 2; ++osc)
        {
            const auto prefix = scenePrefix + "osc" + juce::String (osc) + "_";
            const auto label  = sceneLabel + "Osc " + juce::String (osc) + " ";

            int defaultWave = (scene == 1) ? ((osc == 1) ? 0 : 1)
                                           : ((osc == 1) ? 2 : 5);

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

        const auto fegPrefix = scenePrefix + "feg_";
        const auto fegLabel  = sceneLabel + "Free EG ";

        const juce::StringArray fegTargets = {
            "Off", "Filter Cutoff", "Osc 1+2 Position", "Osc 1+2 Morph", "Osc Pitch", "Scene Morph"
        };
        const juce::StringArray fegModes = {
            "One-Shot", "Sustain Hold", "Loop (Fwd)", "Loop (Ping-Pong)"
        };
        const juce::StringArray fegSyncs = {
            "Free (Sec)", "1/1 Bar", "1/2 Note", "1/4 Note", "1/8 Note", "1/16 Note"
        };

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { fegPrefix + "target", 1 }, fegLabel + "Target",
            fegTargets, 0));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { fegPrefix + "mode", 1 }, fegLabel + "Mode",
            fegModes, 1));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { fegPrefix + "sync", 1 }, fegLabel + "Sync",
            fegSyncs, 0));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { fegPrefix + "time", 1 }, fegLabel + "Time",
            juce::NormalisableRange<float> (0.05f, 16.0f, 0.01f, 0.35f),
            (scene == 1) ? 2.40f : 3.20f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { fegPrefix + "amount", 1 }, fegLabel + "Amount",
            juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f),
            (scene == 1) ? 0.45f : 0.55f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { fegPrefix + "smooth", 1 }, fegLabel + "Smooth",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.15f));

        constexpr std::array<float, 6> defTimes  = { 0.00f, 0.18f, 0.42f, 0.65f, 0.80f, 1.00f };
        constexpr std::array<float, 6> defLevels = { 0.00f, 0.85f, 0.35f, 0.75f, 0.55f, 0.00f };

        for (int pt = 0; pt < 6; ++pt)
        {
            const auto ptStr = juce::String (pt + 1);
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { fegPrefix + "pt" + ptStr + "_t", 1 },
                fegLabel + "Pt " + ptStr + " Time",
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                defTimes[static_cast<size_t> (pt)]));

            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { fegPrefix + "pt" + ptStr + "_v", 1 },
                fegLabel + "Pt " + ptStr + " Level",
                juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f),
                defLevels[static_cast<size_t> (pt)]));
        }

        const auto seqPrefix = scenePrefix + "seq_";
        const auto seqLabel  = sceneLabel + "Step Seq ";

        const juce::StringArray seqModes = {
            "Off", "Pitch (Seq)", "Filter Cutoff", "Osc Position", "Scene Morph"
        };
        const juce::StringArray seqLengths = {
            "8 Steps", "12 Steps", "16 Steps"
        };
        const juce::StringArray seqRates = {
            "1/4 Note", "1/8 Note", "1/8 Dotted", "1/16 Note", "1/16 Triplet", "1/32 Note"
        };

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { seqPrefix + "mode", 1 }, seqLabel + "Mode",
            seqModes, 0));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { seqPrefix + "length", 1 }, seqLabel + "Length",
            seqLengths, 2));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { seqPrefix + "rate", 1 }, seqLabel + "Rate",
            seqRates, 3));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { seqPrefix + "gate", 1 }, seqLabel + "Gate",
            juce::NormalisableRange<float> (0.10f, 1.00f, 0.01f), 0.65f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { seqPrefix + "swing", 1 }, seqLabel + "Swing",
            juce::NormalisableRange<float> (0.00f, 0.75f, 0.01f), 0.00f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { seqPrefix + "amount", 1 }, seqLabel + "Amount / Glide",
            juce::NormalisableRange<float> (0.00f, 1.00f, 0.01f), 0.25f));

        constexpr std::array<float, 16> defSteps = {
            0.0f, 0.0f, 1.0f, 0.0f,  0.5833f, 0.0f, 1.0f, 0.4167f,
            0.0f, 0.0f, 1.0f, 0.0f, -0.1667f, 0.0f, 0.5833f, 0.8333f
        };

        for (int st = 0; st < 16; ++st)
        {
            const auto stStr = juce::String (st + 1);
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { seqPrefix + "st" + stStr + "_v", 1 },
                seqLabel + "Step " + stStr,
                juce::NormalisableRange<float> (-1.0f, 1.0f, 1.0f / 12.0f),
                defSteps[static_cast<size_t> (st)]));

            params.push_back (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { seqPrefix + "st" + stStr + "_g", 1 },
                seqLabel + "Gate " + stStr,
                true));
        }

        const auto lfoPrefix = scenePrefix + "lfo1_";
        const auto lfoLabel  = sceneLabel + "LFO 1 ";

        const juce::StringArray lfoSyncItems = {
            "Free (Hz)", "1/1 Bar", "1/2 Note", "1/4 Note", "1/8 Note", "1/16 Note"
        };

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { lfoPrefix + "sync", 1 }, lfoLabel + "Sync",
            lfoSyncItems, 0));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { lfoPrefix + "rate", 1 }, lfoLabel + "Rate",
            juce::NormalisableRange<float> (0.05f, 20.0f, 0.01f, 0.35f),
            (scene == 1) ? 3.50f : 0.45f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { lfoPrefix + "fade", 1 }, lfoLabel + "Fade In",
            juce::NormalisableRange<float> (0.0f, 4.0f, 0.01f, 0.45f), 0.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { lfoPrefix + "depth", 1 }, lfoLabel + "Depth",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

        const juce::StringArray modSources = {
            "Off",
            "LFO 1 (Sine)", "LFO 1 (Tri)", "LFO 1 (Saw)",
            "Mod Wheel", "Velocity", "Keytrack",
            "Amp EG", "Mod EG", "Free EG", "Step Seq",
            "Assign 1", "Assign 2", "Assign 3", "Assign 4",
            "Assign 5", "Assign 6", "Assign 7", "Assign 8"
        };

        const juce::StringArray modDestinations = {
            "Off",
            "Filter Cutoff", "Filter Resonance",
            "Osc 1 Pitch", "Osc 2 Pitch", "Osc 1+2 Pitch",
            "Osc 1 Position", "Osc 2 Position", "Osc 1+2 Morph",
            "Osc 1 Level", "Osc 2 Level",
            "Scene Morph", "LFO 1 Rate", "LFO 1 Depth"
        };

        for (int m = 0; m < 4; ++m)
        {
            const auto mp = scenePrefix + "mod" + juce::String (m + 1) + "_";
            const auto ml = sceneLabel + "Mod " + juce::String (m + 1) + " ";

            const int defSrc = (m == 0) ? ((scene == 1) ? 4 : 11) : 0;
            const int defDst = (m == 0) ? ((scene == 1) ? 5 : 1)  : 0;
            const float defAmt = (m == 0 && scene == 2) ? 0.45f : 0.0f;

            params.push_back (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { mp + "src", 1 }, ml + "Source",
                modSources, defSrc));

            params.push_back (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { mp + "dst", 1 }, ml + "Destination",
                modDestinations, defDst));

            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { mp + "amt", 1 }, ml + "Amount",
                juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), defAmt));
        }
    }

    return { params.begin(), params.end() };
}

// ==============================================================================
// Preset-Verwaltung & Engine-Panic
// ==============================================================================
juce::File NeomorphX1AudioProcessor::getPresetDirectory() const
{
    auto baseDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    auto dir = baseDir.getChildFile ("Application Support").getChildFile ("NeomorphX1").getChildFile ("Presets");
   #else
    auto dir = baseDir.getChildFile ("NeomorphX1").getChildFile ("Presets");
   #endif

    if (! dir.exists())
        dir.createDirectory();

    return dir;
}

void NeomorphX1AudioProcessor::ensureDefaultPresetExists()
{
    const auto initFile = getPresetDirectory().getChildFile ("Init.xml");
    if (! initFile.existsAsFile())
    {
        auto state = apvts.copyState();
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        if (xml != nullptr)
        {
            xml->setAttribute ("presetName", "Init");
            xml->writeTo (initFile);
        }
    }
}

juce::StringArray NeomorphX1AudioProcessor::getAvailablePresets() const
{
    juce::StringArray names;
    names.add ("Init");

    const auto files = getPresetDirectory().findChildFiles (juce::File::findFiles, false, "*.xml");
    juce::StringArray sorted;
    for (const auto& f : files)
    {
        const auto name = f.getFileNameWithoutExtension();
        if (! name.equalsIgnoreCase ("Init"))
            sorted.add (name);
    }

    sorted.sort (true);
    names.addArray (sorted);
    return names;
}

bool NeomorphX1AudioProcessor::savePreset (const juce::String& presetName)
{
    const auto cleanName = juce::File::createLegalFileName (presetName.trim());
    if (cleanName.isEmpty())
        return false;

    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml == nullptr)
        return false;

    xml->setAttribute ("presetName", cleanName);

    const auto targetFile = getPresetDirectory().getChildFile (cleanName + ".xml");
    if (xml->writeTo (targetFile))
    {
        currentPresetName    = cleanName;
        currentPresetIsSaved = ! cleanName.equalsIgnoreCase ("Init");
        return true;
    }

    return false;
}

bool NeomorphX1AudioProcessor::loadPresetByName (const juce::String& presetName)
{
    if (presetName.equalsIgnoreCase ("Init"))
    {
        const auto initFile = getPresetDirectory().getChildFile ("Init.xml");
        if (initFile.existsAsFile())
        {
            std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (initFile));
            if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
            {
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
                currentPresetName    = "Init";
                currentPresetIsSaved = false;
                panicReset();
                return true;
            }
        }

        loadInitPatch();
        return true;
    }

    const auto file = getPresetDirectory().getChildFile (presetName + ".xml");
    return loadPresetFromFile (file, false);
}

bool NeomorphX1AudioProcessor::loadPresetFromFile (const juce::File& file, bool importToLibrary)
{
    if (! file.existsAsFile())
        return false;

    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    const auto loadedName = xml->getStringAttribute ("presetName", file.getFileNameWithoutExtension());
    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    currentPresetName    = loadedName;
    currentPresetIsSaved = ! loadedName.equalsIgnoreCase ("Init");

    if (importToLibrary && currentPresetIsSaved)
    {
        const auto destFile = getPresetDirectory().getChildFile (juce::File::createLegalFileName (loadedName) + ".xml");
        xml->setAttribute ("presetName", loadedName);
        xml->writeTo (destFile);
    }

    panicReset();
    return true;
}

void NeomorphX1AudioProcessor::loadInitPatch()
{
    for (auto* param : getParameters())
        param->setValueNotifyingHost (param->getDefaultValue());

    currentPresetName    = "Init";
    currentPresetIsSaved = false;
    panicReset();
}

void NeomorphX1AudioProcessor::panicReset()
{
    panicRequested.store (true);
    paramCache.activeSeqStep[0].store (-1);
    paramCache.activeSeqStep[1].store (-1);
    paramCache.liveLfoOut[0].store (0.0f);
    paramCache.liveLfoOut[1].store (0.0f);
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
            if (id.startsWith (prefix)
                && ! id.contains ("_feg_pt")
                && ! id.contains ("_seq_st")
                && ! id.contains ("_mod"))
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

const juce::String NeomorphX1AudioProcessor::getName() const { return JucePlugin_Name; }
bool NeomorphX1AudioProcessor::acceptsMidi() const { return true; }
bool NeomorphX1AudioProcessor::producesMidi() const { return false; }
bool NeomorphX1AudioProcessor::isMidiEffect() const { return false; }
double NeomorphX1AudioProcessor::getTailLengthSeconds() const { return 0.0; }
int NeomorphX1AudioProcessor::getNumPrograms() { return 1; }
int NeomorphX1AudioProcessor::getCurrentProgram() { return 0; }
void NeomorphX1AudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String NeomorphX1AudioProcessor::getProgramName (int index) { juce::ignoreUnused (index); return {}; }
void NeomorphX1AudioProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused (index, newName); }

void NeomorphX1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    synth.setCurrentPlaybackSampleRate (sampleRate);
}

void NeomorphX1AudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NeomorphX1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}
#endif

void NeomorphX1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (panicRequested.exchange (false))
    {
        synth.allNotesOff (0, false);
        midiMessages.clear();
        return;
    }

    if (auto* ph = getPlayHead())
    {
        if (auto posInfo = ph->getPosition())
        {
            if (auto bpm = posInfo->getBpm())
                paramCache.currentBpm.store (static_cast<float> (*bpm));
        }
    }

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

    const int numSamples = buffer.getNumSamples();
    synth.renderNextBlock (buffer, midiMessages, 0, numSamples);

    const float masterVol = paramCache.matrixKnobs[0]->load();
    const float masterPan = paramCache.matrixKnobs[1]->load();

    const float panAngle = masterPan * juce::MathConstants<float>::halfPi;
    const float gainL    = masterVol * std::cos (panAngle) * 1.41421356f;
    const float gainR    = masterVol * std::sin (panAngle) * 1.41421356f;

    if (buffer.getNumChannels() >= 2)
    {
        buffer.applyGain (0, 0, numSamples, gainL);
        buffer.applyGain (1, 0, numSamples, gainR);
    }
    else if (buffer.getNumChannels() == 1)
    {
        buffer.applyGain (0, 0, numSamples, masterVol);
    }
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
    if (xml != nullptr)
    {
        xml->setAttribute ("presetName", currentPresetName);
        xml->setAttribute ("presetIsSaved", currentPresetIsSaved);
        copyXmlToBinary (*xml, destData);
    }
}

void NeomorphX1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
    {
        currentPresetName    = xmlState->getStringAttribute ("presetName", "Init");
        currentPresetIsSaved = xmlState->getBoolAttribute ("presetIsSaved", ! currentPresetName.equalsIgnoreCase ("Init"));
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NeomorphX1AudioProcessor();
}
