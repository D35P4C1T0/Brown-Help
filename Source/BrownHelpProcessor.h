#pragma once

#include "BrownCurveBalancer.h"
#include "Parameters.h"

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>

namespace BrownHelp
{
class BrownHelpProcessor final : public juce::AudioProcessor
{
public:
    BrownHelpProcessor();
    ~BrownHelpProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters();
    BrownCurveBalancer::AnalysisSnapshot getAnalysisSnapshot() const;

private:
    struct SaturationPath
    {
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
        std::vector<juce::dsp::IIR::Filter<float>> highPass;
        double sampleRate = 44100.0;
        float previousFrequency = -1.0f;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    BrownCurveBalancer::Settings readSettings() const;
    void updateOversampling(int oversamplingChoice);
    void prepareHighPass(double sampleRate, int channels);
    void updateHighPass();
    void processHighPass(juce::AudioBuffer<float>& buffer);
    void prepareSaturationPaths(double sampleRate, int maximumBlockSize, int channels);
    void updateSaturation(SaturationPath& path);
    void processSaturation(juce::AudioBuffer<float>& buffer, bool enabled);
    void processSaturationSamples(juce::AudioBuffer<float>& buffer, SaturationPath& path);
    float calculateRms(const juce::AudioBuffer<float>& buffer) const;
    void applyAutoGainCompensation(juce::AudioBuffer<float>& buffer, float inputRms);
    void applyOutputGuard(juce::AudioBuffer<float>& buffer) const;

    juce::AudioProcessorValueTreeState parameters;
    BrownCurveBalancer balancer;
    std::vector<juce::dsp::IIR::Filter<float>> highPassStageOne;
    std::vector<juce::dsp::IIR::Filter<float>> highPassStageTwo;
    std::array<SaturationPath, 3> saturationPaths;
    std::vector<float*> oversampledChannelPointers;
    double hostSampleRate = 44100.0;
    double dspSampleRate = 44100.0;
    int currentOversamplingChoice = -1;
    float previousHighPassFrequency = -1.0f;
    int previousHighPassSlope = -1;
    float smoothedAutoGainDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrownHelpProcessor)
};
}
