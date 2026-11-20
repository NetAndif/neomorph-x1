<h1><img width="32" height="32" alt="icon" src="https://github.com/user-attachments/assets/65b877e2-c2bf-4fba-bab9-e2aa4ad00432" /> Neomorph X1</h1>

Open Source Virtual Analog Synthesizer VST3/AU Plugin inspired by the Yamaha AN1x with advanced Editor &amp; Cyberpunk UI.

<img width="1688" height="979" alt="Screenshot_NeomorphX1" src="https://github.com/user-attachments/assets/1deaa2ef-3bc7-43c1-9cbe-4d6234610626" />

## Features

### Dual-Scene Morphing Architecture
* **16-Voice Polyphonic Synthesizer Engine** built with C++ and the JUCE framework (VST3, AU, and Standalone).
* **Dual-Scene Design (Scene 1 & Scene 2)** with independent synthesis engines, individual scene volume levels, and real-time crossfade morphing.
* **Real-Time Morph Modulation**: Dynamically morph between Scene 1 and Scene 2 via the Free EG, Step Sequencer, or the Modulation Matrix.
* **Scene Utility Tools**: Copy, swap, reset, or randomize parameters between Scene 1 and Scene 2.

### Oscillators & Wavetable Synthesis
* **4 Oscillators Total (2 per Scene)** with octave (`-2` to `+2`), semitone (`-12` to `+12`), fine detune (`±50 cents`), start phase, stereo panning, and level controls.
* **10 Wavetable Modes** with continuous wave position scanning and waveshaping morph control:
  * *Basic Analog* (continuous Sine → Triangle → Saw → Square morph)
  * *PWM / Pulse* (variable pulse width modulation)
  * *Formant* (windowed harmonic formant synthesis)
  * *Digital Fold* (wavefolder distortion)
  * *SuperSaw* (detuned multi-saw stack)
  * *Vocal Morph* (harmonic vocal spectrum)
  * *FM Bell / Metallic* (2-operator phase modulation)
  * *Sub + Harmonics* (sub-oscillator with blendable odd harmonics)
  * *Bitcrush / LoFi* (variable bit/step quantization)
  * *Sync Lead* (hard-sync oscillator sweep)

### Multi-Mode Filtering
* **6 Zero-Delay Feedback State-Variable Filter (SVF) Modes** per scene: *LP 12dB*, *LP 24dB Ladder*, *BP 12dB*, *HP 12dB*, *Notch*, and *Comb / Formant*.
* **Flexible Filter Routing**: Process *Osc 1 + 2*, *Osc 1 Only* (with Osc 2 bypassing the filter), or full *Bypass*.
* **Pre-Filter Saturation Drive**, cutoff resonance, keyboard tracking, velocity sensitivity, and bipolar filter envelope modulation.

### Deep Modulation System
* **Dual ADSR Envelopes** per scene (dedicated Amp Envelope and Filter/Mod Envelope).
* **Interactive 6-Point Free EG** per scene with *One-Shot*, *Sustain Hold*, *Loop (Forward)*, and *Loop (Ping-Pong)* modes, free or host-synced timing (`1/1` to `1/16`), smoothing, and direct routing to Cutoff, Osc Position, Osc Morph, Pitch, or Scene Morph.
* **16-Step Sequencer** per scene (8, 12, or 16 steps) with host tempo sync (`1/4` to `1/32` including dotted and triplet divisions), gate length, swing, glide/amount, and modes for *Pitch*, *Filter Cutoff*, *Osc Position*, or *Scene Morph*.
* **Per-Scene LFO 1** with host sync, fade-in envelope, depth control, and simultaneous *Sine*, *Triangle*, and *Saw* outputs.
* **4-Slot Modulation Matrix** per scene routing LFOs, Envelopes, Free EG, Step Sequencer, Mod Wheel, Velocity, Keytrack, and 8 Assignable Macro Knobs to 16 destinations (including individual/combined oscillator panning, pitch, wavetable position, filter parameters, LFO rate/depth, and Scene Morph).

### Performance, MIDI & Workflow
* **8 Assignable Macro Knobs** with instant MIDI Learn.
* **Hardware Encoder Support**: Supports *Absolute* CC mode with optional **Soft-Takeover (Pickup)** as well as *Relative (2's Complement)* and *Relative (Binary Offset)* rotary encoder modes.
* **Persistent MIDI Maps**: Save and load custom MIDI controller mappings as standalone XML files.
* **Hierarchical Preset Library**: Organized into a read-only **Factory** bank and customizable **User** category subfolders with XML metadata (*Author*, *Notes*, *Category*), factory preset restoration, and quick prev/next patch stepping.
* **Live Telemetry & LCD Display**: Real-time stereo oscilloscope, peak meters, active voice counter, MIDI activity monitor, and a built-in **Panic / Engine Reset** button.

