#include "BrownHelpEditor.h"

#include "Parameters.h"

#include <cmath>

namespace BrownHelp
{
namespace
{
constexpr auto backgroundColour = 0xff171c1f;
constexpr auto panelColour = 0xff1b2226;
constexpr auto outlineColour = 0xff384044;
constexpr auto textColour = 0xfff1eadf;
constexpr auto mutedTextColour = 0xffb9b9b5;
constexpr auto accentColour = 0xffc8ad84;

float parameter(const juce::AudioProcessorValueTreeState& parameters, const char* id)
{
    return parameters.getRawParameterValue(id)->load();
}
}

BrownHelpEditor::BrownHelpEditor(BrownHelpProcessor& processorToUse)
    : AudioProcessorEditor(&processorToUse),
      audioProcessor(processorToUse),
      tiltPreview(processorToUse)
{
    curveBox.addItem("Gentle Brown", 1);
    curveBox.addItem("Brown", 2);
    curveBox.addItem("Dark Brown", 3);
    addAndMakeVisible(curveBox);
    addAndMakeVisible(curveLabel);
    curveLabel.setText("Curve", juce::dontSendNotification);

    oversamplingBox.addItem("1x", 1);
    oversamplingBox.addItem("2x", 2);
    oversamplingBox.addItem("4x", 3);
    addAndMakeVisible(oversamplingBox);
    addAndMakeVisible(oversamplingLabel);
    oversamplingLabel.setText("Oversampling", juce::dontSendNotification);

    addAndMakeVisible(bypassButton);
    bypassButton.setButtonText("Bypass");
    bypassButton.setClickingTogglesState(true);

    addAndMakeVisible(helpButton);
    helpButton.setButtonText("?");
    helpButton.onClick = [this]
    {
        showHelpPanel = ! showHelpPanel;
        repaint();
    };

    addAndMakeVisible(tiltFlipButton);
    tiltFlipButton.setButtonText("Flip");
    tiltFlipButton.setClickingTogglesState(true);

    highPassSlopeBox.addItem("12 dB/oct", 1);
    highPassSlopeBox.addItem("24 dB/oct", 2);
    addAndMakeVisible(highPassSlopeBox);
    addAndMakeVisible(highPassSlopeLabel);
    highPassSlopeLabel.setText("HP Slope", juce::dontSendNotification);

    addAndMakeVisible(highPassButton);
    highPassButton.setButtonText({});

    addAndMakeVisible(saturationButton);
    saturationButton.setButtonText({});

    addSlider(tiltSlider, tiltLabel, "Tilt");
    addSlider(strengthSlider, strengthLabel, "Strength");
    addSlider(mixSlider, mixLabel, "Mix");
    addSlider(lowFrequencySlider, lowFrequencyLabel, "Low");
    addSlider(highFrequencySlider, highFrequencyLabel, "High");
    addSlider(maxCorrectionSlider, maxCorrectionLabel, "Max Correction (dB)");
    addSlider(speedSlider, speedLabel, "Speed");
    addSlider(highPassFrequencySlider, highPassFrequencyLabel, "HP Frequency");
    addSlider(saturationFrequencySlider, saturationFrequencyLabel, "Sat Frequency");
    addSlider(saturationDriveSlider, saturationDriveLabel, "Sat Drive");
    addSlider(saturationMixSlider, saturationMixLabel, "Sat Mix");
    addAndMakeVisible(tiltPreview);

    mixSlider.textFromValueFunction = [] (double value)
    {
        return juce::String(static_cast<int>(std::round(value * 100.0))) + "%";
    };

    mixSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        return text.retainCharacters("0123456789.").getDoubleValue() / 100.0;
    };

    auto percentText = [] (double value)
    {
        return juce::String(value * 100.0, 1) + "%";
    };

    auto percentFromText = [] (const juce::String& text)
    {
        return text.retainCharacters("0123456789.").getDoubleValue() / 100.0;
    };

    for (auto* slider : { &strengthSlider, &speedSlider, &saturationDriveSlider, &saturationMixSlider })
    {
        slider->textFromValueFunction = percentText;
        slider->valueFromTextFunction = percentFromText;
    }

    tiltSlider.textFromValueFunction = [] (double value)
    {
        return juce::String(value, 0) + "%";
    };

    tiltSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        return text.retainCharacters("0123456789.").getDoubleValue();
    };

    maxCorrectionSlider.textFromValueFunction = [] (double value)
    {
        return juce::String(value, 1) + " dB";
    };

    maxCorrectionSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        return text.retainCharacters("0123456789.").getDoubleValue();
    };

    auto frequencyText = [] (double value)
    {
        if (value >= 1000.0)
            return juce::String(value / 1000.0, value >= 10000.0 ? 1 : 2) + " kHz";

        return juce::String(value, 0) + " Hz";
    };

    auto frequencyFromText = [] (const juce::String& text)
    {
        const auto lower = text.toLowerCase();
        const auto value = lower.retainCharacters("0123456789.").getDoubleValue();
        return lower.contains("k") ? value * 1000.0 : value;
    };

    for (auto* slider : { static_cast<juce::Slider*>(&lowFrequencySlider),
                          static_cast<juce::Slider*>(&highFrequencySlider),
                          &highPassFrequencySlider,
                          &saturationFrequencySlider })
    {
        slider->textFromValueFunction = frequencyText;
        slider->valueFromTextFunction = frequencyFromText;
    }

    for (auto* slider : { &tiltSlider, &strengthSlider, &maxCorrectionSlider, &speedSlider,
                          &highPassFrequencySlider, &saturationFrequencySlider,
                          &saturationDriveSlider, &saturationMixSlider })
        configureHorizontalSlider(*slider);

    for (juce::Slider* slider : { &mixSlider })
        configureSmallRotary(*slider);

    for (juce::Slider* slider : { static_cast<juce::Slider*>(&lowFrequencySlider), static_cast<juce::Slider*>(&highFrequencySlider) })
        configureHorizontalSlider(*slider);

    curveAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.getParameters(), curveId, curveBox);
    oversamplingAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.getParameters(), oversamplingId, oversamplingBox);
    highPassSlopeAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.getParameters(), highPassSlopeId, highPassSlopeBox);
    bypassAttachment = std::make_unique<ButtonAttachment>(audioProcessor.getParameters(), bypassId, bypassButton);
    highPassAttachment = std::make_unique<ButtonAttachment>(audioProcessor.getParameters(), highPassEnabledId, highPassButton);
    saturationAttachment = std::make_unique<ButtonAttachment>(audioProcessor.getParameters(), saturationEnabledId, saturationButton);
    tiltFlipAttachment = std::make_unique<ButtonAttachment>(audioProcessor.getParameters(), tiltFlipId, tiltFlipButton);
    tiltAttachment = std::make_unique<SliderAttachment>(audioProcessor.getParameters(), tiltId, tiltSlider);
    strengthAttachment = std::make_unique<SliderAttachment>(audioProcessor.getParameters(), strengthId, strengthSlider);
    mixAttachment = std::make_unique<SliderAttachment>(audioProcessor.getParameters(), mixId, mixSlider);
    lowFrequencyAttachment = std::make_unique<SliderAttachment>(audioProcessor.getParameters(), lowFrequencyId, lowFrequencySlider);
    highFrequencyAttachment = std::make_unique<SliderAttachment>(audioProcessor.getParameters(), highFrequencyId, highFrequencySlider);
    maxCorrectionAttachment = std::make_unique<SliderAttachment>(audioProcessor.getParameters(), maxCorrectionId, maxCorrectionSlider);
    speedAttachment = std::make_unique<SliderAttachment>(audioProcessor.getParameters(), speedId, speedSlider);
    highPassFrequencyAttachment = std::make_unique<SliderAttachment>(audioProcessor.getParameters(), highPassFrequencyId, highPassFrequencySlider);
    saturationFrequencyAttachment = std::make_unique<SliderAttachment>(audioProcessor.getParameters(), saturationFrequencyId, saturationFrequencySlider);
    saturationDriveAttachment = std::make_unique<SliderAttachment>(audioProcessor.getParameters(), saturationDriveId, saturationDriveSlider);
    saturationMixAttachment = std::make_unique<SliderAttachment>(audioProcessor.getParameters(), saturationMixId, saturationMixSlider);

    updateSectionState();
    startTimerHz(12);
    setSize(960, 570);
}

void BrownHelpEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(backgroundColour));
    graphics.setColour(juce::Colour(accentColour));
    graphics.setFont(juce::FontOptions(24.0f));
    graphics.drawText("~", 22, 14, 42, 30, juce::Justification::centred);

    graphics.setColour(juce::Colour(textColour));
    graphics.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    graphics.drawText("Brown Help", 66, 14, 180, 30, juce::Justification::centredLeft);

    graphics.setColour(juce::Colour(mutedTextColour));
    graphics.setFont(juce::FontOptions(15.0f));
    graphics.drawText("D35P Audio", 242, 18, 140, 28, juce::Justification::centredLeft);

    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(54);
    drawPanel(graphics, area.removeFromTop(214), {});
    area.removeFromTop(8);

    auto panelRow = area.removeFromTop(238);
    const auto gap = 8;
    const auto panelWidth = (panelRow.getWidth() - gap * 3) / 4;
    drawPanel(graphics, panelRow.removeFromLeft(panelWidth), "TARGET / TILT");
    panelRow.removeFromLeft(gap);
    drawPanel(graphics, panelRow.removeFromLeft(panelWidth), "MIX / RANGE");
    panelRow.removeFromLeft(gap);
    auto highPassBounds = panelRow.removeFromLeft(panelWidth);
    drawPanel(graphics, highPassBounds, "HIGH-PASS");
    panelRow.removeFromLeft(gap);
    auto saturationBounds = panelRow;
    drawPanel(graphics, saturationBounds, "SATURATION");

    if (parameter(audioProcessor.getParameters(), highPassEnabledId) < 0.5f)
    {
        graphics.setColour(juce::Colour(backgroundColour).withAlpha(0.48f));
        graphics.fillRoundedRectangle(highPassBounds.reduced(1).toFloat(), 5.0f);
    }

    if (parameter(audioProcessor.getParameters(), saturationEnabledId) < 0.5f)
    {
        graphics.setColour(juce::Colour(backgroundColour).withAlpha(0.48f));
        graphics.fillRoundedRectangle(saturationBounds.reduced(1).toFloat(), 5.0f);
    }

    if (parameter(audioProcessor.getParameters(), bypassId) >= 0.5f)
    {
        graphics.setColour(juce::Colour(backgroundColour).withAlpha(0.62f));
        auto content = getLocalBounds().reduced(10);
        content.removeFromTop(54);
        graphics.fillRoundedRectangle(content.toFloat(), 5.0f);
    }

    if (showHelpPanel)
    {
        auto helpBounds = getLocalBounds().withSizeKeepingCentre(620, 260).translated(0, -40);
        graphics.setColour(juce::Colours::black.withAlpha(0.35f));
        graphics.fillRoundedRectangle(getLocalBounds().reduced(10).toFloat(), 5.0f);
        graphics.setColour(juce::Colour(0xff222a2f));
        graphics.fillRoundedRectangle(helpBounds.toFloat(), 7.0f);
        graphics.setColour(juce::Colour(accentColour));
        graphics.drawRoundedRectangle(helpBounds.toFloat().reduced(0.5f), 7.0f, 1.2f);

        helpBounds.reduce(22, 18);
        graphics.setColour(juce::Colour(textColour));
        graphics.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        graphics.drawText("How Brown Help Works", helpBounds.removeFromTop(28), juce::Justification::centredLeft);

        graphics.setColour(juce::Colour(mutedTextColour));
        graphics.setFont(juce::FontOptions(14.0f));
        graphics.drawFittedText(
            "Brown Help gently shapes voice toward a tilt curve.\n\n"
            "Signal chain: High Pass -> Adaptive Correction -> High Saturation -> Output Guard\n\n"
            "Low/High set where correction works. They do not cut audio.\n"
            "High Pass is the real low-cut filter, before correction.\n"
            "Saturation is post-EQ and affects only the high band.",
            helpBounds,
            juce::Justification::topLeft,
            8);
    }
}

void BrownHelpEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    auto header = area.removeFromTop(54);
    auto headerRight = header.removeFromRight(300);
    layoutCombo(oversamplingBox, oversamplingLabel, headerRight.removeFromLeft(156).withTrimmedTop(6).withHeight(42));
    headerRight.removeFromLeft(12);
    bypassButton.setBounds(headerRight.removeFromLeft(100).withTrimmedTop(16).withHeight(26));
    helpButton.setBounds(headerRight.removeFromLeft(28).withTrimmedTop(16).withHeight(26));

    tiltPreview.setBounds(area.removeFromTop(214).reduced(10));
    area.removeFromTop(8);

    auto panelRow = area.removeFromTop(238);
    const auto gap = 8;
    const auto panelWidth = (panelRow.getWidth() - gap * 3) / 4;

    auto targetPanel = panelRow.removeFromLeft(panelWidth).reduced(14);
    targetPanel.removeFromTop(34);
    auto curveRow = targetPanel.removeFromTop(52);
    auto flipArea = curveRow.removeFromRight(54);
    layoutCombo(curveBox, curveLabel, curveRow.reduced(0, 0));
    tiltFlipButton.setBounds(flipArea.withTrimmedTop(18).withHeight(28));
    targetPanel.removeFromTop(8);
    layoutHorizontalSlider(tiltSlider, tiltLabel, targetPanel.removeFromTop(44));
    layoutHorizontalSlider(strengthSlider, strengthLabel, targetPanel.removeFromTop(44));
    layoutHorizontalSlider(maxCorrectionSlider, maxCorrectionLabel, targetPanel.removeFromTop(44));
    layoutHorizontalSlider(speedSlider, speedLabel, targetPanel.removeFromTop(44));

    panelRow.removeFromLeft(gap);
    auto mixPanel = panelRow.removeFromLeft(panelWidth).reduced(14);
    mixPanel.removeFromTop(34);
    layoutSlider(mixSlider, mixLabel, mixPanel.removeFromTop(108).withSizeKeepingCentre(128, 108));
    mixPanel.removeFromTop(12);
    layoutInlineSlider(lowFrequencySlider, lowFrequencyLabel, mixPanel.removeFromTop(28));
    mixPanel.removeFromTop(10);
    layoutInlineSlider(highFrequencySlider, highFrequencyLabel, mixPanel.removeFromTop(28));

    panelRow.removeFromLeft(gap);
    auto highPassPanelBounds = panelRow.removeFromLeft(panelWidth);
    highPassButton.setBounds(highPassPanelBounds.getRight() - 40, highPassPanelBounds.getY() + 9, 24, 24);
    auto highPassPanel = highPassPanelBounds.reduced(14);
    highPassPanel.removeFromTop(48);
    layoutHorizontalSlider(highPassFrequencySlider, highPassFrequencyLabel, highPassPanel.removeFromTop(48));
    highPassPanel.removeFromTop(10);
    layoutCombo(highPassSlopeBox, highPassSlopeLabel, highPassPanel.removeFromTop(54));

    panelRow.removeFromLeft(gap);
    auto saturationPanelBounds = panelRow;
    saturationButton.setBounds(saturationPanelBounds.getRight() - 40, saturationPanelBounds.getY() + 9, 24, 24);
    auto saturationPanel = saturationPanelBounds.reduced(14);
    saturationPanel.removeFromTop(48);
    layoutHorizontalSlider(saturationFrequencySlider, saturationFrequencyLabel, saturationPanel.removeFromTop(48));
    layoutHorizontalSlider(saturationDriveSlider, saturationDriveLabel, saturationPanel.removeFromTop(48));
    layoutHorizontalSlider(saturationMixSlider, saturationMixLabel, saturationPanel.removeFromTop(48));

}

void BrownHelpEditor::addSlider(juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    addAndMakeVisible(slider);
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 16);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffb8c0aa));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xfff4eee3));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff4eee3));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::trackColourId, juce::Colour(accentColour));
    slider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff4a5052));

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, juce::Colour(0xfff4eee3));
}

void BrownHelpEditor::layoutSlider(juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds)
{
    label.setBounds(bounds.removeFromTop(16));
    bounds.removeFromTop(1);
    slider.setBounds(bounds);
}

void BrownHelpEditor::layoutHorizontalSlider(juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds)
{
    auto labelRow = bounds.removeFromTop(18);
    label.setBounds(labelRow.removeFromLeft(112));
    label.setJustificationType(juce::Justification::centredLeft);
    slider.setBounds(bounds);
}

void BrownHelpEditor::layoutInlineSlider(juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds)
{
    label.setBounds(bounds.removeFromLeft(40));
    label.setJustificationType(juce::Justification::centredLeft);
    slider.setBounds(bounds);
}

void BrownHelpEditor::layoutCombo(juce::ComboBox& comboBox, juce::Label& label, juce::Rectangle<int> bounds)
{
    label.setBounds(bounds.removeFromTop(18));
    label.setJustificationType(juce::Justification::centredLeft);
    comboBox.setBounds(bounds);
}

