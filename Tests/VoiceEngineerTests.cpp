#include "VoiceEngineer.h"

#include <cmath>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}

void fillVoiceBlock(juce::AudioBuffer<float>& buffer, double& phase, bool addSibilance)
{
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto time = phase / 48000.0;
        const auto value = 0.08 * std::sin(2.0 * juce::MathConstants<double>::pi * 125.0 * time)
                           + (addSibilance ? 0.025 * std::sin(2.0 * juce::MathConstants<double>::pi * 6000.0 * time) : 0.0);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, sample, static_cast<float>(value));
        phase += 1.0;
    }
}
}

int main()
{
    BrownHelp::VoiceEngineer engineer;
    engineer.prepare(48000.0, 512, 2);

    if (engineer.getLatencySamples() != 240)
        return fail("limiter should report 5 ms look-ahead latency");

    BrownHelp::VoiceEngineer::Settings settings;
    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    engineer.process(buffer, settings, false);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite(buffer.getSample(channel, sample)))
                return fail("silence produced a non-finite sample");

    double phase = 0.0;
    for (int block = 0; block < 450; ++block)
    {
        fillVoiceBlock(buffer, phase, false);
        engineer.process(buffer, settings, false);
    }

    for (int block = 0; block < 500; ++block)
    {
        fillVoiceBlock(buffer, phase, true);
        engineer.process(buffer, settings, false);
    }

    const auto analysis = engineer.getAnalysisSnapshot();
    if (analysis.fundamentalHz < 115.0f || analysis.fundamentalHz > 135.0f)
        return fail("fundamental detector did not learn the 125 Hz voice floor");
    if (analysis.sibilanceHz < 5700.0f || analysis.sibilanceHz > 6300.0f)
        return fail("sibilance detector did not learn the 6 kHz peak");
    if (std::abs((analysis.loudnessLufs + analysis.normalizerGainDb) - BrownHelp::VoiceEngineer::targetLufs) > 1.0f)
        return fail("streaming loudness normalizer did not converge on -14 LUFS");
    if (analysis.fundamentalCorrectionDb > 0.001f || analysis.sibilanceCorrectionDb > 0.001f)
        return fail("automatic band balancing must never boost");

    const auto correctedFundamental = analysis.fundamentalRmsDb + analysis.fundamentalCorrectionDb;
    const auto correctedSibilance = analysis.sibilanceRmsDb + analysis.sibilanceCorrectionDb;
    if (correctedFundamental > -39.0f || correctedSibilance > -39.0f)
        return fail("automatic band balancing did not respect the -40 dB RMS ceiling");
    if (std::abs(correctedFundamental - correctedSibilance) > 1.0f)
        return fail("fundamental and sibilance peaks were not balanced");

    engineer.reset();
    buffer.clear();
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, sample, sample % 2 == 0 ? 4.0f : -4.0f);
    engineer.process(buffer, settings, false);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (std::abs(buffer.getSample(channel, sample)) > 1.00001f)
                return fail("limiter exceeded the 0 dBFS ceiling");

    std::cout << "VoiceEngineerTests passed\n";
    return 0;
}
