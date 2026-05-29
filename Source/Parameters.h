#pragma once

#include <algorithm>

namespace BrownHelp
{
inline constexpr auto curveId = "curve";
inline constexpr auto tiltId = "tilt";
inline constexpr auto tiltFlipId = "tiltFlip";
inline constexpr auto strengthId = "strength";
inline constexpr auto mixId = "mix";
inline constexpr auto lowFrequencyId = "lowFrequency";
inline constexpr auto highFrequencyId = "highFrequency";
inline constexpr auto maxCorrectionId = "maxCorrection";
inline constexpr auto speedId = "speed";
inline constexpr auto oversamplingId = "oversampling";
inline constexpr auto highPassEnabledId = "highPassEnabled";
inline constexpr auto highPassFrequencyId = "highPassFrequency";
inline constexpr auto highPassSlopeId = "highPassSlope";
inline constexpr auto saturationEnabledId = "saturationEnabled";
inline constexpr auto saturationFrequencyId = "saturationFrequency";
inline constexpr auto saturationDriveId = "saturationDrive";
inline constexpr auto saturationMixId = "saturationMix";
inline constexpr auto bypassId = "bypass";

// UI exposes Tilt as a generic amount. Flip mirrors the slope to the bright side.
inline float tiltControlToDbPerOctave(float controlValue, int curveChoice, bool flipped)
{
    const auto normalised = std::clamp(controlValue, 0.0f, 100.0f) / 100.0f;
    const auto sign = flipped ? 1.0f : -1.0f;
    const auto baseSlope = sign * normalised * 6.0f;

    switch (curveChoice)
    {
        case 0: return baseSlope * 0.65f;
        case 2: return baseSlope;
        default: return baseSlope;
    }
}
}
