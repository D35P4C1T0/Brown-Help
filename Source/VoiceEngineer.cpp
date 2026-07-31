#include "VoiceEngineer.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace BrownHelp
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float minimumDb = -100.0f;

float powerToDb(double power)
{
    return static_cast<float>(10.0 * std::log10(std::max(power, 1.0e-10)));
}

float gainToDb(float gain)
{
    return juce::Decibels::gainToDecibels(std::max(gain, 1.0e-5f), minimumDb);
}

bool settingsDiffer(const VoiceEngineer::Settings& a, const VoiceEngineer::Settings& b)
{
    return a.lowShelfEnabled != b.lowShelfEnabled
           || std::abs(a.lowShelfFrequencyHz - b.lowShelfFrequencyHz) > 0.01f
           || std::abs(a.lowShelfReductionDb - b.lowShelfReductionDb) > 0.01f
           || a.lowShelfSlope != b.lowShelfSlope
           || a.highShelfEnabled != b.highShelfEnabled
           || std::abs(a.highShelfFrequencyHz - b.highShelfFrequencyHz) > 0.01f
           || std::abs(a.highShelfReductionDb - b.highShelfReductionDb) > 0.01f
           || a.highShelfSlope != b.highShelfSlope;
}
}

void VoiceEngineer::prepare(double sampleRate, int, int channels)
{
    currentSampleRate = std::max(8000.0, sampleRate);
    currentChannels = std::max(1, channels);
    loudnessChunkSamples = std::max(1, static_cast<int>(std::round(currentSampleRate * 0.1)));
    limiterDelaySamples = std::max(1, static_cast<int>(std::round(currentSampleRate * 0.005)));
    pitchDecimation = std::max(1, static_cast<int>(std::round(currentSampleRate / 12000.0)));
    pitchSampleRate = static_cast<float>(currentSampleRate) / static_cast<float>(pitchDecimation);
    pitchFrameSize = std::clamp(static_cast<int>(std::round(pitchSampleRate * 0.04f)),
                                128,
                                maximumPitchFrameSize);
    pitchHopSize = std::max(1, static_cast<int>(std::round(pitchSampleRate * 0.005f)));

    kHighPass.resize(static_cast<size_t>(currentChannels));
    kHighShelf.resize(static_cast<size_t>(currentChannels));
    fundamentalEq.resize(static_cast<size_t>(currentChannels));
    sibilanceEq.resize(static_cast<size_t>(currentChannels));

    for (auto& stage : lowShelf)
        stage.resize(static_cast<size_t>(currentChannels));

    for (auto& stage : highShelf)
        stage.resize(static_cast<size_t>(currentChannels));

    limiterDelay.assign(static_cast<size_t>(currentChannels),
                        std::vector<float>(static_cast<size_t>(limiterDelaySamples), 0.0f));

    const auto highPassCoefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(
        currentSampleRate, 38.1355f, 0.5003f);
    const auto highShelfCoefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf(
        currentSampleRate, 1681.974f, 0.7071f, juce::Decibels::decibelsToGain(4.0f));

    for (int channel = 0; channel < currentChannels; ++channel)
    {
        *kHighPass[static_cast<size_t>(channel)].coefficients = highPassCoefficients;
        *kHighShelf[static_cast<size_t>(channel)].coefficients = highShelfCoefficients;
    }

    windowSum = 0.0f;
    for (int index = 0; index < fftSize; ++index)
    {
        window[static_cast<size_t>(index)] = 0.5f - 0.5f * std::cos(2.0f * pi * static_cast<float>(index)
                                                                   / static_cast<float>(fftSize - 1));
        windowSum += window[static_cast<size_t>(index)];
    }

    reset();
}

void VoiceEngineer::reset()
{
    loudnessChunkPosition = 0;
    loudnessChunkCount = 0;
    loudnessChunks.fill(0.0);
    loudnessHistory.fill(0.0f);
    loudnessHistoryCount = 0;
    loudnessHistoryWrite = 0;
    loudnessChunkEnergy = 0.0;
    measuredLufs = minimumDb;
    smoothedNormalizerGainDb = 0.0f;
    previousNormalizerGain = 1.0f;
    analysisFifo.fill(0.0f);
    pitchFrame.fill(0.0f);
    pitchHistory.fill(0.0f);
    fftData.fill(0.0f);
    binRms.fill(0.0f);
    displayFifo.fill(0.0f);
    displayFftData.fill(0.0f);
    displayBinRms.fill(0.0f);
    analysisFifoPosition = 0;
    displayFifoPosition = 0;
    pitchDecimationCounter = 0;
    pitchWritePosition = 0;
    pitchSamplesCollected = 0;
    pitchHopCounter = 0;
    pitchHistoryCount = 0;
    pitchHistoryPosition = 0;
    pitchPreviousInput = 0.0f;
    pitchHighPassOutput = 0.0f;
    pitchLowPassOne = 0.0f;
    pitchLowPassTwo = 0.0f;
    shelfSettingsValid = false;
    limiterWritePosition = 0;
    limiterHoldRemaining = 0;
    limiterGain = 1.0f;
    limiterReductionDb = 0.0f;
    appliedFundamentalHz = -1.0f;
    appliedSibilanceHz = -1.0f;
    appliedFundamentalGainDb = 100.0f;
    appliedSibilanceGainDb = 100.0f;

    for (auto* filters : { &kHighPass, &kHighShelf, &fundamentalEq, &sibilanceEq })
        for (auto& filter : *filters)
            filter.reset();

    for (auto* stages : { &lowShelf, &highShelf })
        for (auto& stage : *stages)
            for (auto& filter : stage)
                filter.reset();

    for (auto& channel : limiterDelay)
        std::fill(channel.begin(), channel.end(), 0.0f);

    resetLearning();
    publishAnalysis();
}

