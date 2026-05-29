# Brown Help VST3 Plan

## Goal

Build a mono-first VST3 plugin for podcast audio that dynamically nudges input tone toward a tilted brown-noise reference curve. Default target is `-4.5 dB/octave`, with adjustable range, strength, and wet/dry mix.

Working name: **Brown Help**. Vendor: **D35P Audio**.

## Product Names

1. Brown Help
2. Brownline
3. WarmCurve
4. Podcast Brown
5. Tiltkeeper
6. VoiceSlope
7. ToneSettle
8. Brown Anchor
9. Curve Friend
10. D35P Balancer

## References And Intent

Voxengo TEOTE is a dynamic spectral balancing EQ: it analyzes energy across frequency bands and applies time-varying EQ to move the source toward a selected spectral profile. Brown Help v1 follows that broad idea, but with a smaller podcast-focused scope:

- One target family: brown-like downward spectral tilt.
- Default slope: `-4.5 dB/octave`.
- Dynamic corrective EQ, not static EQ matching.
- Conservative correction, smoothed heavily to avoid pumping.
- Mono-first behavior for spoken-word content.

## Technical Defaults

- Framework: JUCE with CMake.
- Plugin format: VST3.
- Platforms: macOS, Windows, Linux.
- Build strategy: CMake `FetchContent` downloads JUCE.
- Channels: mono preferred, stereo accepted by summing detection and applying same correction to each channel. This keeps REAPER workflow easy while staying useful on stereo tracks.
- UI: generic JUCE parameter editor for v1.
- License note: JUCE is GPL/commercial. Fine for local GPL-compatible work; commercial closed-source distribution needs JUCE commercial terms or framework swap.

## DSP Design

### Detector

- Use 24 log-spaced bands between user low/high frequency bounds.
- Each band has a resonant band-pass detector.
- Detector reads mono sum of input.
- Track band RMS with attack/release smoothing.
- Ignore bands outside selected frequency range.

### Target Curve

For each band center `f`:

```text
targetDb(f) = tiltDbPerOctave * log2(f / referenceFrequency)
```

Then align target and measured spectra by subtracting their weighted average difference. This means plugin corrects tone shape, not overall loudness.

### Correction

For each band:

```text
errorDb = targetShapeDb - measuredShapeDb
correctionDb = clamp(errorDb * strength, -maxCorrectionDb, +maxCorrectionDb)
```

- Default max correction: `12 dB`.
- Apply with a bank of peaking EQ filters.
- Smooth gain changes per block.
- Q set from band spacing.

### Mix

- Processed signal from corrective EQ bank.
- Wet/dry mix parameter blends corrected signal with original.

### Oversampling

- Expose `1x`, `2x`, `4x`.
- v1 keeps this parameter ready in UI and code path.
- For this broadband EQ algorithm, oversampling has limited value; it is mostly useful if later saturation or sharper nonlinear processing is added.

## Parameters

| Parameter | Range | Default | Notes |
| --- | --- | --- | --- |
| Curve | Brown, Dark Brown, Gentle Brown | Brown | Preset target families |
| Tilt | `0` to `100` | `25` | Generic tilt amount; maps to `0` to `-6 dB/oct` |
| Flip Tilt | off/on | off | Mirrors tilt to positive slope |
| Strength | `0` to `100%` | `50%` | Correction depth |
| Mix | `0` to `100%` | `100%` | Wet/dry |
| Low Frequency | `20` to `1000 Hz` | `20 Hz` | Lower correction bound |
| High Frequency | `1000` to `20000 Hz` | `20000 Hz` | Upper correction bound |
| Max Correction | `1` to `18 dB` | `12 dB` | Safety clamp |
| Speed | `0` to `100%` | `45%` | Detector/correction response |
| Oversampling | `1x`, `2x`, `4x` | `1x` | Prepared for future heavier DSP |
| Bypass | off/on | off | Host-compatible bypass |

## Milestones

1. Scaffold JUCE VST3 CMake project.
2. Implement processor parameters and mono/stereo bus layout.
3. Implement `BrownCurveBalancer` DSP class.
4. Add tests for target curve math, range bounds, and correction direction.
5. Build VST3 locally.
6. Load in REAPER.
7. Verify with TDR Prism:
   - Pink-ish input should be darkened toward `-4.5 dB/oct`.
   - Muddy/boomy speech should reduce low-mid excess.
   - Thin speech should restore low/mid body without runaway boosts.
8. Tune smoothing and clamp values from listening tests.

## Manual REAPER Test

1. Insert plugin before TDR Prism.
2. Feed speech or noise through track.
3. Set Prism to average/slow mode.
4. Start with:
   - Curve: Brown
   - Tilt: `-4.5`
   - Strength: `50%`
   - Mix: `100%`
   - Range: `20 Hz` to `20 kHz`
5. Raise Strength until spectral movement is obvious.
6. Lower Mix if voice sounds over-processed.
7. Try range `80 Hz` to `12 kHz` for voice-only correction.

## Open Questions

- Final plugin name.
- Whether custom UI is worth building after DSP validation.
- Whether mono-only bus should be enforced, or stereo compatibility should remain.
- Whether commercial distribution is planned, because that affects JUCE licensing.
