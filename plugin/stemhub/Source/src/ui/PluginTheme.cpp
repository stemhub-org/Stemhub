#include "ui/PluginTheme.hpp"

namespace stemhub::plugin::theme
{
StemhubPluginLookAndFeel::StemhubPluginLookAndFeel()
{
    setDefaultSansSerifTypefaceName("Syne");
}

void StemhubPluginLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                                    juce::Button& button,
                                                    const juce::Colour& backgroundColour,
                                                    bool shouldDrawButtonAsHighlighted,
                                                    bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto fill = backgroundColour;

    if (!button.isEnabled())
        fill = fill.darker(0.2f).withMultipliedAlpha(0.45f);
    else if (shouldDrawButtonAsDown)
        fill = fill.brighter(0.08f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter(0.04f);

    const auto cornerSize = juce::jmin(PluginTheme::buttonRadius,
                                       bounds.getHeight() * 0.48f,
                                       bounds.getWidth() * 0.48f);
    const auto outline = button.hasKeyboardFocus(true)
        ? PluginTheme::kAccent
        : fill.contrasting(0.18f).withAlpha(button.isEnabled() ? 0.7f : 0.25f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, cornerSize);

    g.setColour(outline);
    g.drawRoundedRectangle(bounds, cornerSize, button.hasKeyboardFocus(true) ? 1.6f : 1.0f);
}

void stylePrimaryButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, PluginTheme::kAccent);
    button.setColour(juce::TextButton::buttonOnColourId, PluginTheme::kAccentHover);
    button.setColour(juce::TextButton::textColourOffId, PluginTheme::kForeground);
    button.setColour(juce::TextButton::textColourOnId, PluginTheme::kForeground);
    button.setSize(0, static_cast<int>(PluginTheme::controlHeight));
    button.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void styleSecondaryButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, PluginTheme::kSurfaceSoft.withAlpha(0.95f));
    button.setColour(juce::TextButton::buttonOnColourId, PluginTheme::kSurfaceSoft.brighter(0.1f));
    button.setColour(juce::TextButton::textColourOffId, PluginTheme::kForeground);
    button.setColour(juce::TextButton::textColourOnId, PluginTheme::kForeground);
    button.setSize(0, static_cast<int>(PluginTheme::controlHeight));
    button.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void styleGhostButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, PluginTheme::kBackground.withAlpha(0.6f));
    button.setColour(juce::TextButton::buttonOnColourId, PluginTheme::kSurface);
    button.setColour(juce::TextButton::textColourOffId, PluginTheme::kForegroundSubtle);
    button.setColour(juce::TextButton::textColourOnId, PluginTheme::kForeground);
    button.setSize(0, static_cast<int>(PluginTheme::controlHeight));
    button.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void styleTextInput(juce::TextEditor& input, const juce::String& emptyText)
{
    input.setMultiLine(false);
    input.setFont(bodyFont(13.0f));
    input.setTextToShowWhenEmpty(emptyText, PluginTheme::kForegroundSubtle);
    input.setColour(juce::TextEditor::backgroundColourId, PluginTheme::kSurface);
    input.setColour(juce::TextEditor::textColourId, PluginTheme::kForeground);
    input.setColour(juce::TextEditor::outlineColourId, PluginTheme::kSurfaceBorder);
    input.setColour(juce::TextEditor::focusedOutlineColourId, PluginTheme::kAccent);
    input.setColour(juce::CaretComponent::caretColourId, PluginTheme::kAccent);
}

void styleComboBox(juce::ComboBox& combo)
{
    combo.setColour(juce::ComboBox::backgroundColourId, PluginTheme::kSurface);
    combo.setColour(juce::ComboBox::textColourId, PluginTheme::kForeground);
    combo.setColour(juce::ComboBox::outlineColourId, PluginTheme::kSurfaceBorder);
    combo.setColour(juce::ComboBox::arrowColourId, PluginTheme::kForegroundSubtle);
    combo.setColour(juce::ComboBox::focusedOutlineColourId, PluginTheme::kAccent);
}

void styleStatusLabel(juce::Label& label, const juce::String& message, MessageStatus severity)
{
    const auto border = statusText(severity).withAlpha(0.85f);
    label.setText(message, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::backgroundColourId, statusBg(severity));
    label.setColour(juce::Label::textColourId, statusText(severity));
    label.setColour(juce::Label::outlineColourId, border);
    label.setColour(juce::Label::textWhenEditingColourId, statusText(severity));
    label.setFont(bodyFont(13.0f, juce::Font::plain));
}

void styleInfoLabel(juce::Label& label,
                    const juce::String& text,
                    bool emphasized,
                    bool muted)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(emphasized ? bodyFont(12.0f, juce::Font::bold) : bodyFont(12.0f));
    label.setColour(juce::Label::textColourId, muted
        ? PluginTheme::kForegroundSubtle
        : PluginTheme::kForeground);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::outlineColourId, juce::Colours::transparentWhite);
    label.setColour(juce::Label::backgroundColourId, juce::Colours::transparentWhite);
}

void paintSurface(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    g.setColour(PluginTheme::kSurface);
    g.fillRoundedRectangle(bounds, PluginTheme::panelRadius);
    g.setColour(PluginTheme::kSurfaceBorder);
    g.drawRoundedRectangle(bounds, PluginTheme::panelRadius, 1.0f);
}
}