void VoiceEngineer::resetLearning()
{
    loudnessChunkPosition = 0;
    loudnessChunkCount = 0;
    loudnessChunks.fill(0.0);
    loudnessHistory.fill(0.0f);
    loudnessHistoryCount = 0;
    loudnessHistoryWrite = 0;
    loudnessChunkEnergy = 0.0;
    measuredLufs = minimumDb;
    learnedFundamentalHz = 0.0f;
    activeFundamentalHz = 0.0f;
    fundamentalConfidence = 0.0f;
    pitchHistory.fill(0.0f);
    pitchHistoryCount = 0;
    pitchHistoryPosition = 0;
    fundamentalPeakRmsDb = minimumDb;
    learnedSibilanceHz = 0.0f;
    sibilancePeakRmsDb = minimumDb;
    fundamentalCorrectionDb = 0.0f;
    sibilanceCorrectionDb = 0.0f;
    appliedFundamentalHz = -1.0f;
    appliedSibilanceHz = -1.0f;
    appliedFundamentalGainDb = 100.0f;
    appliedSibilanceGainDb = 100.0f;

    const auto unityLow = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
        currentSampleRate, 120.0f, 1.25f, 1.0f);
    const auto unityHigh = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
        currentSampleRate, 6000.0f, 2.0f, 1.0f);
    for (auto& filter : fundamentalEq)
        *filter.coefficients = unityLow;
    for (auto& filter : sibilanceEq)
        *filter.coefficients = unityHigh;
}

void VoiceEngineer::process(juce::AudioBuffer<float>& buffer, const Settings& settings, bool bypassed)
{
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return;

    currentSettings = settings;

    if (bypassed)
    {
        applyLimiter(buffer, false);
        pushDisplaySamples(buffer);
        return;
    }

    updateLoudness(buffer);
    applyNormalization(buffer);
    pushAnalysisSamples(buffer);
    updateCorrectionFilters(buffer.getNumChannels(), settings.autoBalance);
    applyCorrections(buffer, settings);
    updateShelfFilters(settings, buffer.getNumChannels());
    applyShelves(buffer, settings);
    applyLimiter(buffer, true);
    pushDisplaySamples(buffer);
    publishAnalysis();
}

int VoiceEngineer::getLatencySamples() const
{
    return limiterDelaySamples;
}

void VoiceEngineer::updateLoudness(const juce::AudioBuffer<float>& buffer)
{
    const auto channels = std::min(buffer.getNumChannels(), currentChannels);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto sampleEnergy = 0.0;

        for (int channel = 0; channel < channels; ++channel)
        {
            auto value = buffer.getSample(channel, sample);
            value = kHighShelf[static_cast<size_t>(channel)].processSample(value);
            value = kHighPass[static_cast<size_t>(channel)].processSample(value);
            sampleEnergy += static_cast<double>(value) * static_cast<double>(value);
        }

        loudnessChunkEnergy += sampleEnergy;
        ++loudnessChunkPosition;

        if (loudnessChunkPosition < loudnessChunkSamples)
            continue;

        const auto chunk = loudnessChunkEnergy / static_cast<double>(loudnessChunkSamples);
        loudnessChunks[static_cast<size_t>(loudnessChunkCount % 4)] = chunk;
        ++loudnessChunkCount;
        loudnessChunkPosition = 0;
        loudnessChunkEnergy = 0.0;

        if (loudnessChunkCount < 4)
            continue;

        const auto blockEnergy = std::accumulate(loudnessChunks.begin(), loudnessChunks.end(), 0.0) / 4.0;
        const auto blockLufs = -0.691f + powerToDb(blockEnergy);

        if (blockLufs > -70.0f)
        {
            loudnessHistory[static_cast<size_t>(loudnessHistoryWrite)] = static_cast<float>(blockEnergy);
            loudnessHistoryWrite = (loudnessHistoryWrite + 1) % loudnessHistorySize;
            loudnessHistoryCount = std::min(loudnessHistoryCount + 1, loudnessHistorySize);
        }

        if (loudnessHistoryCount == 0)
            continue;

        auto ungatedEnergy = 0.0;
        for (int index = 0; index < loudnessHistoryCount; ++index)
            ungatedEnergy += loudnessHistory[static_cast<size_t>(index)];
        ungatedEnergy /= static_cast<double>(loudnessHistoryCount);

        const auto relativeGate = -0.691f + powerToDb(ungatedEnergy) - 10.0f;
        const auto gate = std::max(-70.0f, relativeGate);
        auto gatedEnergy = 0.0;
        auto gatedCount = 0;

        for (int index = 0; index < loudnessHistoryCount; ++index)
        {
            const auto energy = loudnessHistory[static_cast<size_t>(index)];
            if (-0.691f + powerToDb(energy) >= gate)
            {
                gatedEnergy += energy;
                ++gatedCount;
            }
        }

        if (gatedCount > 0)
            measuredLufs = -0.691f + powerToDb(gatedEnergy / static_cast<double>(gatedCount));
    }
}

