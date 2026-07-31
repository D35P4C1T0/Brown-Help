# Brown Help

Brown Help is an automatic loudness and voice-band engineer for podcasts and voice-over work by **D35P Audio**.

![Brown Help plugin UI](docs/plugin-ui.png)

## What It Does

- Normalizes mono or stereo program loudness toward `-14 LUFS`
- Uses a linked `5 ms` look-ahead limiter with a `0 dBFS` sample ceiling
- Displays the processed output on a live log-frequency spectrum with `+4.5 dB/oct` display compensation
- Tracks voiced fundamental between `100-600 Hz` with YIN, confidence gating, and first-crest validation
- Provides a manual `100-600 Hz` fundamental override
- Detects the strongest sibilance peak between `3.5-10 kHz`
- Tracks the highest narrow-band RMS value at both learned frequencies
- Applies cut-only dynamic peak EQ so both learned peaks share one level and neither exceeds `-40 dB RMS`
- Provides optional low and high shelves with up to `12 dB` reduction and `12` or `18 dB/oct` slope choices
- Extends low-shelf frequency down to `20 Hz` and high-shelf frequency up to `20 kHz`
- Includes a Reset Learn action that clears loudness and voice-band history for a new speaker or recording

## Signal Flow

```text
Input
  -> K-weighted gated loudness measurement
  -> smooth stereo-linked gain toward -14 LUFS
  -> fundamental and sibilance analysis
  -> automatic cut-only F0 / S balancing
  -> optional low shelf
  -> optional high shelf
  -> 5 ms linked look-ahead limiter at 0 dBFS
  -> tilted output spectrum
Output
```

## Analysis And Balancing

### Loudness

The loudness meter uses K-weighting plus absolute and relative gating modeled on ITU-R BS.1770. Its rolling history drives smooth linked gain, so stereo image does not move.

A real-time insert cannot know the integrated loudness of an entire file before hearing it. Brown Help therefore converges while audio plays. For exact whole-file delivery loudness from the first sample, use an offline two-pass loudness tool after rendering.

### Fundamental

Voiced speech is DC/rumble filtered, low-pass isolated, and decimated before pitch analysis. YIN runs on `40 ms` frames at roughly `5 ms` hops, searches only `100-600 Hz`, rejects weak periodicity, interpolates between lag samples, and median-filters five results. FFT validation prefers the first credible spectral crest from `100 Hz` upward and corrects common octave-high errors.

Manual F0 bypasses automatic pitch selection while retaining RMS measurement and automatic F0/S balancing at the selected frequency.

### Sibilance

Frames with enough `3.5-10 kHz` energy relative to the midrange are treated as sibilance candidates. Brown Help retains the frequency belonging to the strongest detected high-band event and its highest narrow-band RMS value.

### Automatic Correction

Once both bands exist, Brown Help chooses the lower of:

- `-40 dB RMS`
- learned fundamental peak
- learned sibilance peak

Cut-only peak filters reduce each band to that common value. Missing bass or absent sibilance is never boosted, preventing room noise and hiss from being raised. Disable `Auto F0 / S Balance` to monitor without automatic EQ.

### Spectrum

The graph shows processed output, not the detector input. Its display adds `4.5 dB/oct` relative to `1 kHz`; this is analyzer compensation only and does not alter sound. Amber marks learned fundamental. Teal marks learned sibilance. Marker labels show active correction.

### Shelves

Low and high shelves are optional cut-only finishing controls. Each offers:

- Frequency
- `0-12 dB` reduction
- `12 dB/oct` or `18 dB/oct` transition

Low shelf covers `20-500 Hz`. High shelf covers `2-20 kHz` where sample rate permits.

## Status

- Formats: Audio Unit (`.component`) and VST3
- Framework: JUCE 8 + CMake
- Primary test host: REAPER on macOS
- Channel layouts: mono and stereo
- Platforms: macOS, Windows, Linux
- Current version: `0.3.0`

## Building

Requirements:

- CMake 3.22+
- C++20 compiler
- Git, so CMake can fetch JUCE

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release
ctest --test-dir build-release --output-on-failure
```

Release plugins:

```text
build-release/BrownHelp_artefacts/Release/AU/Brown Help.component
build-release/BrownHelp_artefacts/Release/VST3/Brown Help.vst3
```

On macOS, copy bundles to:

```text
~/Library/Audio/Plug-Ins/Components/
~/Library/Audio/Plug-Ins/VST3/
```

Then rescan plugins in REAPER.

## Development

- Main DSP: `Source/VoiceEngineer.*`
- Processor and state: `Source/BrownHelpProcessor.*`
- Spectrum and editor: `Source/BrownHelpUiComponents.*`, `Source/BrownHelpEditor.*`
- Parameter IDs/defaults: `Source/Parameters.h`
- Tests: `Tests/`

## License

Brown Help is released under GNU General Public License v3.0 only. See [LICENSE](LICENSE).

This project uses JUCE. Commercial binary distribution may require an appropriate JUCE license.
