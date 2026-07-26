#include "BrownHelpEditor.h"

#include "ParameterFormatting.h"
#include "Parameters.h"
#include "UiStyle.h"

namespace BrownHelp
{
using namespace Ui;

BrownHelpEditor::BrownHelpEditor(BrownHelpProcessor& processorToUse)
    : AudioProcessorEditor(&processorToUse),
      audioProcessor(processorToUse),
      tiltPreview(processorToUse)
{
    setLookAndFeel(&lookAndFeel);

    curveBox.addItem("Gentle / 3 dB", 1);
    curveBox.addItem("Brown / 6 dB", 2);
    curveBox.addItem("Dark / 9 dB", 3);
    addAndMakeVisible(curveBox);
    addAndMakeVisible(curveLabel);
    curveLabel.setText("Curve", juce::dontSendNotification);

    oversamplingBox.addItem("1x", 1);
    oversamplingBox.addItem("2x", 2);
    oversamplingBox.addItem("4x", 3);
    addAndMakeVisible(oversamplingBox);
    addAndMakeVisible(oversamplingLabel);
    oversamplingLabel.setText("SAT QUALITY", juce::dontSendNotification);

    addAndMakeVisible(bypassButton);
    bypassButton.setButtonText("BYPASS");
    bypassButton.setClickingTogglesState(true);

    addAndMakeVisible(helpButton);
    helpButton.setButtonText("?");
    helpButton.onClick = [this]
    {
        showHelpPanel = ! showHelpPanel;
        repaint();
    };

    addAndMakeVisible(tiltFlipButton);
    tiltFlipButton.setButtonText("FLIP");
    tiltFlipButton.setClickingTogglesState(true);

    highPassSlopeBox.addItem("12 dB/oct", 1);
    highPassSlopeBox.addItem("24 dB/oct", 2);
    addAndMakeVisible(highPassSlopeBox);
    addAndMakeVisible(highPassSlopeLabel);
    highPassSlopeLabel.setText("Slope", juce::dontSendNotification);

    addAndMakeVisible(highPassButton);
    highPassButton.setButtonText({});

    addAndMakeVisible(saturationButton);
    saturationButton.setButtonText({});

    addSlider(tiltSlider, tiltLabel, "Tilt");
    addSlider(strengthSlider, strengthLabel, "Strength");
    addSlider(mixSlider, mixLabel, "Mix");
    addSlider(lowFrequencySlider, lowFrequencyLabel, "Low");
    addSlider(highFrequencySlider, highFrequencyLabel, "High");
    addSlider(maxCorrectionSlider, maxCorrectionLabel, "Correction");
    addSlider(speedSlider, speedLabel, "Speed");
    addSlider(highPassFrequencySlider, highPassFrequencyLabel, "Cutoff");
    addSlider(saturationFrequencySlider, saturationFrequencyLabel, "Crossover");
    addSlider(saturationDriveSlider, saturationDriveLabel, "Drive");
    addSlider(saturationMixSlider, saturationMixLabel, "Amount");
    addAndMakeVisible(tiltPreview);

    for (auto* label : { &curveLabel, &oversamplingLabel, &highPassSlopeLabel })
    {
        label->setColour(juce::Label::textColourId, juce::Colour(mutedTextColour));
        label->setFont(juce::FontOptions(10.0f, juce::Font::bold));
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

    // Attachments install the parameter's default text conversion, so apply the
    // compact UI formatting after the attachments have been created.
    mixSlider.textFromValueFunction = ParameterFormatting::roundedPercent01;
    mixSlider.valueFromTextFunction = ParameterFormatting::percent01FromText;

    for (auto* slider : { &strengthSlider, &speedSlider, &saturationDriveSlider, &saturationMixSlider })
    {
        slider->textFromValueFunction = ParameterFormatting::percent01;
        slider->valueFromTextFunction = ParameterFormatting::percent01FromText;
    }

    tiltSlider.textFromValueFunction = ParameterFormatting::tiltPercent;
    tiltSlider.valueFromTextFunction = ParameterFormatting::numberFromText;
    maxCorrectionSlider.textFromValueFunction = ParameterFormatting::decibels;
    maxCorrectionSlider.valueFromTextFunction = ParameterFormatting::numberFromText;

    for (auto* slider : { static_cast<juce::Slider*>(&lowFrequencySlider),
                          static_cast<juce::Slider*>(&highFrequencySlider),
                          &highPassFrequencySlider,
                          &saturationFrequencySlider })
    {
        slider->textFromValueFunction = ParameterFormatting::frequency;
        slider->valueFromTextFunction = ParameterFormatting::frequencyFromText;
    }

    for (auto* slider : { &tiltSlider, &strengthSlider, &mixSlider, static_cast<juce::Slider*>(&lowFrequencySlider),
                          static_cast<juce::Slider*>(&highFrequencySlider), &maxCorrectionSlider, &speedSlider,
                          &highPassFrequencySlider, &saturationFrequencySlider, &saturationDriveSlider,
                          &saturationMixSlider })
        slider->updateText();

    updateSectionState();
    startTimerHz(12);
    setSize(980, 560);
}

BrownHelpEditor::~BrownHelpEditor()
{
    setLookAndFeel(nullptr);
}

void BrownHelpEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(backgroundColour));
    graphics.setColour(juce::Colour(accentColour));
    graphics.fillRect(18, 18, 3, 25);

