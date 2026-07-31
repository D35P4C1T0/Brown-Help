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
}

BrownHelpProcessor::BrownHelpProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

void BrownHelpProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engineer.prepare(sampleRate, samplesPerBlock, std::max(1, getTotalNumOutputChannels()));
    setLatencySamples(engineer.getLatencySamples());
}

void BrownHelpProcessor::releaseResources()
{
    engineer.reset();
}

bool BrownHelpProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output
           && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

void BrownHelpProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (resetLearningRequested.exchange(false, std::memory_order_acq_rel))
        engineer.resetLearning();

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    engineer.process(buffer, readSettings(), parameterValue<float>(parameters, bypassId) >= 0.5f);
}

juce::AudioProcessorEditor* BrownHelpProcessor::createEditor()
{
    return new BrownHelpEditor(*this);
}

bool BrownHelpProcessor::hasEditor() const { return true; }
const juce::String BrownHelpProcessor::getName() const { return JucePlugin_Name; }
bool BrownHelpProcessor::acceptsMidi() const { return false; }
bool BrownHelpProcessor::producesMidi() const { return false; }
bool BrownHelpProcessor::isMidiEffect() const { return false; }
double BrownHelpProcessor::getTailLengthSeconds() const { return 0.0; }
int BrownHelpProcessor::getNumPrograms() { return 1; }
int BrownHelpProcessor::getCurrentProgram() { return 0; }
void BrownHelpProcessor::setCurrentProgram(int) {}
const juce::String BrownHelpProcessor::getProgramName(int) { return {}; }
void BrownHelpProcessor::changeProgramName(int, const juce::String&) {}

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

VoiceEngineer::AnalysisSnapshot BrownHelpProcessor::getAnalysisSnapshot() const
{
    return engineer.getAnalysisSnapshot();
}

void BrownHelpProcessor::resetLearning()
{
    resetLearningRequested.store(true, std::memory_order_release);
}

juce::AudioProcessorValueTreeState::ParameterLayout BrownHelpProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;
    layout.push_back(std::make_unique<juce::AudioParameterBool>(autoBalanceId, "Auto Balance", true));
    layout.push_back(std::make_unique<juce::AudioParameterBool>(
        manualFundamentalEnabledId, "Manual Fundamental", false));
    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        manualFundamentalFrequencyId, "Manual Fundamental Frequency",
        juce::NormalisableRange<float>(100.0f, 600.0f, 0.1f, 0.4f), 125.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    layout.push_back(std::make_unique<juce::AudioParameterBool>(lowShelfEnabledId, "Low Shelf", false));
    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        lowShelfFrequencyId, "Low Shelf Frequency",
        juce::NormalisableRange<float>(20.0f, 500.0f, 0.1f, 0.4f), 120.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        lowShelfReductionId, "Low Shelf Reduction",
        juce::NormalisableRange<float>(0.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        lowShelfSlopeId, "Low Shelf Slope", juce::StringArray { "12 dB/oct", "18 dB/oct" }, 0));
    layout.push_back(std::make_unique<juce::AudioParameterBool>(highShelfEnabledId, "High Shelf", false));
    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        highShelfFrequencyId, "High Shelf Frequency",
        juce::NormalisableRange<float>(2000.0f, 20000.0f, 1.0f, 0.4f), 6500.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        highShelfReductionId, "High Shelf Reduction",
        juce::NormalisableRange<float>(0.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        highShelfSlopeId, "High Shelf Slope", juce::StringArray { "12 dB/oct", "18 dB/oct" }, 0));
    layout.push_back(std::make_unique<juce::AudioParameterBool>(bypassId, "Bypass", false));
    return { layout.begin(), layout.end() };
}

VoiceEngineer::Settings BrownHelpProcessor::readSettings() const
{
    VoiceEngineer::Settings settings;
    settings.autoBalance = parameterValue<float>(parameters, autoBalanceId) >= 0.5f;
    settings.manualFundamentalEnabled = parameterValue<float>(parameters, manualFundamentalEnabledId) >= 0.5f;
    settings.manualFundamentalFrequencyHz = parameterValue<float>(parameters, manualFundamentalFrequencyId);
    settings.lowShelfEnabled = parameterValue<float>(parameters, lowShelfEnabledId) >= 0.5f;
    settings.lowShelfFrequencyHz = parameterValue<float>(parameters, lowShelfFrequencyId);
    settings.lowShelfReductionDb = parameterValue<float>(parameters, lowShelfReductionId);
    settings.lowShelfSlope = parameterValue<int>(parameters, lowShelfSlopeId);
    settings.highShelfEnabled = parameterValue<float>(parameters, highShelfEnabledId) >= 0.5f;
    settings.highShelfFrequencyHz = parameterValue<float>(parameters, highShelfFrequencyId);
    settings.highShelfReductionDb = parameterValue<float>(parameters, highShelfReductionId);
    settings.highShelfSlope = parameterValue<int>(parameters, highShelfSlopeId);
    return settings;
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BrownHelp::BrownHelpProcessor();
}
