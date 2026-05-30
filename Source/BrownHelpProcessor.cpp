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
    // Tiny bias adds subtle even harmonics; tanh keeps podcast saturation soft.
    const auto bias = 0.02f;
    const auto normaliser = std::tanh(drive + bias);
    const auto shaped = std::tanh(sample * drive + bias) - std::tanh(bias);
    return shaped / std::max(0.1f, normaliser);
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
    currentBlockSize = samplesPerBlock;
    currentOversamplingChoice = -1;
    previousHighPassFrequency = -1.0f;
    previousHighPassSlope = -1;
    previousSaturationFrequency = -1.0f;
    smoothedAutoGainDb = 0.0f;
    prepareHighPass(sampleRate, getTotalNumOutputChannels());
    prepareSaturation(sampleRate, getTotalNumOutputChannels());
    updateOversampling(parameterValue<int>(parameters, oversamplingId));
}

void BrownHelpProcessor::releaseResources()
{
    balancer.reset();
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

    if (parameterValue<float>(parameters, bypassId) > 0.5f)
        return;

    const auto inputRms = calculateRms(buffer);
    const auto oversamplingChoice = parameterValue<int>(parameters, oversamplingId);
    updateOversampling(oversamplingChoice);

    if (oversamplingChoice == 0 || oversampling == nullptr)
    {
        processHighPass(buffer);
        balancer.process(buffer, readSettings());
        processSaturation(buffer);
        applyAutoGainCompensation(buffer, inputRms);
        applyOutputGuard(buffer);
        return;
    }

    juce::dsp::AudioBlock<float> block(buffer);
    auto oversampledBlock = oversampling->processSamplesUp(block);

    oversampledChannelPointers.clear();
    oversampledChannelPointers.reserve(oversampledBlock.getNumChannels());

    for (size_t channel = 0; channel < oversampledBlock.getNumChannels(); ++channel)
        oversampledChannelPointers.push_back(oversampledBlock.getChannelPointer(channel));

    juce::AudioBuffer<float> oversampledBuffer(
        oversampledChannelPointers.data(),
        static_cast<int>(oversampledBlock.getNumChannels()),
        static_cast<int>(oversampledBlock.getNumSamples()));

    processHighPass(oversampledBuffer);
    balancer.process(oversampledBuffer, readSettings());
    processSaturation(oversampledBuffer);
    oversampling->processSamplesDown(block);
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
    parameters.state.writeToStream(stream);
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
        25.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.push_back(std::make_unique<juce::AudioParameterBool>(
        tiltFlipId,
        "Flip Tilt",
        false));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        strengthId,
        "Strength",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.15f,
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
        3.0f,
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
    settings.curve = static_cast<BrownCurveBalancer::Curve>(curveChoice);
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
    if (oversamplingChoice == currentOversamplingChoice)
        return;

    currentOversamplingChoice = oversamplingChoice;
    const auto channels = static_cast<size_t>(std::max(1, getTotalNumOutputChannels()));
    const auto factorPower = static_cast<size_t>(std::clamp(oversamplingChoice, 0, 2));

    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        channels,
        factorPower,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true,
        true);

    oversampling->initProcessing(static_cast<size_t>(std::max(1, currentBlockSize)));
    oversampling->reset();

    // Keep hostSampleRate immutable; derived DSP rate changes with oversampling.
    const auto oversampledRate = hostSampleRate * static_cast<double>(oversampling->getOversamplingFactor());
    const auto oversampledBlockSize = currentBlockSize * static_cast<int>(oversampling->getOversamplingFactor());
    balancer.prepare(oversampledRate, oversampledBlockSize, static_cast<int>(channels));
    prepareHighPass(oversampledRate, static_cast<int>(channels));
    prepareSaturation(oversampledRate, static_cast<int>(channels));
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

    auto coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(dspSampleRate, frequency, 0.70710678f);

    for (auto& filter : highPassStageOne)
        *filter.coefficients = *coefficients;

    for (auto& filter : highPassStageTwo)
        *filter.coefficients = *coefficients;
}

