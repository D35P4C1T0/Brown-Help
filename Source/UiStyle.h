#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace BrownHelp::Ui
{
inline constexpr auto backgroundColour = 0xff101315;
inline constexpr auto panelColour = 0xff181c1f;
inline constexpr auto plotColour = 0xff0c1012;
inline constexpr auto outlineColour = 0xff343a3d;
inline constexpr auto textColour = 0xffe8e6e1;
inline constexpr auto mutedTextColour = 0xff8e9699;
inline constexpr auto accentColour = 0xffc58a52;
inline constexpr auto correctionColour = 0xff69aaa3;

class BrownHelpLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    BrownHelpLookAndFeel()
    {
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(plotColour));
        setColour(juce::ComboBox::textColourId, juce::Colour(textColour));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(outlineColour));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(mutedTextColour));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(panelColour));
        setColour(juce::PopupMenu::textColourId, juce::Colour(textColour));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(accentColour));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(backgroundColour));
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(textColour));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    }

    void drawLinearSlider(juce::Graphics& graphics,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPosition,
                          float,
                          float,
                          juce::Slider::SliderStyle style,
                          juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            LookAndFeel_V4::drawLinearSlider(
                graphics, x, y, width, height, sliderPosition, 0.0f, 0.0f, style, slider);
            return;
        }

        auto bounds = juce::Rectangle<float>(
            static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height));
        const auto centreY = bounds.getCentreY();
        const auto track = juce::Rectangle<float>(bounds.getX(), centreY - 1.0f, bounds.getWidth(), 2.0f);

        graphics.setColour(juce::Colour(0xff3a4043));
        graphics.fillRect(track);

        graphics.setColour(juce::Colour(accentColour));
        graphics.fillRect(track.withWidth(std::max(0.0f, sliderPosition - track.getX())));

        graphics.setColour(slider.isEnabled() ? juce::Colour(textColour) : juce::Colour(mutedTextColour));
        graphics.fillEllipse(sliderPosition - 4.0f, centreY - 4.0f, 8.0f, 8.0f);
    }

    void drawRotarySlider(juce::Graphics& graphics,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPosition,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float>(
            static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height));
        const auto radius = std::max(4.0f, std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f - 7.0f);
        const auto centre = bounds.getCentre();
        const auto angle = rotaryStartAngle + sliderPosition * (rotaryEndAngle - rotaryStartAngle);
        juce::Path background;
        juce::Path value;
        background.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        value.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, angle, true);

        graphics.setColour(juce::Colour(0xff373d40));
        graphics.strokePath(background, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved));
        graphics.setColour(juce::Colour(accentColour));
        graphics.strokePath(value, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved));

        const auto markerLength = radius - 5.0f;
        const auto marker = juce::Point<float>(centre.x + std::sin(angle) * markerLength,
                                               centre.y - std::cos(angle) * markerLength);
        graphics.setColour(juce::Colour(textColour));
        graphics.drawLine(centre.x, centre.y, marker.x, marker.y, 1.5f);
        graphics.fillEllipse(centre.x - 2.0f, centre.y - 2.0f, 4.0f, 4.0f);
    }

    void drawToggleButton(juce::Graphics& graphics,
                          juce::ToggleButton& button,
                          bool,
                          bool) override
    {
        auto area = button.getLocalBounds().toFloat();
        const auto switchBounds = area.withSizeKeepingCentre(28.0f, 14.0f);
        const auto isOn = button.getToggleState();

        graphics.setColour(isOn ? juce::Colour(accentColour) : juce::Colour(0xff303638));
        graphics.fillRoundedRectangle(switchBounds, 7.0f);
        graphics.setColour(isOn ? juce::Colour(0xfff3eee8) : juce::Colour(0xff858d90));
        const auto knobX = isOn ? switchBounds.getRight() - 12.0f : switchBounds.getX() + 2.0f;
        graphics.fillEllipse(knobX, switchBounds.getY() + 2.0f, 10.0f, 10.0f);
    }

    void drawComboBox(juce::Graphics& graphics,
                      int width,
                      int height,
                      bool,
                      int,
                      int,
                      int,
                      int,
                      juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        graphics.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        graphics.fillRoundedRectangle(bounds, 2.0f);
        graphics.setColour(box.findColour(juce::ComboBox::outlineColourId));
        graphics.drawRoundedRectangle(bounds.reduced(0.5f), 2.0f, 1.0f);

        const auto centreX = static_cast<float>(width - 14);
        const auto centreY = static_cast<float>(height) * 0.5f;
        juce::Path arrow;
        arrow.startNewSubPath(centreX - 4.0f, centreY - 2.0f);
        arrow.lineTo(centreX, centreY + 2.0f);
        arrow.lineTo(centreX + 4.0f, centreY - 2.0f);
        graphics.setColour(box.findColour(juce::ComboBox::arrowColourId));
        graphics.strokePath(arrow, juce::PathStrokeType(1.25f));
    }

    void drawButtonBackground(juce::Graphics& graphics,
                              juce::Button& button,
                              const juce::Colour& background,
                              bool highlighted,
                              bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto colour = background;

        if (highlighted)
            colour = colour.brighter(0.08f);

        if (down)
            colour = colour.darker(0.12f);

        graphics.setColour(colour);
        graphics.fillRoundedRectangle(bounds, 2.0f);
        graphics.setColour(button.getToggleState() ? juce::Colour(accentColour) : juce::Colour(outlineColour));
        graphics.drawRoundedRectangle(bounds.reduced(0.5f), 2.0f, 1.0f);
    }
};

inline float parameter(const juce::AudioProcessorValueTreeState& parameters, const char* id)
{
    return parameters.getRawParameterValue(id)->load();
}
}