    graphics.setColour(juce::Colour(textColour));
    graphics.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    graphics.drawText("BROWN HELP", 30, 15, 150, 24, juce::Justification::centredLeft);

    graphics.setColour(juce::Colour(mutedTextColour));
    graphics.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    graphics.drawText("D35P AUDIO  /  ADAPTIVE TONE SHAPER", 177, 17, 260, 22, juce::Justification::centredLeft);

    graphics.setColour(juce::Colour(outlineColour));
    graphics.drawHorizontalLine(59, 16.0f, static_cast<float>(getWidth() - 16));

    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(44);
    area.removeFromTop(8);
    area.removeFromTop(188);
    area.removeFromTop(10);
    auto deck = area.removeFromTop(278);

    graphics.setColour(juce::Colour(panelColour));
    graphics.fillRoundedRectangle(deck.toFloat(), 2.0f);
    graphics.setColour(juce::Colour(outlineColour));
    graphics.drawRoundedRectangle(deck.toFloat().reduced(0.5f), 2.0f, 1.0f);

    auto sections = deck;
    auto shapeSection = sections.removeFromLeft(400);
    auto rangeSection = sections.removeFromLeft(220);
    auto utilitySection = sections;

    graphics.setColour(juce::Colour(outlineColour));
    graphics.drawVerticalLine(shapeSection.getRight(), static_cast<float>(deck.getY() + 12), static_cast<float>(deck.getBottom() - 12));
    graphics.drawVerticalLine(rangeSection.getRight(), static_cast<float>(deck.getY() + 12), static_cast<float>(deck.getBottom() - 12));

    drawSectionTitle(graphics, shapeSection, "SHAPE");
    drawSectionTitle(graphics, rangeSection, "RANGE / BLEND");

    auto highPassSection = utilitySection.removeFromTop(130);
    auto saturationSection = utilitySection;
    drawSectionTitle(graphics, highPassSection, "FILTER");
    drawSectionTitle(graphics, saturationSection, "COLOR");

    graphics.setColour(juce::Colour(outlineColour));
    graphics.drawHorizontalLine(
        highPassSection.getBottom(), static_cast<float>(highPassSection.getX() + 12), static_cast<float>(highPassSection.getRight() - 12));

    if (parameter(audioProcessor.getParameters(), bypassId) >= 0.5f)
    {
        auto content = getLocalBounds().reduced(16);
        content.removeFromTop(52);
        graphics.setColour(juce::Colour(backgroundColour).withAlpha(0.72f));
        graphics.fillRect(content);
        graphics.setColour(juce::Colour(mutedTextColour));
        graphics.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        graphics.drawText("BYPASSED", content, juce::Justification::centred);
    }

    if (showHelpPanel)
    {
        auto helpBounds = getLocalBounds().withSizeKeepingCentre(560, 250);
        graphics.setColour(juce::Colours::black.withAlpha(0.62f));
        graphics.fillRect(getLocalBounds());
        graphics.setColour(juce::Colour(panelColour));
        graphics.fillRoundedRectangle(helpBounds.toFloat(), 2.0f);
        graphics.setColour(juce::Colour(outlineColour));
        graphics.drawRoundedRectangle(helpBounds.toFloat().reduced(0.5f), 2.0f, 1.0f);
        graphics.setColour(juce::Colour(accentColour));
        graphics.fillRect(helpBounds.getX(), helpBounds.getY(), 3, helpBounds.getHeight());

        helpBounds.reduce(26, 20);
        graphics.setColour(juce::Colour(textColour));
        graphics.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        graphics.drawText("SIGNAL FLOW", helpBounds.removeFromTop(24), juce::Justification::centredLeft);

        graphics.setColour(juce::Colour(mutedTextColour));
        graphics.setFont(juce::FontOptions(12.0f));
        graphics.drawFittedText(
            "FILTER  >  ADAPTIVE CORRECTION  >  COLOR  >  OUTPUT GUARD\n\n"
            "The amber line is the target. Teal shows the correction being applied.\n"
            "Low and High limit analysis; they do not remove audio.\n"
            "Filter is the actual low cut. Color saturates only the upper band.\n"
            "Sat Quality controls oversampling for the nonlinear stage.",
            helpBounds,
            juce::Justification::topLeft,
            8);
    }
}

