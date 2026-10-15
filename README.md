# Pastel Drum Sampler — 16-Pad Drum Sampler (VST3 + Standalone)

> **Disclaimer:** this project is vibe-coded for personal use. It is provided
> as-is, without warranty of any kind. The author is not responsible for
> anything — Use at your own risk.

The sample-based sibling of Pastel Drum Synth: the same engine around the
voices (per-pad FX chains, global chain, shared reverb/delay returns,
randomize / mutate with locks, undo, kit browser), with the synth voices
replaced by user-loadable samples.

- **16 generic pads** in an MPC-style 4×4 grid (pad 1 = C1 / MIDI 36,
  chromatic up to D#2). No pad assumes a drum type.
- **4 layers per pad**: **STACK** (all play) or **ROUND ROBIN** (cycle).
  Per layer: tune, fine, start, end, level, pan, reverse, normalize.
- **Per pad**: AHDR amp envelope (HOLD = FULL plays the whole sample),
  pitch envelope, per-hit multimode filter with envelope, drive, comp, tone,
  punch, velocity → amp / start / filter, random pitch, reverb/delay sends,
  choke groups (8), voice mode (Poly / Mono / Gated), mute / solo.
- **4 macros per pad**, generic and identical on every pad (PITCH, LENGTH,
  COLOR, SPACE by default). LENGTH works relative to each sample: turning it
  down fades the sound out earlier (to 5 % at the bottom), and turning it up
  lifts the decaying tail. It drives the pad's LENGTH knob (AMP page). Bipolar: the centre is the sound as programmed.
  Right-click any pad knob to assign it to a macro at a chosen depth.
  Click a macro's target strip to edit depths, add targets or rename it.
- **Multi-out**: Main stereo plus 16 stereo aux buses (`Out 2` … `Out 17`).
  Pick each pad's bus in the OUT selector. Aux buses must be enabled in the
  host (a disabled one falls back to Main). Aux outs carry the pad after its
  FX chain, level and pan, with master gain applied. The global chain, the
  shared returns and the limiter stay on Main.
- **Effects**: the drum synth's 7 effect types, now true stereo. Reverb and
  delay memory is allocated only for slots that use them.
- **Kits**: XML files that store the sample paths. Saving with **COLLECT
  SAMPLES** copies every sample into a `<Kit Name> Samples` folder next to
  the kit and stores relative paths, so the folder can be moved or shared.
  Without it, the kit points at the files where they are. A missing file is
  flagged in red (pad, layer strip and waveform), and its path is kept.
- **Factory kits**: twenty kits, none named or tagged for a genre.
  - Heavy Water, Saw Teeth, Glass Garden, Static Forge, Dust Archive,
    Velvet Basement, Groove Mechanic, Concrete Pulse, Rust Cathedral and
    Brass Tacks cover heavy bass music and fast breaks through house and
    techno.
  - Night Bus, Gold Leaf, Thin Line, Black Steel, Red Clay and Mirror Ball
    are kick-led grooves (2-step shuffle, piano house, minimal, peak time,
    percussion-led, disco-flavoured). Their kicks carry the weight.
  - The house and techno kits (these six, plus Velvet Basement, Groove
    Mechanic, Concrete Pulse and Rust Cathedral) use a 909-style snare: two
    close, near-sine partials (around 236 and 314 Hz) under bright noise that
    holds before it fades. It was fitted to a 909 reference by matching
    statistics only, so no reference audio is used. It sits about 9–10 dB
    under the kick. The bass-music kits keep snares fitted to
    reference snares.
  - Cold Circuit, Razor Wire, Glass Engine and Hard Light are drum & bass
    kits with short punchy kicks (about 0.13–0.19 s). Their snares are Heavy
    Water's snare transposed up 4–7 semitones: only the body, sweep, snap and
    clap tone move; the envelopes stay the same. They sit about 2 dB under
    the kick.
  They are 567 samples synthesised from scratch by `tools/kitgen`: no
  recordings, no third-party samples, and nothing rendered from Pastel Drum
  Synth. They ship as 24-bit FLAC in `Factory/Samples/`, inside the bundle.
  Every kit uses the same pad layout:

  |      |              |              |             |               |
  |------|--------------|--------------|-------------|---------------|
  | row 4 | 13 Bass/Stab | 14 Vox/Stab | 15 FX       | 16 FX         |
  | row 3 | 9 Perc       | 10 Perc     | 11 Perc     | 12 Crash/Ride |
  | row 2 | 5 Kick 2     | 6 Snare 2   | 7 Open Hat  | 8 Ride/Shaker |
  | row 1 | 1 Kick       | 2 Snare     | 3 Clap      | 4 Closed Hat  |

  The hats share choke group 1. Repeated hits carry round-robin variations,
  and a few pads stack separate layers (for example a snare = body + clap +
  gated wash).

- **Sample browser** (BROWSER tab, ported from Pastel Soundfont Player):
  folder shortcuts saved in your user settings, with the factory sample
  folder always listed first. Also search, UP and SCAN, and folders listed
  before files. Selecting a file previews it (PREVIEW toggle).
  Double-click or LOAD > loads it into the selected pad and layer; drag a
  row onto any pad (replaces it), layer slot or the waveform.
- **UNDO / REDO** in the header cover kit loads, sample loads, randomize,
  mutate, pad copy/paste/clear and region edits (30 steps).
- **RANDOMIZE / MUTATE change timbre only**: tune, pitch envelope, filter,
  drive, tone, sends, and the colour of the effects. They never touch level,
  pan, envelopes, compression, punch, velocity response, humanize, macros,
  choke or voice mode, and they never add dynamics effects.

Drag audio files (wav, aiff, flac, ogg, mp3) from your file manager onto a pad (up to 4 files =
4 layers), a layer slot, or the waveform. Clicking a pad selects and plays it.
Right-click a pad to load, copy, paste or clear it. Drag the S / E markers to
set the sample region.

## License

GPL version 3 — see [LICENSE](LICENSE). This also satisfies the JUCE
framework's licensing terms for this project.

## Building

Requirements: CMake 3.22+, a C++17 compiler, Ninja (or Make), Git (JUCE 8.0.6
is fetched automatically), and JUCE's Linux dependencies (ALSA, X11, freetype,
etc.) — see the JUCE docs for the full list.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
# offline: reuse an existing JUCE checkout
#   -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
```

Outputs:

- VST3: `build/PastelDrumSampler_artefacts/Release/VST3/Pastel Drum Sampler.vst3`
  (+ factory kits under `Contents/Resources/Factory/`)
- Standalone: `build/PastelDrumSampler_artefacts/Release/Standalone/Pastel Drum Sampler`
- Dev self-tests (never shipped): `build/E2ETest` (must print
  `ALL E2E TESTS PASSED`), `build/HostCheck` (loads the built VST3 through
  JUCE's VST3 hosting)

Install the VST3 by copying `Pastel Drum Sampler.vst3` to your plugin folder:

| OS      | VST3 folder                          | Notes                                        |
|---------|--------------------------------------|----------------------------------------------|
| Linux   | `~/.vst3/`                           | Rescan plugins in your DAW afterwards        |
| macOS   | `~/Library/Audio/Plug-Ins/VST3/`     | Unsigned build: right-click → Open once      |
| Windows | `C:\Program Files\Common Files\VST3\`| Rescan plugins in your DAW afterwards        |

User kits are saved in `Documents/Pastel Drum Sampler Presets/`.

### OS-specific notes

> Only the Linux build has actually been compiled and tested here. The macOS
> and Windows steps below follow the standard JUCE/CMake flow and *should*
> work, but if you hit an OS-specific snag, please file an issue with the
> failing command and its full output.

#### Linux (verified)

1. Install dependencies (Debian/Ubuntu shown; Fedora/Arch: equivalent
   `-devel` packages):
   ```sh
   sudo apt install build-essential cmake ninja-build git pkg-config \
     libasound2-dev libx11-dev libxcomposite-dev libxcursor-dev \
     libxinerama-dev libxrandr-dev libfreetype6-dev libfontconfig1-dev \
     libcurl4-openssl-dev libwebkit2gtk-4.1-dev
   ```
2. Configure + build (JUCE is fetched automatically — needs network access):
   ```sh
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```
3. Run the end-to-end test suite (must print `ALL E2E TESTS PASSED`):
   ```sh
   ./build/E2ETest
   ```
4. Install: copy `build/PastelDrumSampler_artefacts/Release/VST3/Pastel Drum Sampler.vst3`
   to `~/.vst3/` (factory kits travel inside the bundle), then rescan plugins
   in your DAW.

#### macOS (not yet built here)

1. Install Xcode command-line tools, CMake, Ninja and Git:
   ```sh
   xcode-select --install
   brew install cmake ninja git
   ```
2. Same configure + build as Linux. For a universal (Intel + Apple Silicon)
   binary, add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` to the configure step.