void VoiceEngineer::applyNormalization(juce::AudioBuffer<float>& buffer)
{
    auto desiredGainDb = smoothedNormalizerGainDb;
    if (measuredLufs > -70.0f)
        desiredGainDb = std::clamp(targetLufs - measuredLufs, -36.0f, 36.0f);

    const auto timeSeconds = desiredGainDb < smoothedNormalizerGainDb ? 0.8f : 2.5f;
    const auto smoothing = std::exp(-static_cast<float>(buffer.getNumSamples())
                                    / static_cast<float>(timeSeconds * currentSampleRate));
    smoothedNormalizerGainDb = smoothing * smoothedNormalizerGainDb + (1.0f - smoothing) * desiredGainDb;
    const auto nextGain = juce::Decibels::decibelsToGain(smoothedNormalizerGainDb);
    buffer.applyGainRamp(0, buffer.getNumSamples(), previousNormalizerGain, nextGain);
    previousNormalizerGain = nextGain;
}

void VoiceEngineer::pushAnalysisSamples(const juce::AudioBuffer<float>& buffer)
{
    const auto channels = buffer.getNumChannels();

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto mono = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
            mono += buffer.getSample(channel, sample);
        mono /= static_cast<float>(channels);

        pushPitchSample(mono);
        analysisFifo[static_cast<size_t>(analysisFifoPosition++)] = mono;

        if (analysisFifoPosition == fftSize)
        {
            analyseFrame();
            analysisFifoPosition = 0;
        }
    }
}

void VoiceEngineer::analyseFrame()
{
    for (int index = 0; index < fftSize; ++index)
        fftData[static_cast<size_t>(index)] = analysisFifo[static_cast<size_t>(index)] * window[static_cast<size_t>(index)];
    std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform(fftData.data(), true);

    const auto magnitudeToRms = std::sqrt(2.0f) / windowSum;
    for (int bin = 0; bin <= fftSize / 2; ++bin)
        binRms[static_cast<size_t>(bin)] = fftData[static_cast<size_t>(bin)] * magnitudeToRms;

    const auto frameRms = std::sqrt(std::inner_product(analysisFifo.begin(), analysisFifo.end(),
                                                       analysisFifo.begin(), 0.0f)
                                    / static_cast<float>(fftSize));
    const auto frameDb = gainToDb(frameRms);
    auto automaticFundamental = learnedFundamentalHz;
    const auto firstCrest = frameDb > -60.0f ? firstVoiceCrestHz() : 0.0f;

    if (firstCrest > 0.0f && automaticFundamental > 0.0f)
    {
        if (std::abs(std::log2(automaticFundamental / firstCrest)) < 0.18f)
            automaticFundamental = 0.75f * automaticFundamental + 0.25f * firstCrest;
        else if (std::abs(std::log2(automaticFundamental / (2.0f * firstCrest))) < 0.16f)
            automaticFundamental = firstCrest;
    }

    const auto desiredFundamental = currentSettings.manualFundamentalEnabled
                                        ? std::clamp(currentSettings.manualFundamentalFrequencyHz, 100.0f, 600.0f)
                                        : automaticFundamental;

    if (desiredFundamental > 0.0f)
    {
        const auto changedSubstantially = activeFundamentalHz <= 0.0f
                                          || std::abs(std::log2(desiredFundamental / activeFundamentalHz)) > 0.08f;
        activeFundamentalHz = desiredFundamental;

        if (changedSubstantially)
        {
            fundamentalPeakRmsDb = minimumDb;
            fundamentalCorrectionDb = 0.0f;
            appliedFundamentalHz = -1.0f;
        }

        if (frameDb > -60.0f)
            fundamentalPeakRmsDb = std::max(fundamentalPeakRmsDb,
                                            gainToDb(rmsNearFrequency(activeFundamentalHz, 2)));
    }

    const auto highBandDb = gainToDb(bandRms(3500.0f, 10000.0f));
    const auto midBandDb = gainToDb(bandRms(500.0f, 3000.0f));

    if (highBandDb > -72.0f && highBandDb > midBandDb - 12.0f)
    {
        const auto candidateHz = peakFrequency(3500.0f, 10000.0f);
        const auto candidateDb = gainToDb(rmsNearFrequency(candidateHz, 4));

        if (candidateDb > sibilancePeakRmsDb)
        {
            sibilancePeakRmsDb = candidateDb;
            learnedSibilanceHz = candidateHz;
        }
    }

    if (fundamentalPeakRmsDb > -90.0f && sibilancePeakRmsDb > -90.0f)
    {
        const auto commonLevel = std::min({ bandCeilingDb, fundamentalPeakRmsDb, sibilancePeakRmsDb });
        const auto desiredFundamentalCorrection = std::clamp(
            commonLevel - fundamentalPeakRmsDb, -72.0f, 0.0f);
        const auto desiredSibilance = std::clamp(commonLevel - sibilancePeakRmsDb, -72.0f, 0.0f);
        fundamentalCorrectionDb += 0.2f * (desiredFundamentalCorrection - fundamentalCorrectionDb);
        sibilanceCorrectionDb += 0.2f * (desiredSibilance - sibilanceCorrectionDb);
    }

}

