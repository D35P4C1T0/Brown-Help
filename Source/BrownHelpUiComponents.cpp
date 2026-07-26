#include "BrownHelpUiComponents.h"

#include "Parameters.h"
#include "UiStyle.h"

#include <array>
#include <cmath>

namespace BrownHelp
{
TiltPreview::TiltPreview(BrownHelpProcessor& processorToUse)
    : processor(processorToUse)
{
    startTimerHz(15);
}

void TiltPreview::paint(juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat();
    graphics.setColour(juce::Colour(Ui::plotColour));
    graphics.fillRoundedRectangle(bounds, 2.0f);
    graphics.setColour(juce::Colour(Ui::outlineColour));
    graphics.drawRoundedRectangle(bounds.reduced(0.5f), 2.0f, 1.0f);

    auto plot = getLocalBounds().reduced(12, 7);
    plot.removeFromTop(20);

    auto& parameters = processor.getParameters();
    const auto tilt = parameters.getRawParameterValue(tiltId)->load();
    const auto curve = static_cast<int>(parameters.getRawParameterValue(curveId)->load());
    const auto mix = std::clamp(parameters.getRawParameterValue(mixId)->load(), 0.0f, 1.0f);
    const auto flip = parameters.getRawParameterValue(tiltFlipId)->load() >= 0.5f;
    const auto slope = tiltControlToDbPerOctave(tilt, curve, flip);
    const auto lowFrequency = parameters.getRawParameterValue(lowFrequencyId)->load();
    const auto highFrequency = parameters.getRawParameterValue(highFrequencyId)->load();
    const auto centreY = static_cast<float>(plot.getCentreY());
    const auto scale = static_cast<float>(plot.getHeight()) / 70.0f;

    const auto graphLowLog = std::log(20.0f);
    const auto graphHighLog = std::log(20000.0f);
    const std::array<float, 5> frequencyMarks { 20.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f };

    graphics.setFont(juce::FontOptions(10.0f));

    const auto frequencyToX = [&plot, graphLowLog, graphHighLog](float frequency)
    {
        const auto ratio = (std::log(std::clamp(frequency, 20.0f, 20000.0f)) - graphLowLog)
                           / (graphHighLog - graphLowLog);
        return static_cast<float>(plot.getX()) + ratio * static_cast<float>(plot.getWidth());
    };

    const auto rangeLeft = frequencyToX(std::min(lowFrequency, highFrequency));
    const auto rangeRight = frequencyToX(std::max(lowFrequency, highFrequency));
    graphics.setColour(juce::Colour(Ui::accentColour).withAlpha(0.045f));
    graphics.fillRect(juce::Rectangle<float>(
        rangeLeft,
        static_cast<float>(plot.getY()),
        std::max(1.0f, rangeRight - rangeLeft),
        static_cast<float>(plot.getHeight())));

    for (const auto frequency : frequencyMarks)
    {
        const auto x = frequencyToX(frequency);

        graphics.setColour(juce::Colour(Ui::outlineColour).withAlpha(0.65f));
        graphics.drawVerticalLine(static_cast<int>(std::round(x)), static_cast<float>(plot.getY()), static_cast<float>(plot.getBottom()));
        graphics.setColour(juce::Colour(Ui::mutedTextColour).withAlpha(0.8f));
        const auto label = frequency >= 1000.0f ? juce::String(frequency / 1000.0f, frequency >= 10000.0f ? 0 : 1) + "k"
                                                : juce::String(static_cast<int>(frequency));
        graphics.drawText(label, static_cast<int>(x - 18.0f), plot.getBottom() - 14, 36, 12, juce::Justification::centred);
    }

    if (parameters.getRawParameterValue(highPassEnabledId)->load() >= 0.5f)
    {
        const auto cutoff = parameters.getRawParameterValue(highPassFrequencyId)->load();
        const auto slopeChoice = static_cast<int>(parameters.getRawParameterValue(highPassSlopeId)->load());
        const auto slopePerOctave = slopeChoice == 1 ? 24.0f : 12.0f;
        juce::Path highPassPath;

        for (int point = 0; point < 96; ++point)
        {
            const auto ratio = static_cast<float>(point) / 95.0f;
            const auto frequency = 20.0f * std::pow(1000.0f, ratio);
            const auto attenuationDb = frequency < cutoff ? -slopePerOctave * std::log2(cutoff / std::max(1.0f, frequency)) : 0.0f;
            const auto x = static_cast<float>(plot.getX()) + ratio * static_cast<float>(plot.getWidth());
            const auto y = std::clamp(centreY - attenuationDb * scale,
                                      static_cast<float>(plot.getY()),
                                      static_cast<float>(plot.getBottom()));

            if (point == 0)
                highPassPath.startNewSubPath(x, y);
            else
                highPassPath.lineTo(x, y);
        }

        graphics.setColour(juce::Colour(Ui::mutedTextColour).withAlpha(0.75f));
        graphics.strokePath(highPassPath, juce::PathStrokeType(1.4f));
    }

    juce::Path path;

    for (int point = 0; point < 64; ++point)
    {
        const auto ratio = static_cast<float>(point) / 63.0f;
        const auto frequency = 20.0f * std::pow(1000.0f, ratio);
        const auto targetDb = slope * std::log2(frequency / 1000.0f);
        const auto x = static_cast<float>(plot.getX()) + ratio * static_cast<float>(plot.getWidth());
        const auto y = std::clamp(
            centreY - targetDb * scale,
            static_cast<float>(plot.getY()),
            static_cast<float>(plot.getBottom()));

        if (point == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    graphics.setColour(juce::Colour(Ui::outlineColour));
    graphics.drawHorizontalLine(plot.getCentreY(), static_cast<float>(plot.getX()), static_cast<float>(plot.getRight()));

    graphics.setColour(juce::Colour(Ui::accentColour).withAlpha(0.35f + mix * 0.65f));
    graphics.strokePath(path, juce::PathStrokeType(1.75f));

    const auto analysis = processor.getAnalysisSnapshot();

    if (analysis.frequenciesHz.front() > 0.0f)
    {
        juce::Path correctionPath;

        for (size_t index = 0; index < analysis.frequenciesHz.size(); ++index)
        {
            const auto x = frequencyToX(analysis.frequenciesHz[index]);
            const auto y = std::clamp(
                centreY - analysis.correctionDb[index] * scale * 2.0f,
                static_cast<float>(plot.getY()),
                static_cast<float>(plot.getBottom()));

            if (index == 0)
                correctionPath.startNewSubPath(x, y);
            else
                correctionPath.lineTo(x, y);
        }

        graphics.setColour(juce::Colour(Ui::correctionColour).withAlpha(analysis.signalPresent ? 0.9f : 0.35f));
        graphics.strokePath(correctionPath, juce::PathStrokeType(1.4f));
    }

    graphics.setColour(juce::Colour(Ui::accentColour));
    graphics.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    graphics.drawText("TARGET", 12, 5, 48, 12, juce::Justification::centredLeft);
    graphics.setColour(juce::Colour(Ui::correctionColour));
    graphics.drawText("CORRECTION", 66, 5, 74, 12, juce::Justification::centredLeft);

    graphics.setColour(juce::Colour(Ui::mutedTextColour));
    graphics.setFont(juce::FontOptions(9.0f));
    graphics.drawText(juce::String(slope, 2) + " dB / oct", getWidth() / 2, 5, getWidth() / 2 - 12, 12, juce::Justification::centredRight);
}

void TiltPreview::timerCallback()
{
    repaint();
}

void FrequencySlider::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    juce::ignoreUnused(event);

    const auto delta = std::abs(wheel.deltaY) > std::abs(wheel.deltaX) ? wheel.deltaY : -wheel.deltaX;

    if (delta == 0.0f)
        return;

    wheelAccumulator += delta;

    if (std::abs(wheelAccumulator) < 0.06f)
        return;

    const auto direction = wheelAccumulator > 0.0f ? 1.0 : -1.0;
    wheelAccumulator = 0.0f;
    const auto semitoneRatio = std::pow(2.0, direction / 12.0);
    setValue(getValue() * semitoneRatio, juce::sendNotificationSync);
}
}
