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
    static constexpr float panelRadius = 8.0f;
    static constexpr float buttonRadius = 6.0f;
    static constexpr float inputRadius = 6.0f;
    static constexpr int labelHeight = 22;
    static constexpr float contentPadding = 14.0f;
    static constexpr float controlHeight = 36.0f;

    // Frontend palette (rework baseline)
    inline static const juce::Colour kBackground { 0xff0f0f12 };
    inline static const juce::Colour kSurface { 0xff18181c };
    inline static const juce::Colour kSurfaceElevated { 0xff1f1f24 };
    inline static const juce::Colour kSurfaceSoft { kSurfaceElevated };
    inline static const juce::Colour kSurfaceBorder { 0xff2a2a30 };
    inline static const juce::Colour kBorderSubtle { 0xff202025 };
    inline static const juce::Colour kForeground { 0xfff5f5f7 };
    inline static const juce::Colour kForegroundSubtle { 0xffa1a1aa };
    inline static const juce::Colour kForegroundTertiary { 0xff71717a };

    inline static const juce::Colour kAccent { 0xff22d3ee };
    inline static const juce::Colour kAccentHover { 0xff06b6d4 };
    inline static const juce::Colour kAccentSubtle { 0xff0e7490 };
    inline static const juce::Colour kAccentGlow { 0x2622d3ee };
    inline static const juce::Colour kSuccess { 0xff10b981 };
    inline static const juce::Colour kSuccessSubtle { 0xff065f46 };
    inline static const juce::Colour kWarning { 0xfff59e0b };
    inline static const juce::Colour kWarningSubtle { 0xff92400e };
    inline static const juce::Colour kError { 0xffef4444 };
    inline static const juce::Colour kErrorSubtle { 0xff7f1d1d };
    inline static const juce::Colour kHover { 0xff252529 };
    inline static const juce::Colour kActive { 0xff2f2f35 };
    inline static const juce::Colour kDisabled { 0xff585f67 };
};

inline juce::Font preferredFont(const juce::String& fontName,
                               float size,
                               int styleFlags = juce::Font::plain,
                               bool isMonospace = false)
{
    const auto font = juce::Font(juce::FontOptions(fontName, size, styleFlags));
    if (font.getTypefaceName().isNotEmpty())
        return font;

    return isMonospace
               ? juce::Font(juce::FontOptions("monospace", size, styleFlags))
               : juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), size, styleFlags));
}

inline juce::Colour statusBg(MessageStatus status)
{
    switch (status)
    {
        case MessageStatus::loading:
            return PluginTheme::kAccentSubtle.withAlpha(0.30f);
        case MessageStatus::success:
            return PluginTheme::kSuccessSubtle.withAlpha(0.42f);
        case MessageStatus::warning:
            return PluginTheme::kWarningSubtle.withAlpha(0.40f);
        case MessageStatus::error:
            return PluginTheme::kErrorSubtle.withAlpha(0.42f);
        case MessageStatus::disabled:
            return PluginTheme::kHover.withAlpha(0.55f);
        case MessageStatus::neutral:
        default:
            return PluginTheme::kSurfaceElevated.withAlpha(0.80f);
    }
}

inline juce::Colour statusText(MessageStatus status)
{
    switch (status)
    {
        case MessageStatus::loading:
            return PluginTheme::kAccent;
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
    return preferredFont("Inter", size, juce::Font::bold, false);
}

inline juce::Font bodyFont(float size, int styleFlags = juce::Font::plain)
{
    return preferredFont("Inter", size, styleFlags, false);
}

inline juce::Font monoFont(float size, int styleFlags = juce::Font::plain)
{
    return preferredFont("JetBrains Mono", size, styleFlags, true);
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
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
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
void paintCard(juce::Graphics& g, const juce::Rectangle<float>& bounds, bool includeBorder = true);
void paintSurface(juce::Graphics& g, const juce::Rectangle<float>& bounds);
}
