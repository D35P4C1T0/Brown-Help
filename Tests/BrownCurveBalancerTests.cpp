#include "BrownCurveBalancer.h"
#include "Parameters.h"

#include <cmath>
#include <iostream>

namespace
{
bool nearlyEqual(float a, float b, float tolerance = 0.001f)
{
    return std::abs(a - b) <= tolerance;
}

int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}
}

int main()
{
    using BrownHelp::BrownCurveBalancer;

    if (! nearlyEqual(BrownCurveBalancer::targetDbForFrequency(1000.0f, 1000.0f, -4.5f), 0.0f))
        return fail("reference frequency should be 0 dB");

    if (! nearlyEqual(BrownCurveBalancer::targetDbForFrequency(2000.0f, 1000.0f, -4.5f), -4.5f))
        return fail("one octave up should equal tilt");

    if (! nearlyEqual(BrownCurveBalancer::targetDbForFrequency(500.0f, 1000.0f, -4.5f), 4.5f))
        return fail("one octave down should invert tilt");

    if (! nearlyEqual(BrownHelp::tiltControlToDbPerOctave(100.0f, 1, false), -6.0f))
        return fail("minimum tilt control mismatch");

    if (! nearlyEqual(BrownHelp::tiltControlToDbPerOctave(100.0f, 1, true), 6.0f))
        return fail("maximum tilt control mismatch");

    if (! nearlyEqual(BrownHelp::tiltControlToDbPerOctave(25.0f, 1, false), -1.5f, 0.02f))
        return fail("default tilt control mismatch");

    if (! nearlyEqual(BrownHelp::tiltControlToDbPerOctave(100.0f, 0, false), -3.0f))
        return fail("gentle curve range mismatch");

    if (! nearlyEqual(BrownHelp::tiltControlToDbPerOctave(100.0f, 2, false), -9.0f))
        return fail("dark curve range mismatch");

    BrownCurveBalancer balancer;
    balancer.prepare(48000.0, 512, 1);

    juce::AudioBuffer<float> buffer(1, 512);
    buffer.clear();

    BrownCurveBalancer::Settings settings;
    balancer.process(buffer, settings);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        if (! std::isfinite(buffer.getSample(0, sample)))
            return fail("silence processing produced non-finite sample");
    }

    const auto silentAnalysis = balancer.getAnalysisSnapshot();

    if (silentAnalysis.signalPresent)
        return fail("silence should not engage the adaptive correction");

    for (const auto correction : silentAnalysis.correctionDb)
    {
        if (! nearlyEqual(correction, 0.0f))
            return fail("silence should leave correction at unity");
    }

    juce::AudioBuffer<float> tone(1, 4096);

    for (int sample = 0; sample < tone.getNumSamples(); ++sample)
    {
        const auto phase = static_cast<float>(sample) * 2.0f * 3.14159265359f * 220.0f / 48000.0f;
        tone.setSample(0, sample, 0.8f * std::sin(phase));
    }

    settings.strength = 1.0f;
    settings.maxCorrectionDb = 18.0f;
    settings.mix = 0.0f;

    juce::AudioBuffer<float> originalTone;
    originalTone.makeCopyOf(tone);

    for (int block = 0; block < 20; ++block)
        balancer.process(tone, settings);

    for (int sample = 0; sample < tone.getNumSamples(); ++sample)
    {
        if (! nearlyEqual(tone.getSample(0, sample), originalTone.getSample(0, sample), 0.00001f))
            return fail("zero-percent mix should be transparent");
    }

    const auto toneAnalysis = balancer.getAnalysisSnapshot();

    if (! toneAnalysis.signalPresent)
        return fail("audible input should engage analysis");

    auto correctionMagnitude = 0.0f;

    for (const auto correction : toneAnalysis.correctionDb)
        correctionMagnitude = std::max(correctionMagnitude, std::abs(correction));

    if (correctionMagnitude < 0.05f)
        return fail("strongly non-target input should produce a correction");

    std::cout << "BrownHelpTests passed\n";
    return 0;
}