void BrownHelpEditor::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto header = area.removeFromTop(44);
    auto headerRight = header.removeFromRight(334);
    layoutCombo(oversamplingBox, oversamplingLabel, headerRight.removeFromLeft(150).withTrimmedTop(1).withHeight(40));
    headerRight.removeFromLeft(12);
    bypassButton.setBounds(headerRight.removeFromLeft(88).withTrimmedTop(9).withHeight(25));
    headerRight.removeFromLeft(10);
    helpButton.setBounds(headerRight.removeFromLeft(28).withTrimmedTop(9).withHeight(25));

    area.removeFromTop(8);
    tiltPreview.setBounds(area.removeFromTop(188));
    area.removeFromTop(10);

    auto deck = area.removeFromTop(278);
    auto shapeSection = deck.removeFromLeft(400).reduced(14);
    auto rangeSection = deck.removeFromLeft(220).reduced(14);
    auto utilitySection = deck;

    shapeSection.removeFromTop(30);
    auto curveRow = shapeSection.removeFromTop(42);
    auto flipArea = curveRow.removeFromRight(54);
    layoutCombo(curveBox, curveLabel, curveRow);
    tiltFlipButton.setBounds(flipArea.withTrimmedTop(15).withHeight(24));
    shapeSection.removeFromTop(4);
    layoutHorizontalSlider(tiltSlider, tiltLabel, shapeSection.removeFromTop(43));
    layoutHorizontalSlider(strengthSlider, strengthLabel, shapeSection.removeFromTop(43));
    layoutHorizontalSlider(maxCorrectionSlider, maxCorrectionLabel, shapeSection.removeFromTop(43));
    layoutHorizontalSlider(speedSlider, speedLabel, shapeSection.removeFromTop(43));

    rangeSection.removeFromTop(30);
    layoutSlider(mixSlider, mixLabel, rangeSection.removeFromTop(112).withSizeKeepingCentre(112, 112));
    rangeSection.removeFromTop(9);
    layoutInlineSlider(lowFrequencySlider, lowFrequencyLabel, rangeSection.removeFromTop(36));
    rangeSection.removeFromTop(8);
    layoutInlineSlider(highFrequencySlider, highFrequencyLabel, rangeSection.removeFromTop(36));

    auto highPassSection = utilitySection.removeFromTop(130);
    highPassButton.setBounds(highPassSection.getRight() - 46, highPassSection.getY() + 9, 34, 18);
    auto highPassControls = highPassSection.reduced(14);
    highPassControls.removeFromTop(30);
    layoutHorizontalSlider(highPassFrequencySlider, highPassFrequencyLabel, highPassControls.removeFromTop(40));
    layoutCombo(highPassSlopeBox, highPassSlopeLabel, highPassControls.removeFromTop(43));

    auto saturationSection = utilitySection;
    saturationButton.setBounds(saturationSection.getRight() - 46, saturationSection.getY() + 9, 34, 18);
    auto saturationControls = saturationSection.reduced(14);
    saturationControls.removeFromTop(30);
    layoutHorizontalSlider(saturationFrequencySlider, saturationFrequencyLabel, saturationControls.removeFromTop(30));
    layoutHorizontalSlider(saturationDriveSlider, saturationDriveLabel, saturationControls.removeFromTop(30));
    layoutHorizontalSlider(saturationMixSlider, saturationMixLabel, saturationControls.removeFromTop(30));
}

void BrownHelpEditor::addSlider(juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    addAndMakeVisible(slider);
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 16);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(accentColour));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(textColour));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(textColour));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::trackColourId, juce::Colour(accentColour));
    slider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff3a4043));

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, juce::Colour(mutedTextColour));
    label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
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

void BrownHelpEditor::drawSectionTitle(juce::Graphics& graphics,
                                      juce::Rectangle<int> bounds,
                                      const juce::String& title)
{
    graphics.setColour(juce::Colour(mutedTextColour));
    graphics.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    graphics.drawText(title, bounds.getX() + 14, bounds.getY() + 9, bounds.getWidth() - 28, 18, juce::Justification::centredLeft);
    graphics.setColour(juce::Colour(accentColour));
    graphics.fillRect(bounds.getX() + 14, bounds.getY() + 29, 22, 1);
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

    bypassButton.setColour(juce::TextButton::buttonColourId, bypassed ? juce::Colour(accentColour) : juce::Colour(plotColour));
    bypassButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(accentColour));
    bypassButton.setColour(juce::TextButton::textColourOffId, bypassed ? juce::Colour(backgroundColour) : juce::Colour(textColour));
    bypassButton.setColour(juce::TextButton::textColourOnId, juce::Colour(backgroundColour));

    helpButton.setColour(juce::TextButton::buttonColourId, juce::Colour(plotColour));
    helpButton.setColour(juce::TextButton::textColourOffId, juce::Colour(textColour));

    const auto tiltFlipped = parameter(audioProcessor.getParameters(), tiltFlipId) >= 0.5f;
    tiltFlipButton.setColour(juce::TextButton::buttonColourId, tiltFlipped ? juce::Colour(accentColour) : juce::Colour(plotColour));
    tiltFlipButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(accentColour));
    tiltFlipButton.setColour(juce::TextButton::textColourOffId, tiltFlipped ? juce::Colour(backgroundColour) : juce::Colour(textColour));
    tiltFlipButton.setColour(juce::TextButton::textColourOnId, juce::Colour(backgroundColour));
}

void BrownHelpEditor::timerCallback()
{
    updateSectionState();
    repaint();
}
}
