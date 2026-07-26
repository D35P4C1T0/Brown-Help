#include "BrownHelpProcessor.h"

#include "BrownHelpEditor.h"

namespace BrownHelp
{
namespace
{
template <typename T>
T parameterValue(const juce::AudioProcessorValueTreeState& parameters, const char* id)
{
    return static_cast<T>(parameters.getRawParameterValue(id)->load());
}

float gentleSaturate(float sample, float drive)
{
    if (drive <= 1.0001f)
        return sample;

    // Unity small-signal gain avoids turning Drive into an accidental high shelf.
    return std::tanh(sample * drive) / drive;
}
}

BrownHelpProcessor::BrownHelpProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::mono(), true)
                         .withOutput("Output", juce::AudioChannelSet::mono(), true)),
      parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

void BrownHelpProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    hostSampleRate = sampleRate;
    dspSampleRate = sampleRate;
    currentOversamplingChoice = -1;
    previousHighPassFrequency = -1.0f;
    previousHighPassSlope = -1;
    smoothedAutoGainDb = 0.0f;
    const auto channels = std::max(1, getTotalNumOutputChannels());
    balancer.prepare(sampleRate, samplesPerBlock, channels);
    prepareHighPass(sampleRate, channels);
    prepareSaturationPaths(sampleRate, samplesPerBlock, channels);
    updateOversampling(parameterValue<int>(parameters, oversamplingId));
}

void BrownHelpProcessor::releaseResources()
{
    balancer.reset();

    for (auto& path : saturationPaths)
    {
        if (path.oversampling != nullptr)
            path.oversampling->reset();

        for (auto& filter : path.highPass)
            filter.reset();
    }
}

bool BrownHelpProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

void BrownHelpProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    const auto oversamplingChoice = parameterValue<int>(parameters, oversamplingId);
    updateOversampling(oversamplingChoice);

    if (parameterValue<float>(parameters, bypassId) > 0.5f)
    {
        // Preserve the declared oversampling latency so the plugin's own bypass
        // stays sample-aligned with the active path.
        processSaturation(buffer, false);
        return;
    }

    const auto inputRms = calculateRms(buffer);
    processHighPass(buffer);
    balancer.process(buffer, readSettings());
    processSaturation(buffer, parameterValue<float>(parameters, saturationEnabledId) >= 0.5f);
    applyAutoGainCompensation(buffer, inputRms);
    applyOutputGuard(buffer);
}

juce::AudioProcessorEditor* BrownHelpProcessor::createEditor()
{
    return new BrownHelpEditor(*this);
}

bool BrownHelpProcessor::hasEditor() const
{
    return true;
}

const juce::String BrownHelpProcessor::getName() const
{
    return JucePlugin_Name;
}

bool BrownHelpProcessor::acceptsMidi() const
{
    return false;
}

bool BrownHelpProcessor::producesMidi() const
{
    return false;
}

bool BrownHelpProcessor::isMidiEffect() const
{
    return false;
}

double BrownHelpProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int BrownHelpProcessor::getNumPrograms()
{
    return 1;
}

int BrownHelpProcessor::getCurrentProgram()
{
    return 0;
}

void BrownHelpProcessor::setCurrentProgram(int)
{
}

const juce::String BrownHelpProcessor::getProgramName(int)
{
    return {};
}

void BrownHelpProcessor::changeProgramName(int, const juce::String&)
{
}

void BrownHelpProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, false);
    parameters.copyState().writeToStream(stream);
}

void BrownHelpProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes)); tree.isValid())
        parameters.replaceState(tree);
}

juce::AudioProcessorValueTreeState& BrownHelpProcessor::getParameters()
{
    return parameters;
}

BrownCurveBalancer::AnalysisSnapshot BrownHelpProcessor::getAnalysisSnapshot() const
{
    return balancer.getAnalysisSnapshot();
}

juce::AudioProcessorValueTreeState::ParameterLayout BrownHelpProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;

    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        curveId,
        "Curve",
        juce::StringArray { "Gentle Brown", "Brown", "Dark Brown" },
        1));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        tiltId,
        "Tilt",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        75.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.push_back(std::make_unique<juce::AudioParameterBool>(
        tiltFlipId,
        "Flip Tilt",
        false));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        strengthId,
        "Strength",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.35f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        mixId,
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        lowFrequencyId,
        "Low Frequency",
        juce::NormalisableRange<float>(20.0f, 1000.0f, 0.1f, 0.35f),
        20.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        highFrequencyId,
        "High Frequency",
        juce::NormalisableRange<float>(1000.0f, 20000.0f, 1.0f, 0.35f),
        20000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        maxCorrectionId,
        "Max Correction",
        juce::NormalisableRange<float>(1.0f, 18.0f, 0.1f),
        6.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        speedId,
        "Speed",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.45f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        oversamplingId,
        "Oversampling",
        juce::StringArray { "1x", "2x", "4x" },
        0));

    layout.push_back(std::make_unique<juce::AudioParameterBool>(
        highPassEnabledId,
        "High Pass",
        false));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        highPassFrequencyId,
        "HP Frequency",
        juce::NormalisableRange<float>(20.0f, 400.0f, 0.1f, 0.45f),
        80.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        highPassSlopeId,
        "HP Slope",
        juce::StringArray { "12 dB/oct", "24 dB/oct" },
        0));

    layout.push_back(std::make_unique<juce::AudioParameterBool>(
        saturationEnabledId,
        "High Saturation",
        false));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        saturationFrequencyId,
        "Sat Frequency",
        juce::NormalisableRange<float>(1500.0f, 12000.0f, 1.0f, 0.35f),
        4500.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        saturationDriveId,
        "Sat Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.10f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        saturationMixId,
        "Sat Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.12f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.push_back(std::make_unique<juce::AudioParameterBool>(bypassId, "Bypass", false));

    return { layout.begin(), layout.end() };
}

