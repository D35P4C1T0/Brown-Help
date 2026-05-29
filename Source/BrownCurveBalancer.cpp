#include "BrownCurveBalancer.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace BrownHelp
{
namespace
{
constexpr float epsilon = 1.0e-9f;
constexpr float referenceFrequencyHz = 1000.0f;
constexpr float outputCeiling = 0.98f;

float clampFrequency(float value, float minimum, float maximum)
{
    return std::clamp(value, minimum, maximum);
}

float dbFromPower(float power)
{
    return 10.0f * std::log10(std::max(power, epsilon));
}
}

void BrownCurveBalancer::prepare(double sampleRate, int, int channels)
{
    currentSampleRate = sampleRate;
    currentChannels = std::max(1, channels);

    for (auto& band : bands)
    {
        band.envelope = 0.0f;
        band.smoothedGainDb = 0.0f;
        band.detectors.resize(static_cast<size_t>(currentChannels));
        band.eq.resize(static_cast<size_t>(currentChannels));
    }

    previousLowHz = -1.0f;
    previousHighHz = -1.0f;
}

void BrownCurveBalancer::reset()
{
    for (auto& band : bands)
    {
        band.envelope = 0.0f;
        band.smoothedGainDb = 0.0f;

        for (auto& detector : band.detectors)
            detector.reset();

        for (auto& eq : band.eq)
            eq.reset();
    }
}

void BrownCurveBalancer::process(juce::AudioBuffer<float>& buffer, const Settings& settings)
{
    const auto channels = buffer.getNumChannels();
    const auto samples = buffer.getNumSamples();

    if (channels == 0 || samples == 0)
        return;

    updateBands(settings, channels);

    const auto mix = std::clamp(settings.mix, 0.0f, 1.0f);
    const auto needsDryCopy = mix < 0.999f;

    if (needsDryCopy)
    {
        dryBuffer.setSize(channels, samples, false, false, true);

        for (int channel = 0; channel < channels; ++channel)
            dryBuffer.copyFrom(channel, 0, buffer, channel, 0, samples);
    }

    const auto& detectorInput = needsDryCopy ? dryBuffer : buffer;
    const auto detectorCoeff = std::pow(detectorCoefficient(settings), static_cast<float>(samples));

    std::array<float, bandCount> measuredDb {};
    std::array<float, bandCount> targetDb {};

    for (int bandIndex = 0; bandIndex < bandCount; ++bandIndex)
    {
        auto& band = bands[static_cast<size_t>(bandIndex)];
        auto monoPower = 0.0f;

        for (int channel = 0; channel < channels; ++channel)
        {
            auto& detector = band.detectors[static_cast<size_t>(channel)];
            const auto* input = detectorInput.getReadPointer(channel);

            for (int sample = 0; sample < samples; ++sample)
            {
                const auto filtered = detector.processSample(input[sample]);
                monoPower += filtered * filtered;
            }
        }

        const auto blockPower = monoPower / static_cast<float>(samples * channels);
        band.envelope = detectorCoeff * band.envelope + (1.0f - detectorCoeff) * blockPower;

        measuredDb[static_cast<size_t>(bandIndex)] = dbFromPower(band.envelope);
        targetDb[static_cast<size_t>(bandIndex)] =
            targetDbForFrequency(band.frequencyHz, referenceFrequencyHz, curveTilt(settings.curve, settings.tiltDbPerOctave));
    }

    const auto measuredAverage =
        std::accumulate(measuredDb.begin(), measuredDb.end(), 0.0f) / static_cast<float>(bandCount);
    const auto targetAverage =
        std::accumulate(targetDb.begin(), targetDb.end(), 0.0f) / static_cast<float>(bandCount);

    const auto gainCoeff = std::pow(gainSmoothingCoefficient(settings), static_cast<float>(samples));
    const auto maxCorrection = std::clamp(settings.maxCorrectionDb, 1.0f, 18.0f);
    const auto strength = std::clamp(settings.strength, 0.0f, 1.0f);
    const auto low = std::min(settings.lowFrequencyHz, settings.highFrequencyHz);
    const auto high = std::max(settings.lowFrequencyHz, settings.highFrequencyHz);

    for (int bandIndex = 0; bandIndex < bandCount; ++bandIndex)
    {
        auto& band = bands[static_cast<size_t>(bandIndex)];
        auto desiredGainDb = 0.0f;

        if (band.frequencyHz >= low && band.frequencyHz <= high)
        {
            const auto measuredShape = measuredDb[static_cast<size_t>(bandIndex)] - measuredAverage;
            const auto targetShape = targetDb[static_cast<size_t>(bandIndex)] - targetAverage;
            const auto errorDb = targetShape - measuredShape;
            // Spoken-word correction should tame excess more readily than it boosts missing bands.
            const auto boostScale = errorDb > 0.0f ? 0.18f : 0.85f;
            desiredGainDb = std::clamp(errorDb * strength * boostScale, -maxCorrection, maxCorrection * 0.2f);
        }

        band.smoothedGainDb = gainCoeff * band.smoothedGainDb + (1.0f - gainCoeff) * desiredGainDb;
        updateEqCoefficients(band, channels, 1.0f, band.smoothedGainDb);
    }

    for (auto& band : bands)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            auto& eq = band.eq[static_cast<size_t>(channel)];
            auto* data = buffer.getWritePointer(channel);

            for (int sample = 0; sample < samples; ++sample)
                data[sample] = eq.processSample(data[sample]);
        }
    }

    if (needsDryCopy)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            auto* wet = buffer.getWritePointer(channel);
            const auto* dryData = dryBuffer.getReadPointer(channel);

            for (int sample = 0; sample < samples; ++sample)
                wet[sample] = dryData[sample] + (wet[sample] - dryData[sample]) * mix;
        }
    }

    auto peak = 0.0f;

    for (int channel = 0; channel < channels; ++channel)
    {
        const auto* data = buffer.getReadPointer(channel);

        for (int sample = 0; sample < samples; ++sample)
            peak = std::max(peak, std::abs(data[sample]));
    }

    if (peak > outputCeiling)
        buffer.applyGain(outputCeiling / peak);
}

