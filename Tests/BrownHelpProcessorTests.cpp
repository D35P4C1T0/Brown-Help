#include "BrownHelpProcessor.h"

#include <cmath>
#include <cstring>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}

void setParameter(BrownHelp::BrownHelpProcessor& processor, const char* id, float plainValue)
{
    auto* parameter = processor.getParameters().getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    BrownHelp::BrownHelpProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer(1, 512);

    buffer.clear();
    buffer.applyGain(1.5f);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        buffer.setSample(0, sample, sample % 2 == 0 ? 1.5f : -1.5f);

    processor.processBlock(buffer, midi);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto value = buffer.getSample(0, sample);

        if (! std::isfinite(value))
            return fail("processor produced a non-finite sample");

        if (std::abs(value) > 0.981f)
            return fail("output guard did not enforce its ceiling");
    }

    setParameter(processor, BrownHelp::bypassId, 1.0f);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        buffer.setSample(0, sample, 1.2f * std::sin(0.07f * static_cast<float>(sample)));

    juce::AudioBuffer<float> bypassReference;
    bypassReference.makeCopyOf(buffer);
    processor.processBlock(buffer, midi);

    if (std::memcmp(buffer.getReadPointer(0),
                    bypassReference.getReadPointer(0),
                    static_cast<size_t>(buffer.getNumSamples()) * sizeof(float)) != 0)
        return fail("1x bypass should be bit-transparent");

    setParameter(processor, BrownHelp::oversamplingId, 2.0f);
    buffer.clear();
    processor.processBlock(buffer, midi);

    if (processor.getLatencySamples() <= 0)
        return fail("oversampling latency was not reported to the host");

    setParameter(processor, BrownHelp::strengthId, 0.73f);
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    setParameter(processor, BrownHelp::strengthId, 0.1f);
    processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    const auto restoredStrength = processor.getParameters().getRawParameterValue(BrownHelp::strengthId)->load();

    if (std::abs(restoredStrength - 0.73f) > 0.001f)
        return fail("parameter state did not round-trip");

    std::cout << "BrownHelpProcessorTests passed\n";
    return 0;
}
