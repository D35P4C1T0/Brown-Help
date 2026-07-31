# Brown Help 0.3 DSP Plan

## Goal

Automate podcast voice preparation: streaming `-14 LUFS` normalization, fundamental/sibilance matching at or below `-40 dB RMS`, optional cut-only shelves, and final `0 dBFS` protection.

## Implemented Flow

1. Stereo-linked K-weighted gated loudness meter
2. Smooth normalization gain targeting `-14 LUFS`
3. `4096`-sample analysis frames
4. YIN fundamental detector over `100-600 Hz`, `40 ms` frames, `5 ms` hops
5. Five-frame median tracking, confidence rejection, first-crest octave validation
6. Optional manual F0 override over `100-600 Hz`
7. Sibilance detector over `3.5-10 kHz`
8. Highest-RMS learning for both bands
9. Cut-only peak EQ to common level no higher than `-40 dB RMS`
10. Optional low/high reduction shelves, `0-12 dB`, spanning `20 Hz-20 kHz`
11. Linked `5 ms` look-ahead limiter, `0 dBFS` sample ceiling
12. Processed-output FFT with `+4.5 dB/oct` display tilt

## Validation

- Silence remains finite
- `125 Hz` synthetic voice fundamental is tracked with high confidence
- `60 Hz` rumble and a louder `250 Hz` harmonic do not displace `125 Hz` F0
- Manual fundamental override selects exact requested frequency
- Shelf parameter endpoints reach `20 Hz` and `20 kHz`
- `6 kHz` synthetic sibilance is learned
- Normalizer converges to `-14 LUFS`
- F0/S correction never boosts
- Corrected learned peaks match and remain at or below `-40 dB RMS`
- Limiter output never exceeds `0 dBFS`
- VST state round-trips new shelf parameters
- Processor reports `5 ms` limiter latency

## Known Real-Time Constraint

Integrated loudness is causal and converges after playback starts. Exact first-sample whole-file normalization requires offline two-pass analysis and is outside a real-time plugin insert.
