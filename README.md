# Brown Help

Brown Help is a VST3 podcast/voice-over processor by **D35P Audio**.

It dynamically nudges dialogue toward a controllable brown-noise-style spectral tilt, then optionally adds a gentle high-pass filter and high-band saturation. The goal is not a dramatic EQ effect; it is quick tonal settling for spoken-word audio.

## Current Status

- Format: VST3
- Framework: JUCE + CMake
- Tested target host: REAPER on macOS
- Supported layouts: mono and stereo
- Platforms intended: macOS, Windows, Linux

## Main Features

- Adaptive spectral correction toward a tilt curve
- Generic Tilt amount, plus Flip to mirror the curve brighter
- Mix control with visual tilt opacity
- Low/High frequency range for correction
- Strength, Max Correction, and Speed controls
- Optional high-pass filter with 12/24 dB/oct slope
- Optional gentle high-band saturation
- Tilt curve display with high-pass overlay
- 1x/2x/4x oversampling

## Signal Chain

```text
Input
  -> optional High-Pass
  -> adaptive Brown Help EQ
  -> optional High Saturation
  -> output guard
Output
```

## What Happens Where

### Low / High Range

`Low` and `High` choose the frequency range where the adaptive brown-curve correction is allowed to work. They do not remove audio.

Example: if `Low` is set to `100 Hz`, the plugin stops correcting below `100 Hz`, but sub-bass and rumble can still pass through.

### High-Pass

`High Pass` is an actual filter. It removes low-frequency audio before the adaptive correction stage.

It is placed before the adaptive EQ so rumble and plosives do not trick the detector into over-correcting the voice.

### Adaptive Correction

The adaptive EQ listens to the selected Low/High range, compares the incoming spectral shape against the Tilt curve, then cuts bands that sit too far above the target. It can also gently boost bands below the target, but boosts are intentionally much softer than cuts.

### Saturation

`High Saturation` is applied after the adaptive EQ. It only affects the high band, adding a small amount of soft harmonic density for voice presence.

### Output Guard

The final stage catches runaway peaks and prevents large output jumps.

Oversampling wraps the processing path when set to `2x` or `4x`. For normal dialogue work, start with `1x` for best REAPER render speed.

## Build

Configure Debug:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

Build Debug:

```sh
cmake --build build --config Debug
```

Configure Release:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
```

Build Release:

```sh
cmake --build build-release --config Release
```

Run tests:

```sh
ctest --test-dir build --output-on-failure
ctest --test-dir build-release --output-on-failure
```

Release plugin path:

```text
build-release/BrownHelp_artefacts/Release/VST3/Brown Help.vst3
```

Use the Release build in REAPER. The Debug build is much slower and should only be used while developing.

## Recommended Starting Settings

- Curve: `Brown`
- Tilt: around `25`
- Flip: off for darker brown-style tilt, on for brighter tilt
- Strength: `15%`
- Mix: `100%`
- Max Correction: `3 dB`
- Speed: `45%`
- Oversampling: `1x`
- High Pass: optional, around `70-90 Hz`
- Saturation: optional, low Drive and Mix

## DSP Notes

The core processor uses 24 log-spaced bands over the selected Low/High range. Each band tracks band-pass energy, compares the measured shape against the target tilt, and applies a smoothed peaking-EQ correction.

Boosts are intentionally much gentler than cuts. This keeps the plugin from chasing noise, boosting room tone, or exploding output level. A final output guard catches runaway peaks.

The Tilt knob is a generic musical control, not a raw technical slope knob. It runs from `0` to `100`, mapping to `0` to `-6 dB/oct` by default. Flip mirrors that to `0` to `+6 dB/oct`.

The curve selector currently changes the amount/range of the tilt rather than changing the underlying correction algorithm. `Gentle Brown` scales the available slope down; `Brown` and `Dark Brown` use the full slope range.

## UI Notes

The graph shows the target tilt curve, with opacity based on Mix, plus a high-pass curve overlay when HP is enabled.

## License Note

This project uses JUCE. Check JUCE licensing before distributing binaries commercially, especially if the project is not GPL-compatible.