void VoiceEngineer::pushDisplaySamples(const juce::AudioBuffer<float>& buffer)
{
    const auto channels = buffer.getNumChannels();

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto mono = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
            mono += buffer.getSample(channel, sample);
        displayFifo[static_cast<size_t>(displayFifoPosition++)] = mono / static_cast<float>(channels);

        if (displayFifoPosition == fftSize)
        {
            analyseDisplayFrame();
            displayFifoPosition = 0;
        }
    }
}

void VoiceEngineer::analyseDisplayFrame()
{
    for (int index = 0; index < fftSize; ++index)
        displayFftData[static_cast<size_t>(index)] = displayFifo[static_cast<size_t>(index)] * window[static_cast<size_t>(index)];
    std::fill(displayFftData.begin() + fftSize, displayFftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform(displayFftData.data(), true);

    const auto magnitudeToRms = std::sqrt(2.0f) / windowSum;
    for (int bin = 0; bin <= fftSize / 2; ++bin)
        displayBinRms[static_cast<size_t>(bin)] = displayFftData[static_cast<size_t>(bin)] * magnitudeToRms;

    for (int point = 0; point < spectrumPointCount; ++point)
    {
        const auto ratio = static_cast<float>(point) / static_cast<float>(spectrumPointCount - 1);
        const auto frequency = 30.0f * std::pow(20000.0f / 30.0f, ratio);
        const auto centre = static_cast<int>(std::round(frequency * static_cast<float>(fftSize)
                                                        / static_cast<float>(currentSampleRate)));
        const auto first = std::max(1, centre - 1);
        const auto last = std::min(fftSize / 2, centre + 1);
        auto power = 0.0f;
        for (int bin = first; bin <= last; ++bin)
        {
            const auto rms = displayBinRms[static_cast<size_t>(bin)];
            power += rms * rms;
        }
        const auto rawDb = gainToDb(std::sqrt(power));
        const auto tiltedDb = rawDb + displayTiltDbPerOctave * std::log2(frequency / 1000.0f);
        publishedFrequencies[static_cast<size_t>(point)].store(frequency, std::memory_order_relaxed);
        publishedSpectrum[static_cast<size_t>(point)].store(std::clamp(tiltedDb, minimumDb, 12.0f),
                                                            std::memory_order_relaxed);
    }
}

void VoiceEngineer::pushPitchSample(float sample)
{
    const auto highPassPole = std::exp(-2.0f * pi * 60.0f / static_cast<float>(currentSampleRate));
    const auto highPassed = sample - pitchPreviousInput + highPassPole * pitchHighPassOutput;
    pitchPreviousInput = sample;
    pitchHighPassOutput = highPassed;

    const auto lowPassCoefficient = 1.0f - std::exp(-2.0f * pi * 1800.0f
                                                    / static_cast<float>(currentSampleRate));
    pitchLowPassOne += lowPassCoefficient * (highPassed - pitchLowPassOne);
    pitchLowPassTwo += lowPassCoefficient * (pitchLowPassOne - pitchLowPassTwo);

    if (++pitchDecimationCounter < pitchDecimation)
        return;

    pitchDecimationCounter = 0;
    pitchFrame[static_cast<size_t>(pitchWritePosition)] = pitchLowPassTwo;
    pitchWritePosition = (pitchWritePosition + 1) % pitchFrameSize;
    pitchSamplesCollected = std::min(pitchSamplesCollected + 1, pitchFrameSize);
    ++pitchHopCounter;

    if (pitchSamplesCollected == pitchFrameSize && pitchHopCounter >= pitchHopSize)
    {
        pitchHopCounter = 0;
        analysePitchFrame();
    }
}

void VoiceEngineer::analysePitchFrame()
{
    const auto result = estimateFundamental();

    if (! result.voiced)
    {
        fundamentalConfidence *= 0.92f;
        return;
    }

    pitchHistory[static_cast<size_t>(pitchHistoryPosition)] = result.frequencyHz;
    pitchHistoryPosition = (pitchHistoryPosition + 1) % pitchHistorySize;
    pitchHistoryCount = std::min(pitchHistoryCount + 1, pitchHistorySize);

    auto sorted = pitchHistory;
    std::sort(sorted.begin(), sorted.begin() + pitchHistoryCount);
    const auto median = sorted[static_cast<size_t>(pitchHistoryCount / 2)];

    if (learnedFundamentalHz <= 0.0f)
        learnedFundamentalHz = median;
    else
        learnedFundamentalHz += 0.22f * (median - learnedFundamentalHz);

    learnedFundamentalHz = std::clamp(learnedFundamentalHz, 100.0f, 600.0f);
    fundamentalConfidence += 0.25f * (result.confidence - fundamentalConfidence);
}

VoiceEngineer::PitchResult VoiceEngineer::estimateFundamental() const
{
    std::array<float, maximumPitchFrameSize> frame {};
    auto mean = 0.0f;

    for (int index = 0; index < pitchFrameSize; ++index)
    {
        const auto source = (pitchWritePosition + index) % pitchFrameSize;
        frame[static_cast<size_t>(index)] = pitchFrame[static_cast<size_t>(source)];
        mean += frame[static_cast<size_t>(index)];
    }

    mean /= static_cast<float>(pitchFrameSize);
    auto framePower = 0.0f;
    for (int index = 0; index < pitchFrameSize; ++index)
    {
        frame[static_cast<size_t>(index)] -= mean;
        framePower += frame[static_cast<size_t>(index)] * frame[static_cast<size_t>(index)];
    }

    if (gainToDb(std::sqrt(framePower / static_cast<float>(pitchFrameSize))) < -60.0f)
        return {};

    const auto minimumLag = std::max(2, static_cast<int>(std::floor(pitchSampleRate / 600.0f)));
    const auto maximumLag = std::min(pitchFrameSize / 2,
                                     static_cast<int>(std::ceil(pitchSampleRate / 100.0f)));
    const auto comparisonLength = pitchFrameSize - maximumLag;
    std::array<float, maximumPitchFrameSize / 2 + 1> difference {};
    std::array<float, maximumPitchFrameSize / 2 + 1> normalizedDifference {};

    for (int lag = 1; lag <= maximumLag; ++lag)
    {
        auto sum = 0.0f;
        for (int index = 0; index < comparisonLength; ++index)
        {
            const auto delta = frame[static_cast<size_t>(index)]
                               - frame[static_cast<size_t>(index + lag)];
            sum += delta * delta;
        }
        difference[static_cast<size_t>(lag)] = sum;
    }

    normalizedDifference[0] = 1.0f;
    auto cumulative = 0.0f;
    for (int lag = 1; lag <= maximumLag; ++lag)
    {
        cumulative += difference[static_cast<size_t>(lag)];
        normalizedDifference[static_cast<size_t>(lag)] = cumulative > 1.0e-12f
                                                              ? difference[static_cast<size_t>(lag)]
                                                                    * static_cast<float>(lag) / cumulative
                                                              : 1.0f;
    }

    auto selectedLag = 0;
    constexpr auto yinThreshold = 0.15f;

    for (int lag = minimumLag; lag < maximumLag; ++lag)
    {
        if (normalizedDifference[static_cast<size_t>(lag)] >= yinThreshold)
            continue;

        selectedLag = lag;
        while (selectedLag + 1 <= maximumLag
               && normalizedDifference[static_cast<size_t>(selectedLag + 1)]
                      < normalizedDifference[static_cast<size_t>(selectedLag)])
            ++selectedLag;
        break;
    }

    if (selectedLag == 0)
    {
        selectedLag = minimumLag;
        for (int lag = minimumLag + 1; lag <= maximumLag; ++lag)
            if (normalizedDifference[static_cast<size_t>(lag)]
                < normalizedDifference[static_cast<size_t>(selectedLag)])
                selectedLag = lag;
    }

    const auto confidence = 1.0f - normalizedDifference[static_cast<size_t>(selectedLag)];
    if (confidence < 0.78f)
        return { 0.0f, confidence, false };

    auto offset = 0.0f;
    if (selectedLag > minimumLag && selectedLag < maximumLag)
    {
        const auto previous = normalizedDifference[static_cast<size_t>(selectedLag - 1)];
        const auto current = normalizedDifference[static_cast<size_t>(selectedLag)];
        const auto next = normalizedDifference[static_cast<size_t>(selectedLag + 1)];
        const auto denominator = previous - 2.0f * current + next;
        if (std::abs(denominator) > 1.0e-8f)
            offset = std::clamp(0.5f * (previous - next) / denominator, -0.5f, 0.5f);
    }
    const auto refinedLag = static_cast<float>(selectedLag) + offset;
    const auto frequency = std::clamp(pitchSampleRate / refinedLag, 100.0f, 600.0f);
    return { frequency, confidence, true };
}

float VoiceEngineer::firstVoiceCrestHz() const
{
    const auto firstBin = std::max(2, static_cast<int>(std::ceil(100.0f * static_cast<float>(fftSize)
                                                                 / static_cast<float>(currentSampleRate))));
    const auto lastBin = std::min(fftSize / 2 - 1,
                                  static_cast<int>(std::floor(600.0f * static_cast<float>(fftSize)
                                                               / static_cast<float>(currentSampleRate))));
    auto maximum = 0.0f;
    for (int bin = firstBin; bin <= lastBin; ++bin)
        maximum = std::max(maximum, binRms[static_cast<size_t>(bin)]);

    const auto threshold = maximum * 0.1f;
    for (int bin = firstBin; bin <= lastBin; ++bin)
    {
        const auto value = binRms[static_cast<size_t>(bin)];
        if (value < threshold
            || value < binRms[static_cast<size_t>(bin - 1)]
            || value < binRms[static_cast<size_t>(bin + 1)])
            continue;

        const auto previous = binRms[static_cast<size_t>(bin - 1)];
        const auto next = binRms[static_cast<size_t>(bin + 1)];
        const auto denominator = previous - 2.0f * value + next;
        const auto offset = std::abs(denominator) > 1.0e-8f
                                ? std::clamp(0.5f * (previous - next) / denominator, -0.5f, 0.5f)
                                : 0.0f;
        return (static_cast<float>(bin) + offset) * static_cast<float>(currentSampleRate)
               / static_cast<float>(fftSize);
    }

    return 0.0f;
}

float VoiceEngineer::rmsNearFrequency(float frequencyHz, int halfWidthBins) const
{
    if (frequencyHz <= 0.0f)
        return 0.0f;

    const auto centre = static_cast<int>(std::round(frequencyHz * static_cast<float>(fftSize)
                                                    / static_cast<float>(currentSampleRate)));
    const auto first = std::max(1, centre - halfWidthBins);
    const auto last = std::min(fftSize / 2, centre + halfWidthBins);
    auto power = 0.0f;

    for (int bin = first; bin <= last; ++bin)
    {
        const auto rms = binRms[static_cast<size_t>(bin)];
        power += rms * rms;
    }

    return std::sqrt(power);
}

float VoiceEngineer::bandRms(float lowHz, float highHz) const
{
    const auto first = std::max(1, static_cast<int>(std::ceil(lowHz * static_cast<float>(fftSize)
                                                              / static_cast<float>(currentSampleRate))));
    const auto last = std::min(fftSize / 2, static_cast<int>(std::floor(highHz * static_cast<float>(fftSize)
                                                                        / static_cast<float>(currentSampleRate))));
    auto power = 0.0f;

    for (int bin = first; bin <= last; ++bin)
    {
        const auto rms = binRms[static_cast<size_t>(bin)];
        power += rms * rms;
    }

    return std::sqrt(power);
}

float VoiceEngineer::peakFrequency(float lowHz, float highHz) const
{
    const auto first = std::max(1, static_cast<int>(std::ceil(lowHz * static_cast<float>(fftSize)
                                                              / static_cast<float>(currentSampleRate))));
    const auto last = std::min(fftSize / 2, static_cast<int>(std::floor(highHz * static_cast<float>(fftSize)
                                                                        / static_cast<float>(currentSampleRate))));
    auto bestBin = first;

    for (int bin = first + 1; bin <= last; ++bin)
        if (binRms[static_cast<size_t>(bin)] > binRms[static_cast<size_t>(bestBin)])
            bestBin = bin;

    return static_cast<float>(bestBin) * static_cast<float>(currentSampleRate) / static_cast<float>(fftSize);
}

void VoiceEngineer::updateCorrectionFilters(int channels, bool enabled)
{
    const auto fundamentalGain = enabled ? fundamentalCorrectionDb : 0.0f;
    const auto sibilanceGain = enabled ? sibilanceCorrectionDb : 0.0f;

    if (activeFundamentalHz > 0.0f
        && (std::abs(activeFundamentalHz - appliedFundamentalHz) > 0.1f
            || std::abs(fundamentalGain - appliedFundamentalGainDb) > 0.02f))
    {
        const auto coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
            currentSampleRate, activeFundamentalHz, 1.25f, juce::Decibels::decibelsToGain(fundamentalGain));
        for (int channel = 0; channel < std::min(channels, currentChannels); ++channel)
            *fundamentalEq[static_cast<size_t>(channel)].coefficients = coefficients;
        appliedFundamentalHz = activeFundamentalHz;
        appliedFundamentalGainDb = fundamentalGain;
    }

    if (learnedSibilanceHz > 0.0f
        && (std::abs(learnedSibilanceHz - appliedSibilanceHz) > 0.1f
            || std::abs(sibilanceGain - appliedSibilanceGainDb) > 0.02f))
    {
        const auto coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
            currentSampleRate, learnedSibilanceHz, 2.0f, juce::Decibels::decibelsToGain(sibilanceGain));
        for (int channel = 0; channel < std::min(channels, currentChannels); ++channel)
            *sibilanceEq[static_cast<size_t>(channel)].coefficients = coefficients;
        appliedSibilanceHz = learnedSibilanceHz;
        appliedSibilanceGainDb = sibilanceGain;
    }
}

