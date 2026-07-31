#include "BrownHelpEditor.h"

#include "ParameterFormatting.h"
#include "Parameters.h"

namespace BrownHelp
{
using namespace Ui;

BrownHelpEditor::BrownHelpEditor(BrownHelpProcessor& processorToUse)
    : AudioProcessorEditor(&processorToUse),
      audioProcessor(processorToUse),
      spectrum(processorToUse)
{
    setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(spectrum);

    addAndMakeVisible(bypassButton);
    bypassButton.setButtonText("BYPASS");
    bypassButton.setClickingTogglesState(true);

    addAndMakeVisible(resetButton);
    resetButton.setButtonText("RESET LEARN");
    resetButton.onClick = [this] { audioProcessor.resetLearning(); };

    for (auto* button : { &autoBalanceButton, &manualFundamentalButton, &lowShelfButton, &highShelfButton })
    {
        addAndMakeVisible(button);
        button->setButtonText({});
    }

    addAndMakeVisible(autoBalanceLabel);
    autoBalanceLabel.setText("AUTO F0 / S BALANCE", juce::dontSendNotification);
    autoBalanceLabel.setColour(juce::Label::textColourId, juce::Colour(textColour));
    autoBalanceLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));

    addAndMakeVisible(manualFundamentalLabel);
    manualFundamentalLabel.setText("MANUAL F0", juce::dontSendNotification);
    manualFundamentalLabel.setColour(juce::Label::textColourId, juce::Colour(textColour));
    manualFundamentalLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));

    configureSlider(manualFundamentalSlider, manualFundamentalFrequencyLabel, "Fundamental");
    configureSlider(lowFrequencySlider, lowFrequencyLabel, "Frequency");
    configureSlider(lowReductionSlider, lowReductionLabel, "Reduction");
    configureSlider(highFrequencySlider, highFrequencyLabel, "Frequency");
    configureSlider(highReductionSlider, highReductionLabel, "Reduction");

    for (auto* box : { &lowSlopeBox, &highSlopeBox })
    {
        box->addItem("12 dB/oct", 1);
        box->addItem("18 dB/oct", 2);
        addAndMakeVisible(box);
    }

    for (auto* label : { &lowSlopeLabel, &highSlopeLabel })
    {
        label->setText("Slope", juce::dontSendNotification);
        label->setColour(juce::Label::textColourId, juce::Colour(mutedTextColour));
        label->setFont(juce::FontOptions(10.0f, juce::Font::bold));
    }
    addAndMakeVisible(lowSlopeLabel);
    addAndMakeVisible(highSlopeLabel);

    bypassAttachment = std::make_unique<ButtonAttachment>(audioProcessor.getParameters(), bypassId, bypassButton);
    autoBalanceAttachment = std::make_unique<ButtonAttachment>(audioProcessor.getParameters(), autoBalanceId, autoBalanceButton);
    manualFundamentalAttachment = std::make_unique<ButtonAttachment>(
        audioProcessor.getParameters(), manualFundamentalEnabledId, manualFundamentalButton);
    lowShelfAttachment = std::make_unique<ButtonAttachment>(audioProcessor.getParameters(), lowShelfEnabledId, lowShelfButton);
    highShelfAttachment = std::make_unique<ButtonAttachment>(audioProcessor.getParameters(), highShelfEnabledId, highShelfButton);
    lowFrequencyAttachment = std::make_unique<SliderAttachment>(audioProcessor.getParameters(), lowShelfFrequencyId, lowFrequencySlider);
    manualFundamentalFrequencyAttachment = std::make_unique<SliderAttachment>(
        audioProcessor.getParameters(), manualFundamentalFrequencyId, manualFundamentalSlider);
    lowReductionAttachment = std::make_unique<SliderAttachment>(audioProcessor.getParameters(), lowShelfReductionId, lowReductionSlider);
    highFrequencyAttachment = std::make_unique<SliderAttachment>(audioProcessor.getParameters(), highShelfFrequencyId, highFrequencySlider);
    highReductionAttachment = std::make_unique<SliderAttachment>(audioProcessor.getParameters(), highShelfReductionId, highReductionSlider);
    lowSlopeAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.getParameters(), lowShelfSlopeId, lowSlopeBox);
    highSlopeAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.getParameters(), highShelfSlopeId, highSlopeBox);

    for (auto* slider : { &manualFundamentalSlider, &lowFrequencySlider, &highFrequencySlider })
    {
        slider->textFromValueFunction = ParameterFormatting::frequency;
        slider->valueFromTextFunction = ParameterFormatting::frequencyFromText;
    }

    for (auto* slider : { &lowReductionSlider, &highReductionSlider })
    {
        slider->textFromValueFunction = [](double value)
        {
            return (value < 0.05 ? juce::String("0.0") : juce::String(-value, 1)) + " dB";
        };
        slider->valueFromTextFunction = [](const juce::String& text)
        {
            return std::abs(text.retainCharacters("-0123456789.").getDoubleValue());
        };
    }

    for (auto* slider : { &manualFundamentalSlider, &lowFrequencySlider, &lowReductionSlider,
                          &highFrequencySlider, &highReductionSlider })
        slider->updateText();

    startTimerHz(12);
    setSize(980, 650);
    updateState();
}

