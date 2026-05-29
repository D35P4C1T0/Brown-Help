#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace BrownHelp
{
class BrownCurveBalancer
{
public:
    static constexpr int bandCount = 24;

    enum class Curve
    {
        gentleBrown = 0,
        brown = 1,
        darkBrown = 2
    };

    struct Settings
    {
        Curve curve = Curve::brown;
        float tiltDbPerOctave = -4.5f;
        float strength = 0.5f;
        float mix = 1.0f;
        float lowFrequencyHz = 20.0f;
        float highFrequencyHz = 20000.0f;
        float maxCorrectionDb = 12.0f;
        float speed = 0.45f;
    };

    void prepare(double sampleRate, int maximumBlockSize, int channels);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, const Settings& settings);

    static float targetDbForFrequency(float frequencyHz, float referenceHz, float tiltDbPerOctave);
    static float curveTilt(Curve curve, float userTiltDbPerOctave);

private:
    struct Band
    {
        float frequencyHz = 1000.0f;
        float envelope = 0.0f;
        float smoothedGainDb = 0.0f;
        float appliedGainDb = 1000.0f;
        std::vector<juce::dsp::IIR::Filter<float>> detectors;
        std::vector<juce::dsp::IIR::Filter<float>> eq;
    };

    void updateBands(const Settings& settings, int channels);
    void updateDetectorCoefficients(Band& band, int channels, float q);
    void updateEqCoefficients(Band& band, int channels, float q, float gainDb);
    float detectorCoefficient(const Settings& settings) const;
    float gainSmoothingCoefficient(const Settings& settings) const;

    double currentSampleRate = 44100.0;
    int currentChannels = 0;
    float previousLowHz = -1.0f;
    float previousHighHz = -1.0f;

    std::array<Band, bandCount> bands;
    juce::AudioBuffer<float> dryBuffer;
};
}
