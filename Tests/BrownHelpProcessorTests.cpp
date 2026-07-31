#include "BrownHelpProcessor.h"

#include <cmath>
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

    if (processor.getLatencySamples() != 240)
        return fail("processor did not report limiter latency");

    const auto* lowShelfFrequency = processor.getParameters().getParameter(BrownHelp::lowShelfFrequencyId);
    const auto* highShelfFrequency = processor.getParameters().getParameter(BrownHelp::highShelfFrequencyId);
    if (std::abs(lowShelfFrequency->convertFrom0to1(0.0f) - 20.0f) > 0.01f)
        return fail("low shelf does not extend to 20 Hz");
    if (std::abs(highShelfFrequency->convertFrom0to1(1.0f) - 20000.0f) > 0.01f)
        return fail("high shelf does not extend to 20 kHz");

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer(2, 512);
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, sample, sample % 2 == 0 ? 2.0f : -2.0f);
    processor.processBlock(buffer, midi);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = buffer.getSample(channel, sample);
            if (! std::isfinite(value))
                return fail("processor produced a non-finite sample");
            if (std::abs(value) > 1.00001f)
                return fail("processor exceeded 0 dBFS");
        }

    setParameter(processor, BrownHelp::lowShelfEnabledId, 1.0f);
    setParameter(processor, BrownHelp::lowShelfReductionId, 7.3f);
    setParameter(processor, BrownHelp::lowShelfSlopeId, 1.0f);
    setParameter(processor, BrownHelp::manualFundamentalEnabledId, 1.0f);
    setParameter(processor, BrownHelp::manualFundamentalFrequencyId, 173.0f);
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    setParameter(processor, BrownHelp::lowShelfReductionId, 0.0f);
    processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    const auto restored = processor.getParameters().getRawParameterValue(BrownHelp::lowShelfReductionId)->load();
    if (std::abs(restored - 7.3f) > 0.01f)
        return fail("new parameter state did not round-trip");
    const auto restoredManual = processor.getParameters().getRawParameterValue(
        BrownHelp::manualFundamentalFrequencyId)->load();
    if (std::abs(restoredManual - 173.0f) > 0.01f)
        return fail("manual fundamental state did not round-trip");

    setParameter(processor, BrownHelp::bypassId, 1.0f);
    buffer.clear();
    processor.processBlock(buffer, midi);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite(buffer.getSample(channel, sample)))
                return fail("bypass produced a non-finite sample");

    std::cout << "BrownHelpProcessorTests passed\n";
    return 0;
}
