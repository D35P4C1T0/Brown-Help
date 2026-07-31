# Brown Help 0.2 DSP Plan

## Goal

Automate podcast voice preparation: streaming `-14 LUFS` normalization, fundamental/sibilance matching at or below `-40 dB RMS`, optional cut-only shelves, and final `0 dBFS` protection.

## Implemented Flow

1. Stereo-linked K-weighted gated loudness meter
2. Smooth normalization gain targeting `-14 LUFS`
3. `4096`-sample analysis frames
4. Fundamental detector over `65-350 Hz`
5. Sibilance detector over `3.5-10 kHz`
6. Highest-RMS learning for both bands
7. Cut-only peak EQ to common level no higher than `-40 dB RMS`
8. Optional low/high reduction shelves, `0-12 dB`
9. Linked `5 ms` look-ahead limiter, `0 dBFS` sample ceiling
10. Processed-output FFT with `+4.5 dB/oct` display tilt

## Validation

- Silence remains finite
- `125 Hz` synthetic voice fundamental is learned
- `6 kHz` synthetic sibilance is learned
- Normalizer converges to `-14 LUFS`
- F0/S correction never boosts
- Corrected learned peaks match and remain at or below `-40 dB RMS`
- Limiter output never exceeds `0 dBFS`
- VST state round-trips new shelf parameters
- Processor reports `5 ms` limiter latency

## Known Real-Time Constraint

Integrated loudness is causal and converges after playback starts. Exact first-sample whole-file normalization requires offline two-pass analysis and is outside a real-time VST insert.