void BrownHelpEditor::drawPanel(juce::Graphics& graphics, juce::Rectangle<int> bounds, const juce::String& title)
{
    auto panel = bounds.toFloat();
    graphics.setColour(juce::Colour(panelColour));
    graphics.fillRoundedRectangle(panel, 5.0f);
    graphics.setColour(juce::Colour(outlineColour));
    graphics.drawRoundedRectangle(panel.reduced(0.5f), 5.0f, 1.0f);

    if (title.isNotEmpty())
    {
        graphics.setColour(juce::Colour(textColour));
        graphics.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        graphics.drawText(title, bounds.getX() + 14, bounds.getY() + 12, bounds.getWidth() - 28, 18, juce::Justification::centredLeft);
        graphics.setColour(juce::Colour(accentColour));
        graphics.drawHorizontalLine(bounds.getY() + 36, static_cast<float>(bounds.getX() + 14), static_cast<float>(bounds.getRight() - 14));
    }
}

void BrownHelpEditor::configureHorizontalSlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 66, 18);
}

void BrownHelpEditor::configureSmallRotary(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 74, 18);
}

void BrownHelpEditor::updateSectionState()
{
    const auto highPassEnabled = parameter(audioProcessor.getParameters(), highPassEnabledId) >= 0.5f;
    const auto saturationEnabled = parameter(audioProcessor.getParameters(), saturationEnabledId) >= 0.5f;
    const auto bypassed = parameter(audioProcessor.getParameters(), bypassId) >= 0.5f;

    for (auto* component : { static_cast<juce::Component*>(&highPassFrequencySlider),
                             static_cast<juce::Component*>(&highPassFrequencyLabel),
                             static_cast<juce::Component*>(&highPassSlopeBox),
                             static_cast<juce::Component*>(&highPassSlopeLabel) })
        component->setAlpha((highPassEnabled && ! bypassed) ? 1.0f : 0.38f);

    for (auto* component : { static_cast<juce::Component*>(&saturationFrequencySlider),
                             static_cast<juce::Component*>(&saturationFrequencyLabel),
                             static_cast<juce::Component*>(&saturationDriveSlider),
                             static_cast<juce::Component*>(&saturationDriveLabel),
                             static_cast<juce::Component*>(&saturationMixSlider),
                             static_cast<juce::Component*>(&saturationMixLabel) })
        component->setAlpha((saturationEnabled && ! bypassed) ? 1.0f : 0.38f);

    for (auto* component : { static_cast<juce::Component*>(&curveBox),
                             static_cast<juce::Component*>(&curveLabel),
                             static_cast<juce::Component*>(&tiltFlipButton),
                             static_cast<juce::Component*>(&tiltSlider),
                             static_cast<juce::Component*>(&tiltLabel),
                             static_cast<juce::Component*>(&strengthSlider),
                             static_cast<juce::Component*>(&strengthLabel),
                             static_cast<juce::Component*>(&maxCorrectionSlider),
                             static_cast<juce::Component*>(&maxCorrectionLabel),
                             static_cast<juce::Component*>(&speedSlider),
                             static_cast<juce::Component*>(&speedLabel),
                             static_cast<juce::Component*>(&mixSlider),
                             static_cast<juce::Component*>(&mixLabel),
                             static_cast<juce::Component*>(&lowFrequencySlider),
                             static_cast<juce::Component*>(&lowFrequencyLabel),
                             static_cast<juce::Component*>(&highFrequencySlider),
                             static_cast<juce::Component*>(&highFrequencyLabel),
                             static_cast<juce::Component*>(&tiltPreview),
                             static_cast<juce::Component*>(&oversamplingBox),
                             static_cast<juce::Component*>(&oversamplingLabel) })
        component->setAlpha(bypassed ? 0.38f : 1.0f);

    bypassButton.setColour(juce::TextButton::buttonColourId, bypassed ? juce::Colour(accentColour) : juce::Colour(0xff242b2f));
    bypassButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(accentColour));
    bypassButton.setColour(juce::TextButton::textColourOffId, bypassed ? juce::Colour(backgroundColour) : juce::Colour(textColour));
    bypassButton.setColour(juce::TextButton::textColourOnId, juce::Colour(backgroundColour));

    helpButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff242b2f));
    helpButton.setColour(juce::TextButton::textColourOffId, juce::Colour(textColour));

    const auto tiltFlipped = parameter(audioProcessor.getParameters(), tiltFlipId) >= 0.5f;
    tiltFlipButton.setColour(juce::TextButton::buttonColourId, tiltFlipped ? juce::Colour(accentColour) : juce::Colour(0xff242b2f));
    tiltFlipButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(accentColour));
    tiltFlipButton.setColour(juce::TextButton::textColourOffId, tiltFlipped ? juce::Colour(backgroundColour) : juce::Colour(textColour));
    tiltFlipButton.setColour(juce::TextButton::textColourOnId, juce::Colour(backgroundColour));
}

