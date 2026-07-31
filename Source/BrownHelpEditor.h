#pragma once

#include "BrownHelpProcessor.h"
#include "BrownHelpUiComponents.h"
#include "UiStyle.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace BrownHelp
{
class BrownHelpEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit BrownHelpEditor(BrownHelpProcessor& processor);
    ~BrownHelpEditor() override;
    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void configureSlider(juce::Slider& slider, juce::Label& label, const juce::String& text);
    void layoutSlider(juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds);
    void layoutCombo(juce::ComboBox& combo, juce::Label& label, juce::Rectangle<int> bounds);
    void drawPanel(juce::Graphics& graphics, juce::Rectangle<int> bounds, const juce::String& title);
    void updateState();
    void timerCallback() override;

    Ui::BrownHelpLookAndFeel lookAndFeel;
    BrownHelpProcessor& audioProcessor;
    SpectrumAnalyzer spectrum;

    juce::TextButton bypassButton;
    juce::TextButton resetButton;
    juce::ToggleButton autoBalanceButton;
    juce::ToggleButton manualFundamentalButton;
    juce::ToggleButton lowShelfButton;
    juce::ToggleButton highShelfButton;
    juce::Slider lowFrequencySlider;
    juce::Slider manualFundamentalSlider;
    juce::Slider lowReductionSlider;
    juce::Slider highFrequencySlider;
    juce::Slider highReductionSlider;
    juce::ComboBox lowSlopeBox;
    juce::ComboBox highSlopeBox;
    juce::Label autoBalanceLabel;
    juce::Label manualFundamentalLabel;
    juce::Label manualFundamentalFrequencyLabel;
    juce::Label lowFrequencyLabel;
    juce::Label lowReductionLabel;
    juce::Label lowSlopeLabel;
    juce::Label highFrequencyLabel;
    juce::Label highReductionLabel;
    juce::Label highSlopeLabel;

    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<ButtonAttachment> autoBalanceAttachment;
    std::unique_ptr<ButtonAttachment> manualFundamentalAttachment;
    std::unique_ptr<ButtonAttachment> lowShelfAttachment;
    std::unique_ptr<ButtonAttachment> highShelfAttachment;
    std::unique_ptr<SliderAttachment> lowFrequencyAttachment;
    std::unique_ptr<SliderAttachment> manualFundamentalFrequencyAttachment;
    std::unique_ptr<SliderAttachment> lowReductionAttachment;
    std::unique_ptr<SliderAttachment> highFrequencyAttachment;
    std::unique_ptr<SliderAttachment> highReductionAttachment;
    std::unique_ptr<ComboBoxAttachment> lowSlopeAttachment;
    std::unique_ptr<ComboBoxAttachment> highSlopeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrownHelpEditor)
};
}