void BrownHelpProcessor::processHighPass(juce::AudioBuffer<float>& buffer)
{
    if (parameterValue<float>(parameters, highPassEnabledId) < 0.5f)
        return;

    updateHighPass();

    const auto channels = std::min(buffer.getNumChannels(), static_cast<int>(highPassStageOne.size()));
    const auto samples = buffer.getNumSamples();
    const auto useSecondStage = parameterValue<int>(parameters, highPassSlopeId) == 1;
    const auto cutoff = parameterValue<float>(parameters, highPassFrequencyId);
    const auto cutoffScale = std::clamp((cutoff - 20.0f) / 380.0f, 0.0f, 1.0f);
    const auto makeupDb = useSecondStage ? juce::jmap(cutoffScale, 0.4f, 1.4f)
                                         : juce::jmap(cutoffScale, 0.25f, 0.9f);
    const auto makeupGain = juce::Decibels::decibelsToGain(makeupDb);

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

            data[sample] = value * makeupGain;
        }
    }
}

void BrownHelpProcessor::prepareSaturation(double sampleRate, int channels)
{
    dspSampleRate = sampleRate;
    const auto safeChannels = std::max(1, channels);
    saturationHighPass.resize(static_cast<size_t>(safeChannels));

    for (auto& filter : saturationHighPass)
        filter.reset();

    previousSaturationFrequency = -1.0f;
    updateSaturation();
}

void BrownHelpProcessor::updateSaturation()
{
    const auto frequency = std::clamp(parameterValue<float>(parameters, saturationFrequencyId),
                                      1500.0f,
                                      static_cast<float>(dspSampleRate * 0.45));

    if (std::abs(frequency - previousSaturationFrequency) < 0.01f)
        return;

    previousSaturationFrequency = frequency;

    auto coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(dspSampleRate, frequency, 0.70710678f);

    for (auto& filter : saturationHighPass)
        *filter.coefficients = *coefficients;
}

void BrownHelpProcessor::processSaturation(juce::AudioBuffer<float>& buffer)
{
    if (parameterValue<float>(parameters, saturationEnabledId) < 0.5f)
        return;

    updateSaturation();

    const auto channels = std::min(buffer.getNumChannels(), static_cast<int>(saturationHighPass.size()));
    const auto samples = buffer.getNumSamples();
    const auto driveAmount = parameterValue<float>(parameters, saturationDriveId);
    const auto drive = juce::jmap(std::clamp(driveAmount, 0.0f, 1.0f), 1.01f, 2.1f);
    const auto mix = std::clamp(parameterValue<float>(parameters, saturationMixId), 0.0f, 1.0f) * 0.65f;

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);
        auto& highPass = saturationHighPass[static_cast<size_t>(channel)];

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

    if (inputRms < minimumRms || outputRms < minimumRms)
        return;

    const auto inputDb = juce::Decibels::gainToDecibels(inputRms);
    const auto outputDb = juce::Decibels::gainToDecibels(outputRms);
    const auto desiredDb = std::clamp((inputDb - outputDb) * 0.85f, -3.0f, 8.0f);
    const auto smoothing = 0.92f;
    smoothedAutoGainDb = smoothing * smoothedAutoGainDb + (1.0f - smoothing) * desiredDb;
    buffer.applyGain(juce::Decibels::decibelsToGain(smoothedAutoGainDb));
}

void BrownHelpProcessor::applyOutputGuard(juce::AudioBuffer<float>& buffer) const
{
    constexpr auto ceiling = 0.98f;
    auto peak = 0.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* data = buffer.getReadPointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            peak = std::max(peak, std::abs(data[sample]));
    }

    if (peak > ceiling)
        buffer.applyGain(ceiling / peak);
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BrownHelp::BrownHelpProcessor();
}