BrownHelpEditor::~BrownHelpEditor()
{
    setLookAndFeel(nullptr);
}

void BrownHelpEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(backgroundColour));
    graphics.setColour(juce::Colour(accentColour));
    graphics.fillRect(18, 17, 3, 27);
    graphics.setColour(juce::Colour(textColour));
    graphics.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    graphics.drawText("BROWN HELP", 30, 15, 150, 25, juce::Justification::centredLeft);
    graphics.setColour(juce::Colour(mutedTextColour));
    graphics.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    graphics.drawText("D35P AUDIO  /  VOICE LEVEL ENGINEER", 177, 18, 300, 22, juce::Justification::centredLeft);
    graphics.setColour(juce::Colour(outlineColour));
    graphics.drawHorizontalLine(58, 16.0f, static_cast<float>(getWidth() - 16));

    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(52);
    area.removeFromTop(330);
    area.removeFromTop(10);
    auto deck = area.removeFromTop(226);
    auto autoPanel = deck.removeFromLeft(280);
    auto lowPanel = deck.removeFromLeft(342);
    auto highPanel = deck;
    drawPanel(graphics, autoPanel, "ENGINEER");
    drawPanel(graphics, lowPanel, "OPTIONAL LOW SHELF");
    drawPanel(graphics, highPanel, "OPTIONAL HIGH SHELF");

    graphics.setColour(juce::Colour(outlineColour));
    graphics.drawVerticalLine(autoPanel.getRight(), static_cast<float>(autoPanel.getY() + 12), static_cast<float>(autoPanel.getBottom() - 12));
    graphics.drawVerticalLine(lowPanel.getRight(), static_cast<float>(lowPanel.getY() + 12), static_cast<float>(lowPanel.getBottom() - 12));

    auto info = autoPanel.reduced(14);
    info.removeFromTop(130);
    graphics.setColour(juce::Colour(mutedTextColour));
    graphics.setFont(juce::FontOptions(10.0f));
    graphics.drawFittedText("TARGET  -14 LUFS\nCEILING  0 dBFS\nBANDS  <= -40 dB RMS",
                            info, juce::Justification::topLeft, 3);
}

void BrownHelpEditor::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto header = area.removeFromTop(44);
    auto controls = header.removeFromRight(205);
    resetButton.setBounds(controls.removeFromLeft(104).withTrimmedTop(8).withHeight(26));
    controls.removeFromLeft(9);
    bypassButton.setBounds(controls.removeFromLeft(88).withTrimmedTop(8).withHeight(26));

    area.removeFromTop(8);
    spectrum.setBounds(area.removeFromTop(330));
    area.removeFromTop(10);
    auto deck = area.removeFromTop(226);
    auto autoPanel = deck.removeFromLeft(280).reduced(14);
    auto lowPanel = deck.removeFromLeft(342).reduced(14);
    auto highPanel = deck.reduced(14);

    autoPanel.removeFromTop(32);
    auto autoRow = autoPanel.removeFromTop(25);
    autoBalanceLabel.setBounds(autoRow.removeFromLeft(190));
    autoBalanceButton.setBounds(autoRow.withSizeKeepingCentre(34, 18));
    auto manualRow = autoPanel.removeFromTop(25);
    manualFundamentalLabel.setBounds(manualRow.removeFromLeft(190));
    manualFundamentalButton.setBounds(manualRow.withSizeKeepingCentre(34, 18));
    layoutSlider(manualFundamentalSlider, manualFundamentalFrequencyLabel, autoPanel.removeFromTop(43));

    lowShelfButton.setBounds(lowPanel.getRight() - 38, lowPanel.getY() - 5, 34, 18);
    lowPanel.removeFromTop(30);
    layoutSlider(lowFrequencySlider, lowFrequencyLabel, lowPanel.removeFromTop(43));
    layoutSlider(lowReductionSlider, lowReductionLabel, lowPanel.removeFromTop(43));
    layoutCombo(lowSlopeBox, lowSlopeLabel, lowPanel.removeFromTop(43));

    highShelfButton.setBounds(highPanel.getRight() - 38, highPanel.getY() - 5, 34, 18);
    highPanel.removeFromTop(30);
    layoutSlider(highFrequencySlider, highFrequencyLabel, highPanel.removeFromTop(43));
    layoutSlider(highReductionSlider, highReductionLabel, highPanel.removeFromTop(43));
    layoutCombo(highSlopeBox, highSlopeLabel, highPanel.removeFromTop(43));
}