BrownCurveBalancer::Settings BrownHelpProcessor::readSettings() const
{
    BrownCurveBalancer::Settings settings;

    const auto curveChoice = parameterValue<int>(parameters, curveId);
    settings.tiltDbPerOctave = tiltControlToDbPerOctave(
        parameterValue<float>(parameters, tiltId),
        curveChoice,
        parameterValue<float>(parameters, tiltFlipId) >= 0.5f);
    settings.strength = parameterValue<float>(parameters, strengthId);
    settings.mix = parameterValue<float>(parameters, mixId);
    settings.lowFrequencyHz = parameterValue<float>(parameters, lowFrequencyId);
    settings.highFrequencyHz = parameterValue<float>(parameters, highFrequencyId);
    settings.maxCorrectionDb = parameterValue<float>(parameters, maxCorrectionId);
    settings.speed = parameterValue<float>(parameters, speedId);

    return settings;
}

void BrownHelpProcessor::updateOversampling(int oversamplingChoice)
{
    const auto choice = std::clamp(oversamplingChoice, 0, 2);

    if (choice == currentOversamplingChoice)
        return;

    currentOversamplingChoice = choice;
    auto& path = saturationPaths[static_cast<size_t>(choice)];

    if (path.oversampling != nullptr)
        path.oversampling->reset();

    for (auto& filter : path.highPass)
        filter.reset();

    path.previousFrequency = -1.0f;
    const auto latency = path.oversampling != nullptr
                             ? static_cast<int>(std::round(path.oversampling->getLatencyInSamples()))
                             : 0;
    setLatencySamples(latency);
}

void BrownHelpProcessor::prepareHighPass(double sampleRate, int channels)
{
    dspSampleRate = sampleRate;
    const auto safeChannels = std::max(1, channels);
    highPassStageOne.resize(static_cast<size_t>(safeChannels));
    highPassStageTwo.resize(static_cast<size_t>(safeChannels));

    for (auto& filter : highPassStageOne)
        filter.reset();

    for (auto& filter : highPassStageTwo)
        filter.reset();

    previousHighPassFrequency = -1.0f;
    previousHighPassSlope = -1;
    updateHighPass();
}

void BrownHelpProcessor::updateHighPass()
{
    const auto frequency = std::clamp(parameterValue<float>(parameters, highPassFrequencyId),
                                      20.0f,
                                      static_cast<float>(dspSampleRate * 0.45));
    const auto slope = parameterValue<int>(parameters, highPassSlopeId);

    if (std::abs(frequency - previousHighPassFrequency) < 0.01f && slope == previousHighPassSlope)
        return;

    previousHighPassFrequency = frequency;
    previousHighPassSlope = slope;

    const auto coefficients =
        juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(dspSampleRate, frequency, 0.70710678f);

    for (auto& filter : highPassStageOne)
        *filter.coefficients = coefficients;

    for (auto& filter : highPassStageTwo)
        *filter.coefficients = coefficients;
}

void BrownHelpProcessor::processHighPass(juce::AudioBuffer<float>& buffer)
{
    if (parameterValue<float>(parameters, highPassEnabledId) < 0.5f)
        return;

    updateHighPass();

    const auto channels = std::min(buffer.getNumChannels(), static_cast<int>(highPassStageOne.size()));
    const auto samples = buffer.getNumSamples();
    const auto useSecondStage = parameterValue<int>(parameters, highPassSlopeId) == 1;

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);
        auto& stageOne = highPassStageOne[static_cast<size_t>(channel)];
        auto& stageTwo = highPassStageTwo[static_cast<size_t>(channel)];

        for (int sample = 0; sample < samples; ++sample)
        {
            auto value = stageOne.processSample(data[sample]);

            if (useSecondStage)
                value = stageTwo.processSample(value);

            data[sample] = value;
        }
    }
}

