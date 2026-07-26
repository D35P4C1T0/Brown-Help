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
constexpr int eqBandStride = 2;
constexpr float detectorQ = 0.9f;
constexpr float correctionQ = 1.2f;

float clampFrequency(float value, float minimum, float maximum)
{
    return std::clamp(value, minimum, maximum);
}

float dbFromPower(float power)
{
    return 10.0f * std::log10(std::max(power, epsilon));
}
}

void BrownCurveBalancer::prepare(double sampleRate, int maximumBlockSize, int channels)
{
    currentSampleRate = sampleRate;
    currentChannels = std::max(1, channels);
    dryBuffer.setSize(currentChannels, std::max(1, maximumBlockSize), false, true, false);

    for (auto& band : bands)
    {
        band.envelope = 0.0f;
        band.smoothedGainDb = 0.0f;
        band.detectors.resize(static_cast<size_t>(currentChannels));
        band.eq.resize(static_cast<size_t>(currentChannels));
    }

    for (auto& frequency : publishedFrequenciesHz)
        frequency.store(0.0f, std::memory_order_relaxed);

    for (auto& gain : publishedCorrectionDb)
        gain.store(0.0f, std::memory_order_relaxed);

    publishedSignalPresent.store(false, std::memory_order_relaxed);
    previousLowHz = -1.0f;
    previousHighHz = -1.0f;
}

void BrownCurveBalancer::reset()
{
    for (auto& band : bands)
    {
        band.envelope = 0.0f;
        band.smoothedGainDb = 0.0f;
        band.appliedGainDb = 1000.0f;

        for (auto& detector : band.detectors)
            detector.reset();

        for (auto& eq : band.eq)
            eq.reset();
    }

    for (auto& gain : publishedCorrectionDb)
        gain.store(0.0f, std::memory_order_relaxed);

    publishedSignalPresent.store(false, std::memory_order_relaxed);
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
    auto inputPower = 0.0f;

    for (int channel = 0; channel < channels; ++channel)
    {
        const auto* input = detectorInput.getReadPointer(channel);

        for (int sample = 0; sample < samples; ++sample)
            inputPower += input[sample] * input[sample];
    }

    inputPower /= static_cast<float>(samples * channels);

    std::array<float, bandCount> measuredDb {};
    std::array<float, bandCount> measuredShapeDb {};
    std::array<float, bandCount> targetDb {};
    std::array<float, bandCount> confidence {};
    std::array<float, bandCount> desiredGainDb {};

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
        const auto speed = std::clamp(settings.speed, 0.0f, 1.0f);
        const auto baseTimeSeconds = juce::jmap(speed, 1.8f, 0.10f);
        const auto timeSeconds = blockPower > band.envelope ? baseTimeSeconds * 0.35f : baseTimeSeconds * 1.6f;
        const auto detectorCoeff = std::exp(-static_cast<float>(samples)
                                            / static_cast<float>(timeSeconds * currentSampleRate));
        band.envelope = detectorCoeff * band.envelope + (1.0f - detectorCoeff) * blockPower;

        measuredDb[static_cast<size_t>(bandIndex)] = dbFromPower(band.envelope);
        targetDb[static_cast<size_t>(bandIndex)] =
            targetDbForFrequency(band.frequencyHz, referenceFrequencyHz, settings.tiltDbPerOctave);
    }

    // Smooth over neighbouring detector bands so the EQ follows broad tone rather
    // than individual harmonics, sibilants, or narrow room resonances.
    constexpr std::array<float, 5> spectralKernel { 1.0f, 2.0f, 3.0f, 2.0f, 1.0f };

    for (int bandIndex = 0; bandIndex < bandCount; ++bandIndex)
    {
        auto sum = 0.0f;
        auto weight = 0.0f;

        for (int offset = -2; offset <= 2; ++offset)
        {
            const auto sourceIndex = std::clamp(bandIndex + offset, 0, bandCount - 1);
            const auto kernelWeight = spectralKernel[static_cast<size_t>(offset + 2)];
            sum += measuredDb[static_cast<size_t>(sourceIndex)] * kernelWeight;
            weight += kernelWeight;
        }

        measuredShapeDb[static_cast<size_t>(bandIndex)] = sum / weight;
    }

    const auto loudestBandDb = *std::max_element(measuredShapeDb.begin(), measuredShapeDb.end());
    const auto confidenceFloorDb = std::max(-78.0f, loudestBandDb - 42.0f);
    auto alignment = 0.0f;
    auto alignmentWeight = 0.0f;

    for (int bandIndex = 0; bandIndex < bandCount; ++bandIndex)
    {
        const auto bandConfidence = std::clamp(
            (measuredShapeDb[static_cast<size_t>(bandIndex)] - confidenceFloorDb) / 12.0f,
            0.0f,
            1.0f);
        confidence[static_cast<size_t>(bandIndex)] = bandConfidence;
        alignment += bandConfidence
                     * (targetDb[static_cast<size_t>(bandIndex)]
                        - measuredShapeDb[static_cast<size_t>(bandIndex)]);
        alignmentWeight += bandConfidence;
    }

    const auto signalPresent = dbFromPower(inputPower) > -72.0f
                               && loudestBandDb > -78.0f
                               && alignmentWeight > 0.5f;
    const auto levelAlignmentDb = alignmentWeight > 0.0f ? alignment / alignmentWeight : 0.0f;

    const auto gainCoeff = std::pow(gainSmoothingCoefficient(settings), static_cast<float>(samples));
    const auto maxCorrection = std::clamp(settings.maxCorrectionDb, 1.0f, 18.0f);
    const auto strength = std::clamp(settings.strength, 0.0f, 1.0f);

    for (int bandIndex = 0; bandIndex < bandCount; ++bandIndex)
    {
        if (signalPresent)
        {
            auto errorDb = targetDb[static_cast<size_t>(bandIndex)]
                           - measuredShapeDb[static_cast<size_t>(bandIndex)]
                           - levelAlignmentDb;
            const auto sign = errorDb < 0.0f ? -1.0f : 1.0f;
            errorDb = sign * std::max(0.0f, std::abs(errorDb) - 0.75f);

            // Cuts remain more assertive than boosts so absent content and noise
            // are not pulled forward, while still allowing useful body/presence recovery.
            const auto directionScale = errorDb > 0.0f ? 0.38f : 0.72f;
            const auto confidenceScale = errorDb > 0.0f
                                             ? juce::jmap(confidence[static_cast<size_t>(bandIndex)], 0.25f, 1.0f)
                                             : 1.0f;
            desiredGainDb[static_cast<size_t>(bandIndex)] = std::clamp(
                errorDb * strength * directionScale * confidenceScale,
                -maxCorrection,
                maxCorrection * 0.5f);
        }
    }

    // One more light spatial pass prevents adjacent peak filters from fighting.
    const auto unsmoothedDesired = desiredGainDb;

    for (int bandIndex = 0; bandIndex < bandCount; ++bandIndex)
    {
        const auto previous = unsmoothedDesired[static_cast<size_t>(std::max(0, bandIndex - 1))];
        const auto current = unsmoothedDesired[static_cast<size_t>(bandIndex)];
        const auto next = unsmoothedDesired[static_cast<size_t>(std::min(bandCount - 1, bandIndex + 1))];
        desiredGainDb[static_cast<size_t>(bandIndex)] = 0.25f * previous + 0.5f * current + 0.25f * next;
    }

    for (int bandIndex = 0; bandIndex < bandCount; ++bandIndex)
    {
        auto& band = bands[static_cast<size_t>(bandIndex)];
        band.smoothedGainDb = gainCoeff * band.smoothedGainDb
                              + (1.0f - gainCoeff) * desiredGainDb[static_cast<size_t>(bandIndex)];

        const auto isEqBand = bandIndex % eqBandStride == 0;
        updateEqCoefficients(band, channels, correctionQ, isEqBand ? band.smoothedGainDb : 0.0f);
        publishedFrequenciesHz[static_cast<size_t>(bandIndex)].store(band.frequencyHz, std::memory_order_relaxed);
        publishedCorrectionDb[static_cast<size_t>(bandIndex)].store(band.smoothedGainDb, std::memory_order_relaxed);
    }

    publishedSignalPresent.store(signalPresent, std::memory_order_relaxed);

    for (int bandIndex = 0; bandIndex < bandCount; bandIndex += eqBandStride)
    {
        auto& band = bands[static_cast<size_t>(bandIndex)];

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
}