void BrownHelpEditor::timerCallback()
{
    updateSectionState();
    repaint();
}

void BrownHelpEditor::FrequencySlider::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
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
    setValue(getValue() + direction * 300.0, juce::sendNotificationSync);
}

BrownHelpEditor::TiltPreview::TiltPreview(BrownHelpProcessor& processorToUse)
    : processor(processorToUse)
{
    startTimerHz(15);
}

void BrownHelpEditor::TiltPreview::paint(juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat();
    graphics.setColour(juce::Colour(0xff263239));
    graphics.fillRoundedRectangle(bounds, 6.0f);

    auto plot = getLocalBounds().reduced(10, 5);
    plot.removeFromTop(22);

    auto& parameters = processor.getParameters();
    const auto tilt = parameters.getRawParameterValue(tiltId)->load();
    const auto curve = static_cast<int>(parameters.getRawParameterValue(curveId)->load());
    const auto mix = std::clamp(parameters.getRawParameterValue(mixId)->load(), 0.0f, 1.0f);
    const auto flip = parameters.getRawParameterValue(tiltFlipId)->load() >= 0.5f;
    const auto slope = tiltControlToDbPerOctave(tilt, curve, flip);
    const auto centreY = static_cast<float>(plot.getCentreY());
    const auto scale = static_cast<float>(plot.getHeight()) / 70.0f;

    juce::Path path;

    const auto graphLowLog = std::log(20.0f);
    const auto graphHighLog = std::log(20000.0f);
    const std::array<float, 10> frequencyMarks { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };

    graphics.setFont(juce::FontOptions(10.0f));

    for (const auto frequency : frequencyMarks)
    {
        const auto ratio = (std::log(frequency) - graphLowLog) / (graphHighLog - graphLowLog);
        const auto x = static_cast<float>(plot.getX()) + ratio * static_cast<float>(plot.getWidth());

        graphics.setColour(juce::Colour(0xff60707a).withAlpha(0.35f));
        graphics.drawVerticalLine(static_cast<int>(std::round(x)), static_cast<float>(plot.getY()), static_cast<float>(plot.getBottom()));
        graphics.setColour(juce::Colour(mutedTextColour).withAlpha(0.85f));
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

        graphics.setColour(juce::Colour(0xff8aa0aa).withAlpha(0.75f));
        graphics.strokePath(highPassPath, juce::PathStrokeType(1.4f));
    }

    for (int point = 0; point < 64; ++point)
    {
        const auto ratio = static_cast<float>(point) / 63.0f;
        const auto frequency = 20.0f * std::pow(1000.0f, ratio);
        const auto targetDb = slope * std::log2(frequency / 1000.0f);
        const auto x = static_cast<float>(plot.getX()) + ratio * static_cast<float>(plot.getWidth());
        const auto y = centreY - targetDb * scale;

        if (point == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    graphics.setColour(juce::Colour(0xff60707a));
    graphics.drawHorizontalLine(plot.getCentreY(), static_cast<float>(plot.getX()), static_cast<float>(plot.getRight()));

    graphics.setColour(juce::Colour(accentColour).withAlpha(0.25f + mix * 0.75f));
    graphics.strokePath(path, juce::PathStrokeType(2.0f));

    graphics.setColour(juce::Colour(0xfff4eee3));
    graphics.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    graphics.drawText("Tilt Curve", 10, 3, getWidth() / 2, 14, juce::Justification::centredLeft);

    graphics.setColour(juce::Colour(0xffb8c0aa));
    graphics.setFont(juce::FontOptions(10.0f));
    graphics.drawText(juce::String(slope, 2) + " dB/oct", getWidth() / 2, 3, getWidth() / 2 - 10, 14, juce::Justification::centredRight);
}

void BrownHelpEditor::TiltPreview::timerCallback()
{
    repaint();
}
}