void BrownHelpEditor::configureSlider(juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 76, 18);
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colour(mutedTextColour));
    label.setFont(juce::FontOptions(10.0f, juce::Font::bold));
}

void BrownHelpEditor::layoutSlider(juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds)
{
    label.setBounds(bounds.removeFromTop(16));
    slider.setBounds(bounds);
}

void BrownHelpEditor::layoutCombo(juce::ComboBox& combo, juce::Label& label, juce::Rectangle<int> bounds)
{
    label.setBounds(bounds.removeFromLeft(75));
    combo.setBounds(bounds.withSizeKeepingCentre(std::min(145, bounds.getWidth()), 25));
}

void BrownHelpEditor::drawPanel(juce::Graphics& graphics, juce::Rectangle<int> bounds, const juce::String& title)
{
    graphics.setColour(juce::Colour(panelColour));
    graphics.fillRoundedRectangle(bounds.toFloat(), 2.0f);
    graphics.setColour(juce::Colour(outlineColour));
    graphics.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 2.0f, 1.0f);
    graphics.setColour(juce::Colour(mutedTextColour));
    graphics.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    graphics.drawText(title, bounds.getX() + 14, bounds.getY() + 9, bounds.getWidth() - 62, 18,
                      juce::Justification::centredLeft);
    graphics.setColour(juce::Colour(accentColour));
    graphics.fillRect(bounds.getX() + 14, bounds.getY() + 29, 22, 1);
}

void BrownHelpEditor::updateState()
{
    const auto lowEnabled = parameter(audioProcessor.getParameters(), lowShelfEnabledId) >= 0.5f;
    const auto highEnabled = parameter(audioProcessor.getParameters(), highShelfEnabledId) >= 0.5f;
    const auto manualFundamentalEnabled = parameter(audioProcessor.getParameters(), manualFundamentalEnabledId) >= 0.5f;
    const auto bypassed = parameter(audioProcessor.getParameters(), bypassId) >= 0.5f;

    for (auto* component : { static_cast<juce::Component*>(&lowFrequencySlider), static_cast<juce::Component*>(&lowFrequencyLabel),
                             static_cast<juce::Component*>(&lowReductionSlider), static_cast<juce::Component*>(&lowReductionLabel),
                             static_cast<juce::Component*>(&lowSlopeBox), static_cast<juce::Component*>(&lowSlopeLabel) })
        component->setAlpha(lowEnabled && ! bypassed ? 1.0f : 0.35f);

    for (auto* component : { static_cast<juce::Component*>(&highFrequencySlider), static_cast<juce::Component*>(&highFrequencyLabel),
                             static_cast<juce::Component*>(&highReductionSlider), static_cast<juce::Component*>(&highReductionLabel),
                             static_cast<juce::Component*>(&highSlopeBox), static_cast<juce::Component*>(&highSlopeLabel) })
        component->setAlpha(highEnabled && ! bypassed ? 1.0f : 0.35f);

    for (auto* component : { static_cast<juce::Component*>(&spectrum), static_cast<juce::Component*>(&autoBalanceButton),
                             static_cast<juce::Component*>(&autoBalanceLabel), static_cast<juce::Component*>(&lowShelfButton),
                             static_cast<juce::Component*>(&highShelfButton), static_cast<juce::Component*>(&resetButton),
                             static_cast<juce::Component*>(&manualFundamentalButton),
                             static_cast<juce::Component*>(&manualFundamentalLabel) })
        component->setAlpha(bypassed ? 0.35f : 1.0f);

    for (auto* component : { static_cast<juce::Component*>(&manualFundamentalSlider),
                             static_cast<juce::Component*>(&manualFundamentalFrequencyLabel) })
        component->setAlpha(manualFundamentalEnabled && ! bypassed ? 1.0f : 0.35f);

    bypassButton.setColour(juce::TextButton::buttonColourId, bypassed ? juce::Colour(accentColour) : juce::Colour(plotColour));
    bypassButton.setColour(juce::TextButton::textColourOffId, bypassed ? juce::Colour(backgroundColour) : juce::Colour(textColour));
    resetButton.setColour(juce::TextButton::buttonColourId, juce::Colour(plotColour));
    resetButton.setColour(juce::TextButton::textColourOffId, juce::Colour(textColour));
}

void BrownHelpEditor::timerCallback()
{
    updateState();
}
}
