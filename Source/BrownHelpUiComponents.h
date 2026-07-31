#pragma once

#include "BrownHelpProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace BrownHelp
{
class SpectrumAnalyzer final : public juce::Component,
                               private juce::Timer
{
public:
    explicit SpectrumAnalyzer(BrownHelpProcessor& processorToUse);
    void paint(juce::Graphics& graphics) override;

private:
    void timerCallback() override;
    BrownHelpProcessor& processor;
};
}
