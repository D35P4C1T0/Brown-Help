#include "BrownHelpUiComponents.h"

#include "UiStyle.h"

#include <cmath>

namespace BrownHelp
{
namespace
{
juce::String frequencyText(float frequency)
{
    if (frequency <= 0.0f)
        return "--";
    if (frequency >= 1000.0f)
        return juce::String(frequency / 1000.0f, 2) + " kHz";
    return juce::String(frequency, 0) + " Hz";
}

juce::String levelText(float level, const juce::String& suffix)
{
    return level <= -99.0f ? "--" : juce::String(level, 1) + suffix;
}
}

SpectrumAnalyzer::SpectrumAnalyzer(BrownHelpProcessor& processorToUse)
    : processor(processorToUse)
{
    startTimerHz(20);
}

void SpectrumAnalyzer::paint(juce::Graphics& graphics)
{
    using namespace Ui;
    const auto snapshot = processor.getAnalysisSnapshot();
    auto bounds = getLocalBounds().toFloat();

    graphics.setColour(juce::Colour(plotColour));
    graphics.fillRoundedRectangle(bounds, 2.0f);
    graphics.setColour(juce::Colour(outlineColour));
    graphics.drawRoundedRectangle(bounds.reduced(0.5f), 2.0f, 1.0f);

    auto header = getLocalBounds().reduced(14, 8).removeFromTop(42);
    const auto metricWidth = std::max(120, header.getWidth() / 5);
    const std::array<juce::String, 5> titles { "LOUDNESS", "NORMALIZER", "FUNDAMENTAL", "SIBILANCE", "LIMITER" };
    const std::array<juce::String, 5> values {
        levelText(snapshot.loudnessLufs, " LUFS"),
        levelText(snapshot.normalizerGainDb, " dB"),
        frequencyText(snapshot.fundamentalHz) + "  " + levelText(snapshot.fundamentalRmsDb, " dB RMS"),
        frequencyText(snapshot.sibilanceHz) + "  " + levelText(snapshot.sibilanceRmsDb, " dB RMS"),
        levelText(snapshot.limiterReductionDb, " dB")
    };

    for (int index = 0; index < 5; ++index)
    {
        auto cell = header.removeFromLeft(index == 4 ? header.getWidth() : metricWidth);
        graphics.setColour(juce::Colour(mutedTextColour));
        graphics.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        graphics.drawText(titles[static_cast<size_t>(index)], cell.removeFromTop(14), juce::Justification::centredLeft);
        graphics.setColour(index == 2 ? juce::Colour(accentColour)
                                     : index == 3 ? juce::Colour(correctionColour)
                                                  : juce::Colour(textColour));
        graphics.setFont(juce::FontOptions(11.0f));
        graphics.drawFittedText(values[static_cast<size_t>(index)], cell, juce::Justification::centredLeft, 1);
    }

    auto plot = getLocalBounds().reduced(14, 8);
    plot.removeFromTop(49);
    plot.removeFromLeft(35);
    plot.removeFromBottom(18);
    const auto lowLog = std::log(30.0f);
    const auto highLog = std::log(20000.0f);

    const auto frequencyToX = [&plot, lowLog, highLog](float frequency)
    {
        const auto ratio = (std::log(std::clamp(frequency, 30.0f, 20000.0f)) - lowLog) / (highLog - lowLog);
        return static_cast<float>(plot.getX()) + ratio * static_cast<float>(plot.getWidth());
    };
    const auto dbToY = [&plot](float db)
    {
        const auto ratio = std::clamp((db + 80.0f) / 80.0f, 0.0f, 1.0f);
        return static_cast<float>(plot.getBottom()) - ratio * static_cast<float>(plot.getHeight());
    };

    for (const auto db : { -80, -60, -40, -20, 0 })
    {
        const auto y = dbToY(static_cast<float>(db));
        graphics.setColour(juce::Colour(outlineColour).withAlpha(db == -40 ? 0.9f : 0.48f));
        graphics.drawHorizontalLine(static_cast<int>(std::round(y)), static_cast<float>(plot.getX()), static_cast<float>(plot.getRight()));
        graphics.setColour(db == -40 ? juce::Colour(accentColour) : juce::Colour(mutedTextColour));
        graphics.setFont(juce::FontOptions(9.0f));
        graphics.drawText(juce::String(db), 2, static_cast<int>(y) - 7, 31, 14, juce::Justification::centredRight);
    }

    for (const auto frequency : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        const auto x = frequencyToX(frequency);
        graphics.setColour(juce::Colour(outlineColour).withAlpha(0.42f));
        graphics.drawVerticalLine(static_cast<int>(std::round(x)), static_cast<float>(plot.getY()), static_cast<float>(plot.getBottom()));
        graphics.setColour(juce::Colour(mutedTextColour));
        graphics.setFont(juce::FontOptions(9.0f));
        const auto label = frequency >= 1000.0f ? juce::String(frequency / 1000.0f, 0) + "k"
                                                : juce::String(frequency, 0);
        graphics.drawText(label, static_cast<int>(x) - 18, plot.getBottom() + 2, 36, 14, juce::Justification::centred);
    }

    if (snapshot.frequenciesHz.front() > 0.0f)
    {
        juce::Path spectrum;
        for (size_t index = 0; index < snapshot.frequenciesHz.size(); ++index)
        {
            const auto x = frequencyToX(snapshot.frequenciesHz[index]);
            const auto y = dbToY(snapshot.spectrumDb[index]);
            if (index == 0)
                spectrum.startNewSubPath(x, y);
            else
                spectrum.lineTo(x, y);
        }

        graphics.setColour(juce::Colour(correctionColour).withAlpha(snapshot.signalPresent ? 0.95f : 0.35f));
        graphics.strokePath(spectrum, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved));
    }

    const auto drawMarker = [&](float frequency, juce::Colour colour, const juce::String& label)
    {
        if (frequency <= 0.0f)
            return;
        const auto x = frequencyToX(frequency);
        graphics.setColour(colour.withAlpha(0.9f));
        graphics.drawVerticalLine(static_cast<int>(std::round(x)), static_cast<float>(plot.getY()), static_cast<float>(plot.getBottom()));
        graphics.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        graphics.drawText(label, static_cast<int>(x) + 3, plot.getY() + 3, 90, 14, juce::Justification::centredLeft);
    };

    drawMarker(snapshot.fundamentalHz, juce::Colour(accentColour),
               "F0  " + juce::String(snapshot.fundamentalCorrectionDb, 1) + " dB");
    drawMarker(snapshot.sibilanceHz, juce::Colour(correctionColour),
               "S  " + juce::String(snapshot.sibilanceCorrectionDb, 1) + " dB");

    graphics.setColour(juce::Colour(mutedTextColour));
    graphics.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    graphics.drawText("DISPLAY TILT  +4.5 dB/OCT", plot.getRight() - 180, plot.getY() + 3, 180, 14,
                      juce::Justification::centredRight);
    graphics.setColour(juce::Colour(accentColour));
    graphics.drawText("-40 dB RMS CEILING", plot.getX() + 4, static_cast<int>(dbToY(-40.0f)) - 15, 130, 14,
                      juce::Justification::centredLeft);
}

void SpectrumAnalyzer::timerCallback()
{
    repaint();
}
}
