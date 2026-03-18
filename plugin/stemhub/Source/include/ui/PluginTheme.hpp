#pragma once

#include <JuceHeader.h>

namespace stemhub::plugin::theme
{
enum class MessageStatus
{
    neutral,
    loading,
    success,
    warning,
    error,
    disabled
};

struct PluginTheme
{
    static constexpr float panelRadius = 12.0f;
    static constexpr float buttonRadius = 12.0f;
    static constexpr int labelHeight = 24;
    static constexpr float contentPadding = 14.0f;
    static constexpr float controlHeight = 30.0f;

    inline static const juce::Colour kBackground { 0xff111315 };
    inline static const juce::Colour kSurface { 0xff171A1D };
    inline static const juce::Colour kSurfaceSoft { 0xff1D2126 };
    inline static const juce::Colour kSurfaceBorder { 0xff2A2F36 };
    inline static const juce::Colour kForeground { 0xffF3F5F7 };
    inline static const juce::Colour kForegroundSubtle { 0xffA7B0BC };

    inline static const juce::Colour kAccent { 0xff9C57DF };
    inline static const juce::Colour kAccentHover { 0xffAE72E6 };
    inline static const juce::Colour kAccentAlt { 0xff3E63DD };
    inline static const juce::Colour kSuccess { 0xff3BB273 };
    inline static const juce::Colour kWarning { 0xffF4B860 };
    inline static const juce::Colour kError { 0xffE06C75 };
    inline static const juce::Colour kDisabled { 0xff585F67 };
};

inline juce::Colour statusBg(MessageStatus status)
{
    switch (status)
    {
        case MessageStatus::loading:
            return PluginTheme::kAccentAlt.withAlpha(0.28f);
        case MessageStatus::success:
            return PluginTheme::kSuccess.withAlpha(0.22f);
        case MessageStatus::warning:
            return PluginTheme::kWarning.withAlpha(0.2f);
        case MessageStatus::error:
            return PluginTheme::kError.withAlpha(0.2f);
        case MessageStatus::disabled:
            return PluginTheme::kDisabled.withAlpha(0.18f);
        case MessageStatus::neutral:
        default:
            return PluginTheme::kSurfaceSoft.withAlpha(0.85f);
    }
}

inline juce::Colour statusText(MessageStatus status)
{
    switch (status)
    {
        case MessageStatus::loading:
            return PluginTheme::kAccentAlt;
        case MessageStatus::success:
            return PluginTheme::kSuccess;
        case MessageStatus::warning:
            return PluginTheme::kWarning;
        case MessageStatus::error:
            return PluginTheme::kError;
        case MessageStatus::disabled:
            return PluginTheme::kDisabled;
        case MessageStatus::neutral:
        default:
            return PluginTheme::kForeground;
    }
}

inline juce::Font headingFont(float size)
{
    return juce::Font(juce::FontOptions("Syne", size, juce::Font::bold));
}

inline juce::Font bodyFont(float size, int styleFlags = juce::Font::plain)
{
    return juce::Font(juce::FontOptions("Syne", size, styleFlags));
}

class StemhubPluginLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    StemhubPluginLookAndFeel();

    void drawButtonBackground(juce::Graphics&,
                              juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
};

void stylePrimaryButton(juce::TextButton& button);
void styleSecondaryButton(juce::TextButton& button);
void styleGhostButton(juce::TextButton& button);
void styleTextInput(juce::TextEditor& input, const juce::String& emptyText);
void styleComboBox(juce::ComboBox& combo);
void styleStatusLabel(juce::Label& label,
                      const juce::String& message,
                      MessageStatus severity);
void styleInfoLabel(juce::Label& label,
                    const juce::String& text,
                    bool emphasized = false,
                    bool muted = false);
void paintSurface(juce::Graphics& g, const juce::Rectangle<float>& bounds);
}
