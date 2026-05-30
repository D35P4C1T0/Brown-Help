#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace BrownHelp::Ui
{
inline constexpr auto backgroundColour = 0xff171c1f;
inline constexpr auto panelColour = 0xff1b2226;
inline constexpr auto outlineColour = 0xff384044;
inline constexpr auto textColour = 0xfff1eadf;
inline constexpr auto mutedTextColour = 0xffb9b9b5;
inline constexpr auto accentColour = 0xffc8ad84;

inline float parameter(const juce::AudioProcessorValueTreeState& parameters, const char* id)
{
    return parameters.getRawParameterValue(id)->load();
}
}