void VoiceEngineer::applyCorrections(juce::AudioBuffer<float>& buffer, const Settings&)
{
    for (int channel = 0; channel < std::min(buffer.getNumChannels(), currentChannels); ++channel)
    {
        auto* data = buffer.getWritePointer(channel);
        auto& low = fundamentalEq[static_cast<size_t>(channel)];
        auto& high = sibilanceEq[static_cast<size_t>(channel)];

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            data[sample] = high.processSample(low.processSample(data[sample]));
    }
}

void VoiceEngineer::updateShelfFilters(const Settings& settings, int channels)
{
    if (shelfSettingsValid && ! settingsDiffer(settings, previousShelfSettings))
        return;

    previousShelfSettings = settings;
    shelfSettingsValid = true;
    const auto lowReduction = settings.lowShelfEnabled ? std::clamp(settings.lowShelfReductionDb, 0.0f, 12.0f) : 0.0f;
    const auto highReduction = settings.highShelfEnabled ? std::clamp(settings.highShelfReductionDb, 0.0f, 12.0f) : 0.0f;
    const auto lowStages = settings.lowShelfSlope == 1 ? 2 : 1;
    const auto highStages = settings.highShelfSlope == 1 ? 2 : 1;

    for (int stage = 0; stage < 2; ++stage)
    {
        const auto lowGainDb = stage < lowStages ? -lowReduction / static_cast<float>(lowStages) : 0.0f;
        const auto highGainDb = stage < highStages ? -highReduction / static_cast<float>(highStages) : 0.0f;
        const auto lowCoefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf(
            currentSampleRate,
            std::clamp(settings.lowShelfFrequencyHz, 20.0f, 500.0f),
            0.7071f,
            juce::Decibels::decibelsToGain(lowGainDb));
        const auto highCoefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf(
            currentSampleRate,
            std::clamp(settings.highShelfFrequencyHz,
                       2000.0f,
                       std::min(20000.0f, static_cast<float>(currentSampleRate * 0.49))),
            0.7071f,
            juce::Decibels::decibelsToGain(highGainDb));

        for (int channel = 0; channel < std::min(channels, currentChannels); ++channel)
        {
            *lowShelf[static_cast<size_t>(stage)][static_cast<size_t>(channel)].coefficients = lowCoefficients;
            *highShelf[static_cast<size_t>(stage)][static_cast<size_t>(channel)].coefficients = highCoefficients;
        }
    }
}