void BrownHelpProcessor::prepareSaturationPaths(double sampleRate, int maximumBlockSize, int channels)
{
    const auto safeChannels = std::max(1, channels);
    oversampledChannelPointers.clear();
    oversampledChannelPointers.reserve(static_cast<size_t>(safeChannels));

    for (int choice = 0; choice < static_cast<int>(saturationPaths.size()); ++choice)
    {
        auto& path = saturationPaths[static_cast<size_t>(choice)];
        const auto factor = 1 << choice;
        path.sampleRate = sampleRate * static_cast<double>(factor);
        path.previousFrequency = -1.0f;
        path.highPass.resize(static_cast<size_t>(safeChannels));

        for (auto& filter : path.highPass)
            filter.reset();

        if (choice == 0)
        {
            path.oversampling.reset();
            continue;
        }

        path.oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
            static_cast<size_t>(safeChannels),
            static_cast<size_t>(choice),
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true,
            true);
        path.oversampling->initProcessing(static_cast<size_t>(std::max(1, maximumBlockSize)));
        path.oversampling->reset();
    }
}

void BrownHelpProcessor::updateSaturation(SaturationPath& path)
{
    const auto frequency = std::clamp(parameterValue<float>(parameters, saturationFrequencyId),
                                      1500.0f,
                                      static_cast<float>(path.sampleRate * 0.45));

    if (std::abs(frequency - path.previousFrequency) < 0.01f)
        return;

    path.previousFrequency = frequency;

    const auto coefficients =
        juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(path.sampleRate, frequency, 0.70710678f);

    for (auto& filter : path.highPass)
        *filter.coefficients = coefficients;
}

void BrownHelpProcessor::processSaturation(juce::AudioBuffer<float>& buffer, bool enabled)
{
    auto& path = saturationPaths[static_cast<size_t>(std::clamp(currentOversamplingChoice, 0, 2))];

    if (path.oversampling == nullptr)
    {
        if (enabled)
            processSaturationSamples(buffer, path);

        return;
    }

    juce::dsp::AudioBlock<float> block(buffer);
    auto oversampledBlock = path.oversampling->processSamplesUp(block);

    oversampledChannelPointers.clear();

    for (size_t channel = 0; channel < oversampledBlock.getNumChannels(); ++channel)
        oversampledChannelPointers.push_back(oversampledBlock.getChannelPointer(channel));

    juce::AudioBuffer<float> oversampledBuffer(
        oversampledChannelPointers.data(),
        static_cast<int>(oversampledBlock.getNumChannels()),
        static_cast<int>(oversampledBlock.getNumSamples()));

    if (enabled)
        processSaturationSamples(oversampledBuffer, path);

    path.oversampling->processSamplesDown(block);
}

void BrownHelpProcessor::processSaturationSamples(juce::AudioBuffer<float>& buffer, SaturationPath& path)
{
    updateSaturation(path);

    const auto channels = std::min(buffer.getNumChannels(), static_cast<int>(path.highPass.size()));
    const auto samples = buffer.getNumSamples();
    const auto driveAmount = parameterValue<float>(parameters, saturationDriveId);
    const auto drive = juce::jmap(std::clamp(driveAmount, 0.0f, 1.0f), 1.0f, 6.0f);
    const auto mix = std::clamp(parameterValue<float>(parameters, saturationMixId), 0.0f, 1.0f);

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);
        auto& highPass = path.highPass[static_cast<size_t>(channel)];

        for (int sample = 0; sample < samples; ++sample)
        {
            const auto highBand = highPass.processSample(data[sample]);
            const auto saturatedHighBand = gentleSaturate(highBand, drive);
            data[sample] += (saturatedHighBand - highBand) * mix;
        }
    }
}

float BrownHelpProcessor::calculateRms(const juce::AudioBuffer<float>& buffer) const
{
    auto sumSquares = 0.0;
    auto count = 0;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* data = buffer.getReadPointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = static_cast<double>(data[sample]);
            sumSquares += value * value;
        }

        count += buffer.getNumSamples();
    }

    if (count == 0)
        return 0.0f;

    return static_cast<float>(std::sqrt(sumSquares / static_cast<double>(count)));
}

void BrownHelpProcessor::applyAutoGainCompensation(juce::AudioBuffer<float>& buffer, float inputRms)
{
    constexpr auto minimumRms = 0.001f;

    const auto outputRms = calculateRms(buffer);
    const auto previousGain = juce::Decibels::decibelsToGain(smoothedAutoGainDb);
    auto desiredDb = 0.0f;

    if (inputRms >= minimumRms && outputRms >= minimumRms)
    {
        const auto inputDb = juce::Decibels::gainToDecibels(inputRms);
        const auto outputDb = juce::Decibels::gainToDecibels(outputRms);
        desiredDb = std::clamp((inputDb - outputDb) * 0.75f, -3.0f, 4.0f);
    }

    const auto smoothing = std::exp(-static_cast<float>(buffer.getNumSamples())
                                    / static_cast<float>(0.45 * hostSampleRate));
    smoothedAutoGainDb = smoothing * smoothedAutoGainDb + (1.0f - smoothing) * desiredDb;
    const auto nextGain = juce::Decibels::decibelsToGain(smoothedAutoGainDb);
    buffer.applyGainRamp(0, buffer.getNumSamples(), previousGain, nextGain);
}

void BrownHelpProcessor::applyOutputGuard(juce::AudioBuffer<float>& buffer) const
{
    constexpr auto ceiling = 0.98f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            data[sample] = std::clamp(data[sample], -ceiling, ceiling);
    }
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BrownHelp::BrownHelpProcessor();
}
