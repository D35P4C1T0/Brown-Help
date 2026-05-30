#pragma once

#include "BrownHelpProcessor.h"
#include "BrownHelpUiComponents.h"

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>

namespace BrownHelp
{
class BrownHelpEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit BrownHelpEditor(BrownHelpProcessor& processor);
    ~BrownHelpEditor() override = default;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void addSlider(juce::Slider& slider, juce::Label& label, const juce::String& text);
    void layoutSlider(juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds);
    void layoutHorizontalSlider(juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds);
    void layoutInlineSlider(juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds);
    void layoutCombo(juce::ComboBox& comboBox, juce::Label& label, juce::Rectangle<int> bounds);
    void drawPanel(juce::Graphics& graphics, juce::Rectangle<int> bounds, const juce::String& title);
    void configureHorizontalSlider(juce::Slider& slider);
    void configureSmallRotary(juce::Slider& slider);
    void updateSectionState();
    void timerCallback() override;

    BrownHelpProcessor& audioProcessor;
    TiltPreview tiltPreview;
    bool showHelpPanel = false;

    juce::ComboBox curveBox;
    juce::ComboBox oversamplingBox;
    juce::ComboBox highPassSlopeBox;
    juce::TextButton bypassButton;
    juce::TextButton helpButton;
    juce::TextButton tiltFlipButton;
    juce::ToggleButton highPassButton;
    juce::ToggleButton saturationButton;

    juce::Slider tiltSlider;
    juce::Slider strengthSlider;
    juce::Slider mixSlider;
    FrequencySlider lowFrequencySlider;
    FrequencySlider highFrequencySlider;
    juce::Slider maxCorrectionSlider;
    juce::Slider speedSlider;
    juce::Slider highPassFrequencySlider;
    juce::Slider saturationFrequencySlider;
    juce::Slider saturationDriveSlider;
    juce::Slider saturationMixSlider;

    juce::Label curveLabel;
    juce::Label oversamplingLabel;
    juce::Label highPassSlopeLabel;
    juce::Label tiltLabel;
    juce::Label strengthLabel;
    juce::Label mixLabel;
    juce::Label lowFrequencyLabel;
    juce::Label highFrequencyLabel;
    juce::Label maxCorrectionLabel;
    juce::Label speedLabel;
    juce::Label highPassFrequencyLabel;
    juce::Label saturationFrequencyLabel;
    juce::Label saturationDriveLabel;
    juce::Label saturationMixLabel;

    std::unique_ptr<ComboBoxAttachment> curveAttachment;
    std::unique_ptr<ComboBoxAttachment> oversamplingAttachment;
    std::unique_ptr<ComboBoxAttachment> highPassSlopeAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<ButtonAttachment> highPassAttachment;
    std::unique_ptr<ButtonAttachment> saturationAttachment;
    std::unique_ptr<ButtonAttachment> tiltFlipAttachment;
    std::unique_ptr<SliderAttachment> tiltAttachment;
    std::unique_ptr<SliderAttachment> strengthAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> lowFrequencyAttachment;
    std::unique_ptr<SliderAttachment> highFrequencyAttachment;
    std::unique_ptr<SliderAttachment> maxCorrectionAttachment;
    std::unique_ptr<SliderAttachment> speedAttachment;
    std::unique_ptr<SliderAttachment> highPassFrequencyAttachment;
    std::unique_ptr<SliderAttachment> saturationFrequencyAttachment;
    std::unique_ptr<SliderAttachment> saturationDriveAttachment;
    std::unique_ptr<SliderAttachment> saturationMixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrownHelpEditor)
};
}