3. Run `./build/E2ETest` — expect `ALL E2E TESTS PASSED`.
4. Install: copy `Pastel Drum Sampler.vst3` to
   `~/Library/Audio/Plug-Ins/VST3/`. These are unsigned personal builds: if
   macOS refuses to load them, ad-hoc sign with
   `codesign --force --deep -s - <path>` and/or strip the quarantine flag
   with `xattr -dr com.apple.quarantine <path>`. Codesigning/notarisation for
   distribution is out of scope for this project.

#### Windows (not yet built here)

1. Install Visual Studio 2022 with the **Desktop development with C++**
   workload (MSVC v143 or newer), plus CMake, Ninja and Git
   (`winget install Kitware.CMake Ninja-build.Ninja Git.Git` works).
2. Open an **x64 Native Tools Command Prompt** (so MSVC is on `PATH`), then
   the same configure + build commands as Linux. Alternatively use the
   Visual Studio generator instead of Ninja:
   ```bat
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   ```
3. Run `build\E2ETest.exe` — expect `ALL E2E TESTS PASSED`.
4. Install: copy `Pastel Drum Sampler.vst3` to
   `C:\Program Files\Common Files\VST3\`, then rescan plugins in your DAW.
   No ASIO SDK or extra setup required.

## Repository layout

- `Source/` — plugin DSP, UI, preset/kit backend
  (`PluginProcessor.*`, `PluginEditor.*`, `PresetBrowser.*`, `Presets.*`,
  `PresetFiles.*`, `FileLibrary.*`, `dsp/SampleEngine.*`, `dsp/Effects.*`)
- `Factory/` — the 20 factory kits (`*.xml`) plus their 567 synthesised
  24-bit FLAC samples in `Factory/Samples/` (data, never compiled in)
- `TestE2E.cpp` — end-to-end regression suite: drives the real processor like
  a host (MIDI mapping, multi-out routing, stack/round robin, choke, mono and
  gated modes, sample edits, macros, FX tails, DAW state, collected kits,
  undo, the editor); must print `ALL E2E TESTS PASSED`
- `HostCheck.cpp` — loads the built VST3 through JUCE's VST3 hosting; checks
  the 17 buses, the parameters, audio, state and the editor
- `UiShot.cpp` — renders the editor offscreen to PNGs
- `FactoryBuild.cpp` — builds kit XMLs from a `tools/kitgen` manifest via the
  real processor
- `tools/kitgen/` — the factory sound generator (Python: numpy, scipy,
  soundfile). It covers FM metal hats, modal rides, crashes and percussion,
  layered snares and claps, physical-model toms and congas, formant vocal
  chops, basses, stabs and FX. `kits.py` defines the kits; `build.py`
  renders them deterministically (fixed seeds), trims tails, peak-normalises
  to -0.5 dBFS and checks each file's spectral balance against its pad role.
  `inspect_audio.py` prints band energies and draws spectrogram sheets.
  `fit_snare.py ref.wav ...` fits the snare recipe to reference snares. It
  matches only broad statistics (per-band envelopes and octave balance), and
  no reference audio ends up in any sample.
  To regenerate:
  ```sh
  python3 -m venv tools/.venv && tools/.venv/bin/pip install numpy scipy soundfile matplotlib
  tools/.venv/bin/python tools/kitgen/build.py Factory      # samples + tools/kitgen/kits.json
  ./build/FactoryBuild tools/kitgen/kits.json Factory        # kit XMLs via the real processor
  ```
- `mockup/` — the SVG UI mockup the editor was built from
- `.github/workflows/build.yml` — CI: builds VST3 + standalone on
  Linux/macOS/Windows, runs the E2E suite, and publishes an auto-tagged
  release on every push to `main`
