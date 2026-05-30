#pragma once

#include "BrownHelpProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace BrownHelp
{
class TiltPreview final : public juce::Component,
                          private juce::Timer
{
public:
    explicit TiltPreview(BrownHelpProcessor& processorToUse);
    void paint(juce::Graphics& graphics) override;

private:
    void timerCallback() override;

    BrownHelpProcessor& processor;
};

class FrequencySlider final : public juce::Slider
{
public:
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

private:
    float wheelAccumulator = 0.0f;
};
}
