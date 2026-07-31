#pragma once

#include <array>
#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace BrownHelp
{
class VoiceEngineer
{
public:
    static constexpr int spectrumPointCount = 96;
    static constexpr float targetLufs = -14.0f;
    static constexpr float bandCeilingDb = -40.0f;
    static constexpr float displayTiltDbPerOctave = 4.5f;

    struct Settings
    {
        bool autoBalance = true;
        bool manualFundamentalEnabled = false;
        float manualFundamentalFrequencyHz = 125.0f;
        bool lowShelfEnabled = false;
        float lowShelfFrequencyHz = 120.0f;
        float lowShelfReductionDb = 0.0f;
        int lowShelfSlope = 0;
        bool highShelfEnabled = false;
        float highShelfFrequencyHz = 6500.0f;
        float highShelfReductionDb = 0.0f;
        int highShelfSlope = 0;
    };

    struct AnalysisSnapshot
    {
        std::array<float, spectrumPointCount> frequenciesHz {};
        std::array<float, spectrumPointCount> spectrumDb {};
        float loudnessLufs = -100.0f;
        float normalizerGainDb = 0.0f;
        float fundamentalHz = 0.0f;
        float fundamentalConfidence = 0.0f;
        bool fundamentalIsManual = false;
        float fundamentalRmsDb = -100.0f;
        float sibilanceHz = 0.0f;
        float sibilanceRmsDb = -100.0f;
        float fundamentalCorrectionDb = 0.0f;
        float sibilanceCorrectionDb = 0.0f;
        float limiterReductionDb = 0.0f;
        bool signalPresent = false;
    };

    void prepare(double sampleRate, int maximumBlockSize, int channels);
    void reset();
    void resetLearning();
    void process(juce::AudioBuffer<float>& buffer, const Settings& settings, bool bypassed);
    int getLatencySamples() const;
    AnalysisSnapshot getAnalysisSnapshot() const;

private:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int loudnessHistorySize = 1200;
    static constexpr int maximumPitchFrameSize = 1024;
    static constexpr int pitchHistorySize = 5;

    struct PitchResult
    {
        float frequencyHz = 0.0f;
        float confidence = 0.0f;
        bool voiced = false;
    };

    void updateLoudness(const juce::AudioBuffer<float>& buffer);
    void applyNormalization(juce::AudioBuffer<float>& buffer);
    void pushAnalysisSamples(const juce::AudioBuffer<float>& buffer);
    void analyseFrame();
    void pushDisplaySamples(const juce::AudioBuffer<float>& buffer);
    void analyseDisplayFrame();
    void updateCorrectionFilters(int channels, bool enabled);
    void applyCorrections(juce::AudioBuffer<float>& buffer, const Settings& settings);
    void updateShelfFilters(const Settings& settings, int channels);
    void applyShelves(juce::AudioBuffer<float>& buffer, const Settings& settings);
    void applyLimiter(juce::AudioBuffer<float>& buffer, bool limitingEnabled);
    void pushPitchSample(float sample);
    void analysePitchFrame();
    PitchResult estimateFundamental() const;
    float firstVoiceCrestHz() const;
    float rmsNearFrequency(float frequencyHz, int halfWidthBins) const;
    float bandRms(float lowHz, float highHz) const;
    float peakFrequency(float lowHz, float highHz) const;
    void publishAnalysis();

    double currentSampleRate = 44100.0;
    int currentChannels = 0;
    int loudnessChunkSamples = 4410;
    int loudnessChunkPosition = 0;
    int loudnessChunkCount = 0;
    std::array<double, 4> loudnessChunks {};
    std::array<float, loudnessHistorySize> loudnessHistory {};
    int loudnessHistoryCount = 0;
    int loudnessHistoryWrite = 0;
    double loudnessChunkEnergy = 0.0;
    float measuredLufs = -100.0f;
    float smoothedNormalizerGainDb = 0.0f;
    float previousNormalizerGain = 1.0f;

    std::vector<juce::dsp::IIR::Filter<float>> kHighPass;
    std::vector<juce::dsp::IIR::Filter<float>> kHighShelf;
    std::vector<juce::dsp::IIR::Filter<float>> fundamentalEq;
    std::vector<juce::dsp::IIR::Filter<float>> sibilanceEq;
    std::array<std::vector<juce::dsp::IIR::Filter<float>>, 2> lowShelf;
    std::array<std::vector<juce::dsp::IIR::Filter<float>>, 2> highShelf;

    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize> analysisFifo {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2 + 1> binRms {};
    std::array<float, fftSize> displayFifo {};
    std::array<float, fftSize * 2> displayFftData {};
    std::array<float, fftSize / 2 + 1> displayBinRms {};
    std::array<float, fftSize> window {};
    int analysisFifoPosition = 0;
    int displayFifoPosition = 0;
    float windowSum = 1.0f;
    std::array<float, maximumPitchFrameSize> pitchFrame {};
    std::array<float, pitchHistorySize> pitchHistory {};
    int pitchFrameSize = 480;
    int pitchHopSize = 60;
    int pitchDecimation = 4;
    int pitchDecimationCounter = 0;
    int pitchWritePosition = 0;
    int pitchSamplesCollected = 0;
    int pitchHopCounter = 0;
    int pitchHistoryCount = 0;
    int pitchHistoryPosition = 0;
    float pitchSampleRate = 12000.0f;
    float pitchPreviousInput = 0.0f;
    float pitchHighPassOutput = 0.0f;
    float pitchLowPassOne = 0.0f;
    float pitchLowPassTwo = 0.0f;

    float learnedFundamentalHz = 0.0f;
    float activeFundamentalHz = 0.0f;
    float fundamentalConfidence = 0.0f;
    float fundamentalPeakRmsDb = -100.0f;
    float learnedSibilanceHz = 0.0f;
    float sibilancePeakRmsDb = -100.0f;
    float fundamentalCorrectionDb = 0.0f;
    float sibilanceCorrectionDb = 0.0f;
    float appliedFundamentalHz = -1.0f;
    float appliedSibilanceHz = -1.0f;
    float appliedFundamentalGainDb = 100.0f;
    float appliedSibilanceGainDb = 100.0f;

    Settings previousShelfSettings;
    Settings currentSettings;
    bool shelfSettingsValid = false;

    std::vector<std::vector<float>> limiterDelay;
    int limiterDelaySamples = 0;
    int limiterWritePosition = 0;
    int limiterHoldRemaining = 0;
    float limiterGain = 1.0f;
    float limiterReductionDb = 0.0f;

    std::array<std::atomic<float>, spectrumPointCount> publishedFrequencies {};
    std::array<std::atomic<float>, spectrumPointCount> publishedSpectrum {};
    std::atomic<float> publishedLufs { -100.0f };
    std::atomic<float> publishedNormalizerGainDb { 0.0f };
    std::atomic<float> publishedFundamentalHz { 0.0f };
    std::atomic<float> publishedFundamentalConfidence { 0.0f };
    std::atomic<bool> publishedFundamentalIsManual { false };
    std::atomic<float> publishedFundamentalRmsDb { -100.0f };
    std::atomic<float> publishedSibilanceHz { 0.0f };
    std::atomic<float> publishedSibilanceRmsDb { -100.0f };
    std::atomic<float> publishedFundamentalCorrectionDb { 0.0f };
    std::atomic<float> publishedSibilanceCorrectionDb { 0.0f };
    std::atomic<float> publishedLimiterReductionDb { 0.0f };
    std::atomic<bool> publishedSignalPresent { false };
};
}
