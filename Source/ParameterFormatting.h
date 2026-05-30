#pragma once

#include <cmath>
#include <juce_audio_processors/juce_audio_processors.h>

namespace BrownHelp::ParameterFormatting
{
inline juce::String roundedPercent01(double value)
{
    return juce::String(static_cast<int>(std::round(value * 100.0))) + "%";
}

inline juce::String percent01(double value)
{
    return juce::String(value * 100.0, 1) + "%";
}

inline double percent01FromText(const juce::String& text)
{
    return text.retainCharacters("0123456789.").getDoubleValue() / 100.0;
}

inline juce::String tiltPercent(double value)
{
    return juce::String(value, 0) + "%";
}

inline double numberFromText(const juce::String& text)
{
    return text.retainCharacters("0123456789.").getDoubleValue();
}

inline juce::String decibels(double value)
{
    return juce::String(value, 1) + " dB";
}

inline juce::String frequency(double value)
{
    if (value >= 1000.0)
        return juce::String(value / 1000.0, value >= 10000.0 ? 1 : 2) + " kHz";

    return juce::String(value, 0) + " Hz";
}

inline double frequencyFromText(const juce::String& text)
{
    const auto lower = text.toLowerCase();
    const auto value = lower.retainCharacters("0123456789.").getDoubleValue();
    return lower.contains("k") ? value * 1000.0 : value;
}
}