float BrownCurveBalancer::targetDbForFrequency(float frequencyHz, float referenceHz, float tiltDbPerOctave)
{
    return tiltDbPerOctave * std::log2(std::max(frequencyHz, 1.0f) / std::max(referenceHz, 1.0f));
}

float BrownCurveBalancer::curveTilt(Curve curve, float userTiltDbPerOctave)
{
    juce::ignoreUnused(curve);
    return userTiltDbPerOctave;
}

void BrownCurveBalancer::updateBands(const Settings& settings, int channels)
{
    const auto nyquist = static_cast<float>(currentSampleRate * 0.475);
    const auto low = clampFrequency(std::min(settings.lowFrequencyHz, settings.highFrequencyHz), 20.0f, nyquist * 0.5f);
    const auto high = clampFrequency(std::max(settings.lowFrequencyHz, settings.highFrequencyHz), low * 1.5f, nyquist);

    if (std::abs(low - previousLowHz) < 0.01f && std::abs(high - previousHighHz) < 0.01f && channels == currentChannels)
        return;

    currentChannels = channels;
    previousLowHz = low;
    previousHighHz = high;

    const auto lowLog = std::log(low);
    const auto highLog = std::log(high);
    const auto q = 0.55f;

    for (int index = 0; index < bandCount; ++index)
    {
        auto& band = bands[static_cast<size_t>(index)];
        const auto ratio = static_cast<float>(index) / static_cast<float>(bandCount - 1);
        band.frequencyHz = std::exp(lowLog + (highLog - lowLog) * ratio);
        band.detectors.resize(static_cast<size_t>(channels));
        band.eq.resize(static_cast<size_t>(channels));
        band.appliedGainDb = 1000.0f;

        updateDetectorCoefficients(band, channels, q);
        updateEqCoefficients(band, channels, q, band.smoothedGainDb);
    }
}

void BrownCurveBalancer::updateDetectorCoefficients(Band& band, int channels, float q)
{
    auto coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(currentSampleRate, band.frequencyHz, q);

    for (int channel = 0; channel < channels; ++channel)
        *band.detectors[static_cast<size_t>(channel)].coefficients = *coefficients;
}

void BrownCurveBalancer::updateEqCoefficients(Band& band, int channels, float q, float gainDb)
{
    if (std::abs(gainDb - band.appliedGainDb) < 0.02f)
        return;

    band.appliedGainDb = gainDb;

    auto coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        currentSampleRate,
        band.frequencyHz,
        q,
        juce::Decibels::decibelsToGain(gainDb));

    for (int channel = 0; channel < channels; ++channel)
        *band.eq[static_cast<size_t>(channel)].coefficients = *coefficients;
}

float BrownCurveBalancer::detectorCoefficient(const Settings& settings) const
{
    const auto speed = std::clamp(settings.speed, 0.0f, 1.0f);
    const auto timeSeconds = juce::jmap(speed, 2.0f, 0.08f);
    return std::exp(-static_cast<float>(1.0 / (timeSeconds * currentSampleRate)));
}

float BrownCurveBalancer::gainSmoothingCoefficient(const Settings& settings) const
{
    const auto speed = std::clamp(settings.speed, 0.0f, 1.0f);
    const auto timeSeconds = juce::jmap(speed, 1.5f, 0.04f);
    return std::exp(-static_cast<float>(1.0 / (timeSeconds * currentSampleRate)));
}
}
