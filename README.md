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

---

## Building from Source

Neomorph X1 uses **CMake** (3.22+) and automatically fetches **JUCE 8.0.4** via `FetchContent` during the initial configuration step.

### Prerequisites (All Platforms)
* **Git**
* **CMake** `3.22` or newer
* A C++20-compatible compiler (Apple Clang, GCC/Clang, or MSVC 2022)

---

### macOS (Apple Silicon & Intel)

#### 1. Install Build Tools
Install Xcode Command Line Tools and CMake (via [Homebrew](https://brew.sh/)):
```bash
xcode-select --install
brew install cmake
```

#### 2. Configure & Build (Universal Binary)
```bash
git clone https://github.com/<your-username>/neomorph-x1.git
cd neomorph-x1

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release -j
```

#### 3. Install Plugins & Standalone App
The compiled artifacts are located in `build/NeomorphX1_artefacts/Release/`:
```bash
cp -R "build/NeomorphX1_artefacts/Release/VST3/Neomorph X1.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "build/NeomorphX1_artefacts/Release/AU/Neomorph X1.component" ~/Library/Audio/Plug-Ins/Components/
cp -R "build/NeomorphX1_artefacts/Release/Standalone/Neomorph X1.app" /Applications/
```

#### 4. macOS Gatekeeper & Unsigned App Notice
Pre-built GitHub releases (or bundles copied from another Mac) are **ad-hoc signed** and not notarized with an Apple Developer ID certificate. When launching the Standalone app or scanning the VST3/AU plugins in your DAW for the first time, macOS Gatekeeper may show a warning (*"Neomorph X1 cannot be opened because the developer cannot be verified"* or *"is damaged and can't be opened"*).

To allow macOS to run the unsigned binaries, remove the quarantine attribute in Terminal:
```bash
xattr -cr "/Applications/Neomorph X1.app"
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Neomorph X1.vst3"
xattr -cr ~/Library/Audio/Plug-Ins/Components/"Neomorph X1.component"
```
Alternatively, open **System Settings → Privacy & Security**, scroll down to the **Security** section, and click **Open Anyway** / **Allow Anyway** next to the blocked Neomorph X1 entry.

*(Note: After installing a new Audio Unit `.component` on macOS, you may need to log out and back in—or run `killall -9 AudioComponentRegistrar`—for Logic Pro / GarageBand to recognize the new AU plugin.)*

---

### Ubuntu / Debian Linux

#### 1. Install Dependencies
Install the required compiler toolchain and JUCE Linux development libraries (X11, ALSA, FreeType, WebKit/GTK, cURL):
```bash
sudo apt update
sudo apt install -y build-essential cmake git pkg-config \
    libasound2-dev libjack-jackd2-dev \
    libfreetype6-dev libfontconfig1-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
    libxinerama-dev libxrandr-dev libxrender-dev \
    libgl1-mesa-dev libglu1-mesa-dev \
    libgtk-3-dev libwebkit2gtk-4.1-dev libcurl4-openssl-dev
```
*(Note: On older Ubuntu releases such as 22.04 LTS, replace `libwebkit2gtk-4.1-dev` with `libwebkit2gtk-4.0-dev`.)*

#### 2. Configure & Build
```bash
git clone https://github.com/<your-username>/neomorph-x1.git
cd neomorph-x1

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j$(nproc)
```

#### 3. Install VST3 & Standalone Binary
```bash
mkdir -p ~/.vst3 ~/.local/bin
cp -r "build/NeomorphX1_artefacts/Release/VST3/Neomorph X1.vst3" ~/.vst3/
cp "build/NeomorphX1_artefacts/Release/Standalone/Neomorph X1" ~/.local/bin/
```

---

### Windows (10 / 11)

#### 1. Install Build Tools
* Install **Visual Studio 2022** (Community Edition is sufficient) with the **"Desktop development with C++"** workload selected.
* Install **CMake** (or use the CMake version bundled with Visual Studio) and **Git for Windows**.

#### 2. Configure & Build (PowerShell or x64 Native Tools Command Prompt)
```powershell
git clone https://github.com/<your-username>/neomorph-x1.git
cd neomorph-x1

cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

#### 3. Install VST3 & Standalone Executable
Copy the build outputs from `build\NeomorphX1_artefacts\Release\`:
* **VST3 Plugin:** Copy `Neomorph X1.vst3` to `C:\Program Files\Common Files\VST3\`
* **Standalone App:** Copy `Neomorph X1.exe` to any folder of your choice (e.g., `C:\Program Files\Neomorph X1\`)

*(Note: If Windows Defender SmartScreen displays a warning on unsigned standalone builds, click **More info → Run anyway**.)*