void VoiceEngineer::applyShelves(juce::AudioBuffer<float>& buffer, const Settings& settings)
{
    const auto lowStages = settings.lowShelfSlope == 1 ? 2 : 1;
    const auto highStages = settings.highShelfSlope == 1 ? 2 : 1;

    for (int channel = 0; channel < std::min(buffer.getNumChannels(), currentChannels); ++channel)
    {
        auto* data = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            auto value = data[sample];
            for (int stage = 0; stage < lowStages; ++stage)
                value = lowShelf[static_cast<size_t>(stage)][static_cast<size_t>(channel)].processSample(value);
            for (int stage = 0; stage < highStages; ++stage)
                value = highShelf[static_cast<size_t>(stage)][static_cast<size_t>(channel)].processSample(value);
            data[sample] = value;
        }
    }
}

void VoiceEngineer::applyLimiter(juce::AudioBuffer<float>& buffer, bool limitingEnabled)
{
    const auto channels = std::min(buffer.getNumChannels(), currentChannels);
    const auto release = std::exp(-1.0f / static_cast<float>(0.08 * currentSampleRate));

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto peak = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
            peak = std::max(peak, std::abs(buffer.getSample(channel, sample)));

        if (limitingEnabled)
        {
            const auto requiredGain = peak > 1.0f ? 1.0f / peak : 1.0f;

            if (requiredGain < limiterGain)
            {
                limiterGain = requiredGain;
                limiterHoldRemaining = limiterDelaySamples;
            }
            else if (limiterHoldRemaining > 0)
            {
                --limiterHoldRemaining;
            }
            else
            {
                limiterGain = requiredGain + release * (limiterGain - requiredGain);
            }
        }
        else
        {
            limiterGain = 1.0f;
            limiterHoldRemaining = 0;
        }

        for (int channel = 0; channel < channels; ++channel)
        {
            auto& delay = limiterDelay[static_cast<size_t>(channel)];
            const auto input = buffer.getSample(channel, sample);
            const auto delayed = delay[static_cast<size_t>(limiterWritePosition)];
            delay[static_cast<size_t>(limiterWritePosition)] = input;
            buffer.setSample(channel, sample, std::clamp(delayed * limiterGain, -1.0f, 1.0f));
        }

        limiterWritePosition = (limiterWritePosition + 1) % limiterDelaySamples;
    }

    limiterReductionDb = limitingEnabled ? std::min(0.0f, gainToDb(limiterGain)) : 0.0f;
}