BrownCurveBalancer::AnalysisSnapshot BrownCurveBalancer::getAnalysisSnapshot() const
{
    AnalysisSnapshot snapshot;

    for (size_t index = 0; index < bands.size(); ++index)
    {
        snapshot.frequenciesHz[index] = publishedFrequenciesHz[index].load(std::memory_order_relaxed);
        snapshot.correctionDb[index] = publishedCorrectionDb[index].load(std::memory_order_relaxed);
    }

    snapshot.signalPresent = publishedSignalPresent.load(std::memory_order_relaxed);
    return snapshot;
}

float BrownCurveBalancer::targetDbForFrequency(float frequencyHz, float referenceHz, float tiltDbPerOctave)
{
    return tiltDbPerOctave * std::log2(std::max(frequencyHz, 1.0f) / std::max(referenceHz, 1.0f));
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
    for (int index = 0; index < bandCount; ++index)
    {
        auto& band = bands[static_cast<size_t>(index)];
        const auto ratio = static_cast<float>(index) / static_cast<float>(bandCount - 1);
        band.frequencyHz = std::exp(lowLog + (highLog - lowLog) * ratio);
        band.detectors.resize(static_cast<size_t>(channels));
        band.eq.resize(static_cast<size_t>(channels));
        band.appliedGainDb = 1000.0f;
        band.envelope = 0.0f;
        band.smoothedGainDb = 0.0f;

        for (auto& detector : band.detectors)
            detector.reset();

        for (auto& eq : band.eq)
            eq.reset();

        updateDetectorCoefficients(band, channels, detectorQ);
        updateEqCoefficients(band, channels, correctionQ, 0.0f);
    }
}

void BrownCurveBalancer::updateDetectorCoefficients(Band& band, int channels, float q)
{
    const auto coefficients =
        juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass(currentSampleRate, band.frequencyHz, q);

    for (int channel = 0; channel < channels; ++channel)
        *band.detectors[static_cast<size_t>(channel)].coefficients = coefficients;
}

void BrownCurveBalancer::updateEqCoefficients(Band& band, int channels, float q, float gainDb)
{
    if (std::abs(gainDb - band.appliedGainDb) < 0.01f)
        return;

    band.appliedGainDb = gainDb;

    const auto coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
        currentSampleRate,
        band.frequencyHz,
        q,
        juce::Decibels::decibelsToGain(gainDb));

    for (int channel = 0; channel < channels; ++channel)
        *band.eq[static_cast<size_t>(channel)].coefficients = coefficients;
}

float BrownCurveBalancer::gainSmoothingCoefficient(const Settings& settings) const
{
    const auto speed = std::clamp(settings.speed, 0.0f, 1.0f);
    const auto timeSeconds = juce::jmap(speed, 1.25f, 0.06f);
    return std::exp(-static_cast<float>(1.0 / (timeSeconds * currentSampleRate)));
}
}
