#pragma once

#include <JuceHeader.h>

// StemHub identity system 2026: Ink / Paper / Blue, Syne display type over a
// Plus Jakarta Sans workhorse, hard edges, tracked uppercase meta labels and
// block "pattern bars" lifted from the step-sequencer motif.
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

// Visual role of a TextButton, read back by StemhubPluginLookAndFeel when painting.
enum class ButtonVariant
{
    primary,
    secondary,
    ghost,
    tab,
    link
};

struct PluginTheme
{
    // Brand palette (Blue is kAccent below)
    inline static const juce::Colour kInk { 0xff0a0a0a };
    inline static const juce::Colour kPaper { 0xfff2efe6 };
    inline static const juce::Colour kSignal { 0xffff5a36 };
    inline static const juce::Colour kSlate { 0xff698e9c };

    // Ink surfaces
    inline static const juce::Colour kBackground { 0xff0a0a0a };
    inline static const juce::Colour kSurface { 0xff121212 };
    inline static const juce::Colour kSurfaceElevated { 0xff1a1a1a };
    inline static const juce::Colour kSurfaceBorder { 0xff2a2a2a };
    inline static const juce::Colour kBorderSubtle { 0xff1e1e1e };
    inline static const juce::Colour kForeground { 0xfff2efe6 };
    inline static const juce::Colour kForegroundSubtle { 0xff9c9894 };
    inline static const juce::Colour kForegroundTertiary { 0xff64615c };
    inline static const juce::Colour kDisabled { 0xff4a4845 };

    // Paper surfaces
    inline static const juce::Colour kPaperInput { 0xfffbfaf6 };
    inline static const juce::Colour kPaperBorder { 0xffd3cfc3 };
    inline static const juce::Colour kInkSubtle { 0xff5c5a55 };

    // Signals
    inline static const juce::Colour kAccent { 0xff00b8ff };
    inline static const juce::Colour kAccentHover { 0xff4dceff };
    inline static const juce::Colour kAccentPressed { 0xff0096d1 };
    inline static const juce::Colour kWarning { 0xffffb020 };
    inline static const juce::Colour kError { 0xffff5a36 };
};

// Custom colour id for button outlines (secondary variant).
inline constexpr int buttonOutlineColourId = 0x5e1b0001;

juce::Colour statusColour(MessageStatus status);

juce::String arrowRight();
juce::String arrowLeft();
juce::String middleDot();
juce::String ellipsis();

// Typography. All faces are embedded (see Resources/fonts) so rendering never
// depends on what the host machine has installed.
juce::Font displayFont(float size);     // Syne ExtraBold, -3% tracking: hero statements
juce::Font headingFont(float size);     // Plus Jakarta Sans Bold: UI titles
juce::Font bodyFont(float size, int styleFlags = juce::Font::plain);
juce::Font mediumFont(float size);
juce::Font semiboldFont(float size);
juce::Font labelFont(float size);       // Plus Jakarta Sans Bold, +8% tracking: uppercase meta labels
juce::Font monoFont(float size, int styleFlags = juce::Font::plain);

struct BrandTypefaces;

// Largest display font size (<= maxSize) at which every line fits in maxWidth.
float fitDisplayFontSize(const juce::StringArray& lines, float maxWidth, float maxSize, float minSize = 18.0f);

class StemhubPluginLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    StemhubPluginLookAndFeel();
    ~StemhubPluginLookAndFeel() override;

    void drawButtonBackground(juce::Graphics&,
                              juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
    void drawButtonText(juce::Graphics&,
                        juce::TextButton&,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    void drawLabel(juce::Graphics&, juce::Label&) override;

    void fillTextEditorBackground(juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int width, int height, juce::TextEditor&) override;

    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;

    int getDefaultScrollbarWidth() override;
    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                       bool isMouseOver, bool isMouseDown) override;

    juce::AlertWindow* createAlertWindow(const juce::String& title,
                                         const juce::String& message,
                                         const juce::String& button1,
                                         const juce::String& button2,
                                         const juce::String& button3,
                                         juce::MessageBoxIconType iconType,
                                         int numButtons,
                                         juce::Component* associatedComponent) override;
    void drawAlertBox(juce::Graphics&, juce::AlertWindow&, const juce::Rectangle<int>& textArea,
                      juce::TextLayout&) override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;

    void drawTooltip(juce::Graphics&, const juce::String& text, int width, int height) override;

private:
    // Keeps the embedded typefaces registered for as long as the editor lives.
    juce::SharedResourcePointer<BrandTypefaces> typefaces;
};

void styleButton(juce::TextButton& button, ButtonVariant variant);
void stylePrimaryButton(juce::TextButton& button);
void styleSecondaryButton(juce::TextButton& button);
void styleGhostButton(juce::TextButton& button);
// Underlined text tab; the selected tab is Paper with a Blue rule.
void styleTabButton(juce::TextButton& button, bool selected);
void styleLinkButton(juce::TextButton& button, juce::Colour colour, juce::Colour hoverColour);
// Keeps a primary button painted as active while disabled (e.g. "Signing in...").
void setButtonBusy(juce::TextButton& button, bool busy);

void styleTextInput(juce::TextEditor& input, const juce::String& emptyText);
void stylePaperTextInput(juce::TextEditor& input, const juce::String& emptyText);
void styleComboBox(juce::ComboBox& combo);

// Status strip: tinted fill, severity bar on the left, readable text.
void styleStatusLabel(juce::Label& label,
                      const juce::String& message,
                      MessageStatus severity);
// Re-tints a status strip for placement on a Paper surface.
void adaptStatusLabelForPaper(juce::Label& label, MessageStatus severity);
// Compact chip (severity square + uppercase text); combine with styleStatusLabel.
void makeStatusChip(juce::Label& label);
// Borderless status line (severity square + text); combine with styleStatusLabel.
void makeInlineStatus(juce::Label& label);
void styleMetaLabel(juce::Label& label, const juce::String& text, juce::Colour colour, float size = 10.0f);

void paintMetaText(juce::Graphics& g,
                   const juce::String& text,
                   juce::Rectangle<int> area,
                   juce::Colour colour,
                   juce::Justification justification = juce::Justification::centredLeft,
                   float size = 10.0f);
void paintTag(juce::Graphics& g,
              const juce::String& text,
              juce::Rectangle<float> area,
              juce::Colour fill,
              juce::Colour textColour,
              bool outlined = false);
float tagWidth(const juce::String& text);
// Word-wrapped Syne with the brand's tight leading; overflow is ellipsised on the last line.
// Returns the painted height (cap line of the first row to the baseline of the last).
int paintDisplayText(juce::Graphics& g,
                     const juce::String& text,
                     juce::Rectangle<int> area,
                     float fontSize,
                     juce::Colour colour,
                     int maxLines,
                     bool anchorBottom = false);
// The StemHub figure "catching the signal".
void paintLogoMark(juce::Graphics& g,
                   juce::Rectangle<float> bounds,
                   juce::Colour colour,
                   juce::RectanglePlacement placement = juce::RectanglePlacement::centred);
void paintLogoTile(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour tileColour, juce::Colour markColour);
// Deterministic step-sequencer blocks; the same seed always draws the same pattern.
void paintBlockPattern(juce::Graphics& g,
                       juce::Rectangle<float> bounds,
                       const juce::String& seed,
                       int steps,
                       juce::Colour blockColour,
                       juce::Colour accentColour);
// Stacked block patterns, like the tracks of a step sequencer: a project's generated artwork.
void paintSequencerArt(juce::Graphics& g,
                       juce::Rectangle<float> bounds,
                       const juce::String& seed,
                       int tracks,
                       int steps,
                       juce::Colour blockColour,
                       juce::Colour accentColour);
}