void VoiceEngineer::publishAnalysis()
{
    publishedLufs.store(measuredLufs, std::memory_order_relaxed);
    publishedNormalizerGainDb.store(smoothedNormalizerGainDb, std::memory_order_relaxed);
    publishedFundamentalHz.store(activeFundamentalHz, std::memory_order_relaxed);
    publishedFundamentalConfidence.store(currentSettings.manualFundamentalEnabled ? 1.0f : fundamentalConfidence,
                                         std::memory_order_relaxed);
    publishedFundamentalIsManual.store(currentSettings.manualFundamentalEnabled, std::memory_order_relaxed);
    publishedFundamentalRmsDb.store(fundamentalPeakRmsDb, std::memory_order_relaxed);
    publishedSibilanceHz.store(learnedSibilanceHz, std::memory_order_relaxed);
    publishedSibilanceRmsDb.store(sibilancePeakRmsDb, std::memory_order_relaxed);
    publishedFundamentalCorrectionDb.store(fundamentalCorrectionDb, std::memory_order_relaxed);
    publishedSibilanceCorrectionDb.store(sibilanceCorrectionDb, std::memory_order_relaxed);
    publishedLimiterReductionDb.store(limiterReductionDb, std::memory_order_relaxed);
    publishedSignalPresent.store(measuredLufs > -70.0f, std::memory_order_relaxed);
}

