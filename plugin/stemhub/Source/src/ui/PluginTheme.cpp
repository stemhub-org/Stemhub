#include "ui/PluginTheme.hpp"

namespace stemhub::plugin::theme
{
StemhubPluginLookAndFeel::StemhubPluginLookAndFeel()
{
    // TODO: Load Inter + JetBrains Mono from BinaryData in a follow-up pass.
    setDefaultSansSerifTypefaceName("Inter");
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
        fill = fill.brighter(0.07f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter(0.03f);

    const auto cornerSize = juce::jmin(PluginTheme::buttonRadius, bounds.getHeight() * 0.48f, bounds.getWidth() * 0.48f);
    const auto outline = button.hasKeyboardFocus(true)
                            ? PluginTheme::kAccent
                            : fill.contrasting(0.18f).withAlpha(button.isEnabled() ? 0.7f : 0.22f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, cornerSize);

    if ((shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown || button.isMouseOver()) && button.isEnabled())
    {
        g.setColour(PluginTheme::kAccentGlow);
        g.fillRoundedRectangle(bounds.expanded(1.0f), cornerSize + 0.6f);
    }

    g.setColour(outline);
    g.drawRoundedRectangle(bounds, cornerSize, button.hasKeyboardFocus(true) ? 1.4f : 0.95f);
}

juce::Font StemhubPluginLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return bodyFont(juce::jlimit(11.5f, 14.0f, static_cast<float>(buttonHeight) * 0.36f + 0.8f),
                    juce::Font::bold);
}

juce::Font StemhubPluginLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return bodyFont(12.5f, juce::Font::bold);
}

void stylePrimaryButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, PluginTheme::kAccent);
    button.setColour(juce::TextButton::buttonOnColourId, PluginTheme::kAccentHover);
    button.setColour(juce::TextButton::textColourOffId, PluginTheme::kBackground);
    button.setColour(juce::TextButton::textColourOnId, PluginTheme::kBackground);
    button.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    button.setSize(0, static_cast<int>(PluginTheme::controlHeight));
}

void styleSecondaryButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, PluginTheme::kSurfaceElevated);
    button.setColour(juce::TextButton::buttonOnColourId, PluginTheme::kActive);
    button.setColour(juce::TextButton::textColourOffId, PluginTheme::kForeground);
    button.setColour(juce::TextButton::textColourOnId, PluginTheme::kForeground);
    button.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    button.setSize(0, static_cast<int>(PluginTheme::controlHeight));
}

void styleGhostButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, PluginTheme::kBackground.withAlpha(0.02f));
    button.setColour(juce::TextButton::buttonOnColourId, PluginTheme::kHover);
    button.setColour(juce::TextButton::textColourOffId, PluginTheme::kForegroundSubtle);
    button.setColour(juce::TextButton::textColourOnId, PluginTheme::kForeground);
    button.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    button.setSize(0, static_cast<int>(PluginTheme::controlHeight));
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
    input.setSize(0, static_cast<int>(PluginTheme::controlHeight));
}

void styleComboBox(juce::ComboBox& combo)
{
    combo.setColour(juce::ComboBox::backgroundColourId, PluginTheme::kSurface);
    combo.setColour(juce::ComboBox::textColourId, PluginTheme::kForeground);
    combo.setColour(juce::ComboBox::outlineColourId, PluginTheme::kSurfaceBorder);
    combo.setColour(juce::ComboBox::arrowColourId, PluginTheme::kForegroundSubtle);
    combo.setColour(juce::ComboBox::focusedOutlineColourId, PluginTheme::kAccent);
    combo.setColour(juce::ComboBox::buttonColourId, PluginTheme::kSurfaceElevated);
    combo.setSize(0, static_cast<int>(PluginTheme::controlHeight));
}

void styleStatusLabel(juce::Label& label, const juce::String& message, MessageStatus severity)
{
    const auto border = statusText(severity).withAlpha(0.85f);
    label.setText(message, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::backgroundColourId, statusBg(severity));
    label.setColour(juce::Label::textColourId, statusText(severity));
    label.setColour(juce::Label::outlineColourId, border);
    label.setColour(juce::Label::outlineWhenEditingColourId, border);
    label.setColour(juce::Label::textWhenEditingColourId, statusText(severity));
    label.setFont(bodyFont(11.5f, juce::Font::plain));
    label.setBorderSize(juce::BorderSize<int>(6));
}

void styleInfoLabel(juce::Label& label, const juce::String& text, bool emphasized, bool muted)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(emphasized ? bodyFont(12.0f, juce::Font::bold) : bodyFont(11.5f));
    label.setColour(juce::Label::textColourId, muted ? PluginTheme::kForegroundSubtle : PluginTheme::kForeground);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::outlineColourId, juce::Colours::transparentWhite);
    label.setColour(juce::Label::backgroundColourId, juce::Colours::transparentWhite);
}

void paintCard(juce::Graphics& g, const juce::Rectangle<float>& bounds, bool includeBorder)
{
    g.setColour(PluginTheme::kSurface);
    g.fillRoundedRectangle(bounds, PluginTheme::panelRadius);

    if (includeBorder)
    {
        g.setColour(PluginTheme::kSurfaceBorder);
        g.drawRoundedRectangle(bounds, PluginTheme::panelRadius, 1.0f);
    }
}

void paintSurface(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    paintCard(g, bounds, true);
}
}