VoiceEngineer::AnalysisSnapshot VoiceEngineer::getAnalysisSnapshot() const
{
    AnalysisSnapshot snapshot;
    for (int point = 0; point < spectrumPointCount; ++point)
    {
        snapshot.frequenciesHz[static_cast<size_t>(point)] = publishedFrequencies[static_cast<size_t>(point)].load(std::memory_order_relaxed);
        snapshot.spectrumDb[static_cast<size_t>(point)] = publishedSpectrum[static_cast<size_t>(point)].load(std::memory_order_relaxed);
    }
    snapshot.loudnessLufs = publishedLufs.load(std::memory_order_relaxed);
    snapshot.normalizerGainDb = publishedNormalizerGainDb.load(std::memory_order_relaxed);
    snapshot.fundamentalHz = publishedFundamentalHz.load(std::memory_order_relaxed);
    snapshot.fundamentalConfidence = publishedFundamentalConfidence.load(std::memory_order_relaxed);
    snapshot.fundamentalIsManual = publishedFundamentalIsManual.load(std::memory_order_relaxed);
    snapshot.fundamentalRmsDb = publishedFundamentalRmsDb.load(std::memory_order_relaxed);
    snapshot.sibilanceHz = publishedSibilanceHz.load(std::memory_order_relaxed);
    snapshot.sibilanceRmsDb = publishedSibilanceRmsDb.load(std::memory_order_relaxed);
    snapshot.fundamentalCorrectionDb = publishedFundamentalCorrectionDb.load(std::memory_order_relaxed);
    snapshot.sibilanceCorrectionDb = publishedSibilanceCorrectionDb.load(std::memory_order_relaxed);
    snapshot.limiterReductionDb = publishedLimiterReductionDb.load(std::memory_order_relaxed);
    snapshot.signalPresent = publishedSignalPresent.load(std::memory_order_relaxed);
    return snapshot;
}
}
