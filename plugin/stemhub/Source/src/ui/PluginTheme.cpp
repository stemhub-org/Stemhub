#include "ui/PluginTheme.hpp"
#include "StemhubAssets.h"

#include <array>
#include <vector>

namespace stemhub::plugin::theme
{
struct BrandTypefaces
{
    juce::Typeface::Ptr syneExtraBold = load(StemhubAssets::SyneExtraBold_ttf, StemhubAssets::SyneExtraBold_ttfSize);
    juce::Typeface::Ptr jakartaRegular = load(StemhubAssets::PlusJakartaSansRegular_ttf, StemhubAssets::PlusJakartaSansRegular_ttfSize);
    juce::Typeface::Ptr jakartaMedium = load(StemhubAssets::PlusJakartaSansMedium_ttf, StemhubAssets::PlusJakartaSansMedium_ttfSize);
    juce::Typeface::Ptr jakartaSemiBold = load(StemhubAssets::PlusJakartaSansSemiBold_ttf, StemhubAssets::PlusJakartaSansSemiBold_ttfSize);
    juce::Typeface::Ptr jakartaBold = load(StemhubAssets::PlusJakartaSansBold_ttf, StemhubAssets::PlusJakartaSansBold_ttfSize);

private:
    static juce::Typeface::Ptr load(const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor(data, static_cast<size_t>(size));
    }
};

namespace
{
constexpr auto kVariantProperty = "stemhubVariant";
constexpr auto kBusyProperty = "stemhubBusy";
constexpr auto kStatusProperty = "stemhubStatus";
constexpr auto kChipProperty = "stemhubChip";
constexpr auto kInlineStatusProperty = "stemhubInlineStatus";

enum class Face
{
    syneExtraBold,
    jakartaRegular,
    jakartaMedium,
    jakartaSemiBold,
    jakartaBold
};

juce::Font makeFont(Face face, float size, float tracking = 0.0f)
{
    const juce::SharedResourcePointer<BrandTypefaces> faces;
    juce::Typeface::Ptr typeface;

    switch (face)
    {
        case Face::syneExtraBold: typeface = faces->syneExtraBold; break;
        case Face::jakartaRegular: typeface = faces->jakartaRegular; break;
        case Face::jakartaMedium: typeface = faces->jakartaMedium; break;
        case Face::jakartaSemiBold: typeface = faces->jakartaSemiBold; break;
        case Face::jakartaBold: typeface = faces->jakartaBold; break;
    }

    if (typeface == nullptr)
    {
        const auto isBold = face == Face::syneExtraBold || face == Face::jakartaBold;
        return juce::Font(juce::FontOptions(size, isBold ? juce::Font::bold : juce::Font::plain).withKerningFactor(tracking));
    }

    return juce::Font(juce::FontOptions(typeface).withHeight(size).withKerningFactor(tracking));
}

ButtonVariant variantOf(const juce::Button& button)
{
    const auto& value = button.getProperties()[kVariantProperty];
    return value.isVoid() ? ButtonVariant::secondary : static_cast<ButtonVariant>(static_cast<int>(value));
}

bool isBusy(const juce::Button& button)
{
    return static_cast<bool>(button.getProperties()[kBusyProperty]);
}

// The figure in 0..100 app-icon space, traced from the identity board.
std::vector<juce::Path> createLogoParts()
{
    std::vector<juce::Path> parts;

    juce::Path head;
    head.addEllipse(35.5f, 13.8f, 16.4f, 16.4f);
    parts.push_back(head);

    juce::Path torso;
    torso.addRoundedRectangle(35.5f, 32.9f, 17.8f, 27.0f, 1.2f);
    parts.push_back(torso);

    const auto addLimb = [&parts](float x1, float y1, float x2, float y2, float thickness)
    {
        juce::Path line;
        line.startNewSubPath(x1, y1);
        line.lineTo(x2, y2);

        juce::Path stroked;
        juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
            .createStrokedPath(stroked, line);
        parts.push_back(stroked);
    };

    addLimb(39.0f, 36.5f, 20.6f, 46.6f, 6.6f); // lower arm
    addLimb(49.0f, 37.5f, 65.8f, 21.8f, 7.0f); // arm reaching for the signal
    addLimb(40.6f, 57.0f, 25.8f, 84.0f, 8.4f); // back leg
    addLimb(48.9f, 57.0f, 48.9f, 87.4f, 8.6f); // standing leg

    const std::array<juce::Rectangle<float>, 3> bars {
        juce::Rectangle<float> { 68.0f, 10.1f, 4.4f, 7.4f },
        juce::Rectangle<float> { 74.4f, 5.5f, 4.5f, 16.5f },
        juce::Rectangle<float> { 81.1f, 8.6f, 4.4f, 10.5f }
    };

    for (const auto& bar : bars)
    {
        juce::Path signalBar;
        signalBar.addRoundedRectangle(bar, bar.getWidth() * 0.5f);
        parts.push_back(signalBar);
    }

    return parts;
}

void fillLogoParts(juce::Graphics& g, const juce::AffineTransform& transform, juce::Colour colour)
{
    g.setColour(colour);
    // Filled one by one: the stroked limbs wind differently from the ellipse/rects and
    // would cut holes where they overlap if merged into a single non-zero path.
    for (const auto& part : createLogoParts())
        g.fillPath(part, transform);
}
}

//==============================================================================
juce::Colour statusColour(MessageStatus status)
{
    switch (status)
    {
        case MessageStatus::loading: return PluginTheme::kSlate;
        case MessageStatus::success: return PluginTheme::kAccent;
        case MessageStatus::warning: return PluginTheme::kWarning;
        case MessageStatus::error: return PluginTheme::kError;
        case MessageStatus::disabled: return PluginTheme::kDisabled;
        case MessageStatus::neutral:
        default: return PluginTheme::kForegroundSubtle;
    }
}

juce::String arrowRight() { return juce::String::fromUTF8("\xe2\x86\x92"); }
juce::String arrowLeft() { return juce::String::fromUTF8("\xe2\x86\x90"); }
juce::String middleDot() { return juce::String::fromUTF8("\xc2\xb7"); }
juce::String ellipsis() { return juce::String::fromUTF8("\xe2\x80\xa6"); }

juce::Font displayFont(float size) { return makeFont(Face::syneExtraBold, size, -0.02f); }
juce::Font headingFont(float size) { return makeFont(Face::jakartaBold, size, -0.01f); }
juce::Font mediumFont(float size) { return makeFont(Face::jakartaMedium, size); }
juce::Font semiboldFont(float size) { return makeFont(Face::jakartaSemiBold, size); }
juce::Font labelFont(float size) { return makeFont(Face::jakartaBold, size, 0.06f); }

juce::Font bodyFont(float size, int styleFlags)
{
    return makeFont((styleFlags & juce::Font::bold) != 0 ? Face::jakartaBold : Face::jakartaRegular, size);
}

juce::Font monoFont(float size, int styleFlags)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size, styleFlags));
}

float fitDisplayFontSize(const juce::StringArray& lines, float maxWidth, float maxSize, float minSize)
{
    auto size = maxSize;
    const auto reference = displayFont(maxSize);

    for (const auto& line : lines)
    {
        const auto width = juce::GlyphArrangement::getStringWidth(reference, line);
        if (width > maxWidth && width > 0.0f)
            size = juce::jmin(size, maxSize * maxWidth / width);
    }

    return juce::jmax(minSize, std::floor(size));
}

//==============================================================================
StemhubPluginLookAndFeel::StemhubPluginLookAndFeel()
{
    setColourScheme({ PluginTheme::kBackground,
                      PluginTheme::kSurface,
                      PluginTheme::kSurfaceElevated,
                      PluginTheme::kSurfaceBorder,
                      PluginTheme::kForeground,
                      PluginTheme::kSurfaceElevated,
                      PluginTheme::kInk,
                      PluginTheme::kAccent,
                      PluginTheme::kForeground });

    if (typefaces->jakartaRegular != nullptr)
        setDefaultSansSerifTypeface(typefaces->jakartaRegular);

    setColour(buttonOutlineColourId, PluginTheme::kForeground.withAlpha(0.26f));
    setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::textColourOffId, PluginTheme::kForeground);
    setColour(juce::TextButton::textColourOnId, PluginTheme::kForeground);

    setColour(juce::TextEditor::backgroundColourId, PluginTheme::kSurface);
    setColour(juce::TextEditor::textColourId, PluginTheme::kForeground);
    setColour(juce::TextEditor::outlineColourId, PluginTheme::kSurfaceBorder);
    setColour(juce::TextEditor::focusedOutlineColourId, PluginTheme::kAccent);
    setColour(juce::TextEditor::highlightColourId, PluginTheme::kAccent.withAlpha(0.32f));
    setColour(juce::TextEditor::highlightedTextColourId, PluginTheme::kForeground);
    setColour(juce::CaretComponent::caretColourId, PluginTheme::kAccent);

    setColour(juce::ComboBox::backgroundColourId, PluginTheme::kSurface);
    setColour(juce::ComboBox::textColourId, PluginTheme::kForeground);
    setColour(juce::ComboBox::outlineColourId, PluginTheme::kSurfaceBorder);
    setColour(juce::ComboBox::arrowColourId, PluginTheme::kForegroundSubtle);
    setColour(juce::ComboBox::focusedOutlineColourId, PluginTheme::kAccent);

    setColour(juce::PopupMenu::backgroundColourId, PluginTheme::kSurfaceElevated);
    setColour(juce::PopupMenu::textColourId, PluginTheme::kForeground);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, PluginTheme::kAccent);
    setColour(juce::PopupMenu::highlightedTextColourId, PluginTheme::kInk);

    setColour(juce::AlertWindow::backgroundColourId, PluginTheme::kBackground);
    setColour(juce::AlertWindow::textColourId, PluginTheme::kForeground);
    setColour(juce::AlertWindow::outlineColourId, PluginTheme::kSurfaceBorder);

    setColour(juce::ScrollBar::thumbColourId, PluginTheme::kForeground.withAlpha(0.2f));
    setColour(juce::TooltipWindow::backgroundColourId, PluginTheme::kSurfaceElevated);
    setColour(juce::TooltipWindow::textColourId, PluginTheme::kForeground);
    setColour(juce::TooltipWindow::outlineColourId, PluginTheme::kSurfaceBorder);
    setColour(juce::Label::textColourId, PluginTheme::kForeground);
}

StemhubPluginLookAndFeel::~StemhubPluginLookAndFeel() = default;

void StemhubPluginLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                                    juce::Button& button,
                                                    const juce::Colour& backgroundColour,
                                                    bool shouldDrawButtonAsHighlighted,
                                                    bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const auto enabled = button.isEnabled() || isBusy(button);
    const auto hot = button.isEnabled() && (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown);
    const auto textColour = button.findColour(juce::TextButton::textColourOffId);

    switch (variantOf(button))
    {
        case ButtonVariant::primary:
        {
            if (!enabled)
            {
                g.setColour(PluginTheme::kSurfaceElevated);
                g.fillRect(bounds);
                g.setColour(PluginTheme::kSurfaceBorder);
                g.drawRect(bounds, 1.0f);
                return;
            }

            const auto isAccent = backgroundColour == PluginTheme::kAccent;
            auto fill = backgroundColour;
            if (shouldDrawButtonAsDown && hot)
                fill = isAccent ? PluginTheme::kAccentPressed : fill.interpolatedWith(textColour, 0.22f);
            else if (hot)
                fill = isAccent ? PluginTheme::kAccentHover : fill.interpolatedWith(textColour, 0.12f);

            g.setColour(fill);
            g.fillRect(bounds);
            return;
        }

        case ButtonVariant::tab:
            if (button.getToggleState())
            {
                g.setColour(PluginTheme::kAccent);
                g.fillRect(bounds.withTop(bounds.getBottom() - 2.0f));
            }
            return;

        case ButtonVariant::secondary:
        {
            auto fill = backgroundColour;
            if (hot && !button.getToggleState())
                fill = fill.overlaidWith(textColour.withAlpha(shouldDrawButtonAsDown ? 0.14f : 0.07f));

            g.setColour(fill);
            g.fillRect(bounds);

            auto outline = button.findColour(buttonOutlineColourId);
            if (hot)
                outline = textColour.withAlpha(0.7f);
            if (!enabled)
                outline = outline.withMultipliedAlpha(0.5f);

            g.setColour(outline);
            const auto r = bounds.reduced(0.5f);
            g.drawLine(r.getX(), r.getY(), r.getRight(), r.getY(), 1.0f);
            g.drawLine(r.getX(), r.getBottom(), r.getRight(), r.getBottom(), 1.0f);
            g.drawLine(r.getRight(), r.getY(), r.getRight(), r.getBottom(), 1.0f);
            if (!button.isConnectedOnLeft())
                g.drawLine(r.getX(), r.getY(), r.getX(), r.getBottom(), 1.0f);
            return;
        }

        case ButtonVariant::ghost:
            if (hot)
            {
                g.setColour(textColour.withAlpha(shouldDrawButtonAsDown ? 0.12f : 0.07f));
                g.fillRect(bounds);
            }
            return;

        case ButtonVariant::link:
        default:
            return;
    }
}

void StemhubPluginLookAndFeel::drawButtonText(juce::Graphics& g,
                                              juce::TextButton& button,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    const auto variant = variantOf(button);
    const auto enabled = button.isEnabled() || isBusy(button);
    const auto hot = button.isEnabled() && (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown);
    const auto font = getTextButtonFont(button, button.getHeight());

    auto colour = button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                            : juce::TextButton::textColourOffId);
    if (hot && (variant == ButtonVariant::ghost || variant == ButtonVariant::link || variant == ButtonVariant::tab))
        colour = button.findColour(juce::TextButton::textColourOnId);

    if (!enabled)
        colour = variant == ButtonVariant::primary ? PluginTheme::kForegroundTertiary
                                                   : colour.withMultipliedAlpha(0.42f);

    const auto isLink = variant == ButtonVariant::link;
    const auto text = isLink ? button.getButtonText() : button.getButtonText().toUpperCase();
    const auto area = button.getLocalBounds().reduced(isLink || variant == ButtonVariant::tab ? 0 : 10, 0);
    const auto justification = static_cast<bool>(button.getProperties()["stemhubAlignLeft"])
                                   ? juce::Justification::centredLeft
                                   : juce::Justification::centred;

    g.setFont(font);
    g.setColour(colour);
    g.drawText(text, area, justification, true);

    if (isLink && static_cast<bool>(button.getProperties()["underlined"]))
    {
        const auto textWidth = juce::jmin(static_cast<float>(area.getWidth()),
                                          juce::GlyphArrangement::getStringWidth(font, text));
        const auto x = justification == juce::Justification::centredLeft
                           ? static_cast<float>(area.getX())
                           : static_cast<float>(area.getCentreX()) - textWidth * 0.5f;
        const auto baseline = static_cast<float>(area.getCentreY()) - font.getHeight() * 0.5f + font.getAscent();
        g.fillRect(x, baseline + 2.5f, textWidth, 1.0f);
    }
}

juce::Font StemhubPluginLookAndFeel::getTextButtonFont(juce::TextButton& button, int)
{
    switch (variantOf(button))
    {
        case ButtonVariant::link: return mediumFont(12.5f);
        case ButtonVariant::tab: return labelFont(10.5f);
        case ButtonVariant::primary:
        case ButtonVariant::secondary:
        case ButtonVariant::ghost:
        default: return labelFont(11.0f);
    }
}

void StemhubPluginLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    const auto alpha = label.isEnabled() ? 1.0f : 0.5f;
    const auto bounds = label.getLocalBounds();
    const auto& properties = label.getProperties();
    const auto background = label.findColour(juce::Label::backgroundColourId);
    const auto textColour = label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha);
    const auto accent = label.findColour(juce::Label::outlineColourId);

    if (static_cast<bool>(properties[kChipProperty]))
    {
        g.setColour(background);
        g.fillRect(bounds);
        g.setColour(accent.withAlpha(0.55f));
        g.drawRect(bounds, 1);

        auto content = bounds.reduced(10, 0);
        g.setColour(accent);
        g.fillRect(content.removeFromLeft(6).withSizeKeepingCentre(6, 6));
        content.removeFromLeft(8);

        g.setColour(textColour);
        g.setFont(labelFont(10.0f));
        g.drawText(label.getText().toUpperCase(), content, juce::Justification::centredLeft, true);
        return;
    }

    if (static_cast<bool>(properties[kInlineStatusProperty]))
    {
        auto content = bounds;
        g.setColour(accent);
        g.fillRect(content.removeFromLeft(6).withSizeKeepingCentre(6, 6));
        content.removeFromLeft(10);

        g.setColour(textColour);
        g.setFont(getLabelFont(label));
        g.drawText(label.getText(), content, juce::Justification::centredLeft, true);
        return;
    }

    if (static_cast<bool>(properties[kStatusProperty]))
    {
        g.setColour(background);
        g.fillRect(bounds);
        g.setColour(accent);
        g.fillRect(bounds.withWidth(3));

        const auto font = getLabelFont(label);
        g.setColour(textColour);
        g.setFont(font);
        g.drawFittedText(label.getText(), bounds.withTrimmedLeft(15).withTrimmedRight(10),
                         juce::Justification::centredLeft, 2, 1.0f);
        return;
    }

    g.fillAll(background);

    if (!label.isBeingEdited())
    {
        const auto font = getLabelFont(label);
        const auto textArea = getLabelBorderSize(label).subtractedFrom(bounds);
        g.setColour(textColour);
        g.setFont(font);
        // Never squash glyphs horizontally: truncate instead, it keeps the type honest.
        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                         juce::jmax(1, static_cast<int>(static_cast<float>(textArea.getHeight()) / font.getHeight())),
                         1.0f);
    }

    if (!accent.isTransparent())
    {
        g.setColour(accent.withMultipliedAlpha(alpha));
        g.drawRect(bounds);
    }
}

void StemhubPluginLookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour(editor.findColour(juce::TextEditor::backgroundColourId));
    g.fillRect(0, 0, width, height);
}

void StemhubPluginLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    const auto focused = editor.hasKeyboardFocus(true) && !editor.isReadOnly();
    auto colour = editor.findColour(focused ? juce::TextEditor::focusedOutlineColourId
                                            : juce::TextEditor::outlineColourId);
    if (!editor.isEnabled())
        colour = colour.withMultipliedAlpha(0.5f);

    g.setColour(colour);
    g.drawRect(0, 0, width, height, focused ? 2 : 1);
}

void StemhubPluginLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                            int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int>(width, height);
    const auto hot = box.isEnabled() && (isButtonDown || box.isMouseOver(true));

    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRect(bounds);
    g.setColour(hot ? PluginTheme::kForeground.withAlpha(0.5f) : box.findColour(juce::ComboBox::outlineColourId));
    g.drawRect(bounds, 1);

    const auto centreX = static_cast<float>(width) - 16.0f;
    const auto centreY = static_cast<float>(height) * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath(centreX - 4.0f, centreY - 2.0f);
    chevron.lineTo(centreX, centreY + 2.0f);
    chevron.lineTo(centreX + 4.0f, centreY - 2.0f);

    g.setColour(box.findColour(juce::ComboBox::arrowColourId).withMultipliedAlpha(box.isEnabled() ? 1.0f : 0.4f));
    g.strokePath(chevron, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font StemhubPluginLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return semiboldFont(12.5f);
}

void StemhubPluginLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(10, 1, box.getWidth() - 34, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setMinimumHorizontalScale(1.0f);
}

juce::Font StemhubPluginLookAndFeel::getPopupMenuFont()
{
    return mediumFont(13.5f);
}

int StemhubPluginLookAndFeel::getDefaultScrollbarWidth()
{
    return 8;
}

void StemhubPluginLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                                             bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                                             bool isMouseOver, bool isMouseDown)
{
    if (thumbSize <= 0)
        return;

    const auto thumb = isScrollbarVertical
                           ? juce::Rectangle<int>(x + width - 4, thumbStartPosition, 3, thumbSize)
                           : juce::Rectangle<int>(thumbStartPosition, y + height - 4, thumbSize, 3);

    g.setColour(PluginTheme::kForeground.withAlpha(isMouseDown ? 0.5f : (isMouseOver ? 0.34f : 0.18f)));
    g.fillRect(thumb);
}

juce::AlertWindow* StemhubPluginLookAndFeel::createAlertWindow(const juce::String& title,
                                                               const juce::String& message,
                                                               const juce::String& button1,
                                                               const juce::String& button2,
                                                               const juce::String& button3,
                                                               juce::MessageBoxIconType iconType,
                                                               int numButtons,
                                                               juce::Component* associatedComponent)
{
    auto* alert = juce::LookAndFeel_V4::createAlertWindow(title, message, button1, button2, button3,
                                                          iconType, numButtons, associatedComponent);

    // The first button is always the affirmative one ("OK", "Yes").
    if (alert != nullptr && alert->getNumButtons() > 0)
        if (auto* affirmative = dynamic_cast<juce::TextButton*>(alert->getButton(0)))
            stylePrimaryButton(*affirmative);

    return alert;
}

void StemhubPluginLookAndFeel::drawAlertBox(juce::Graphics& g, juce::AlertWindow& alert,
                                            const juce::Rectangle<int>&, juce::TextLayout& textLayout)
{
    const auto bounds = alert.getLocalBounds();
    const auto isWarning = alert.getAlertType() == juce::MessageBoxIconType::WarningIcon;
    const auto accent = isWarning ? PluginTheme::kSignal : PluginTheme::kAccent;

    g.setColour(alert.findColour(juce::AlertWindow::backgroundColourId));
    g.fillRect(bounds);
    g.setColour(alert.findColour(juce::AlertWindow::outlineColourId));
    g.drawRect(bounds, 1);
    g.setColour(accent);
    g.fillRect(bounds.withHeight(4));

    auto iconSpaceUsed = 0;
    if (alert.getAlertType() != juce::MessageBoxIconType::NoIcon)
    {
        const juce::Rectangle<int> icon { 26, 32, 34, 34 };
        g.setColour(accent);
        g.fillRect(icon);
        g.setColour(PluginTheme::kInk);
        g.setFont(displayFont(24.0f));
        g.drawText(isWarning ? "!" : (alert.getAlertType() == juce::MessageBoxIconType::InfoIcon ? "i" : "?"),
                   icon, juce::Justification::centred, false);
        iconSpaceUsed = 80;
    }

    g.setColour(alert.findColour(juce::AlertWindow::textColourId));
    const juce::Rectangle<int> textBounds { bounds.getX() + 1 + iconSpaceUsed, 30,
                                            bounds.getWidth(), bounds.getHeight() - getAlertWindowButtonHeight() - 20 };
    textLayout.draw(g, textBounds.toFloat());
}

juce::Font StemhubPluginLookAndFeel::getAlertWindowTitleFont() { return headingFont(18.0f); }
juce::Font StemhubPluginLookAndFeel::getAlertWindowMessageFont() { return bodyFont(14.0f); }
juce::Font StemhubPluginLookAndFeel::getAlertWindowFont() { return bodyFont(13.0f); }

void StemhubPluginLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto bounds = juce::Rectangle<int>(width, height);
    g.setColour(findColour(juce::TooltipWindow::backgroundColourId));
    g.fillRect(bounds);
    g.setColour(findColour(juce::TooltipWindow::outlineColourId));
    g.drawRect(bounds, 1);
    g.setColour(findColour(juce::TooltipWindow::textColourId));
    g.setFont(bodyFont(12.5f));
    g.drawFittedText(text, bounds.reduced(8, 4), juce::Justification::centredLeft, 6, 1.0f);
}

//==============================================================================
void styleButton(juce::TextButton& button, ButtonVariant variant)
{
    button.getProperties().set(kVariantProperty, static_cast<int>(variant));
    button.setMouseCursor(juce::MouseCursor::PointingHandCursor);

    switch (variant)
    {
        case ButtonVariant::primary:
            button.setColour(juce::TextButton::buttonColourId, PluginTheme::kAccent);
            button.setColour(juce::TextButton::buttonOnColourId, PluginTheme::kAccent);
            button.setColour(juce::TextButton::textColourOffId, PluginTheme::kInk);
            button.setColour(juce::TextButton::textColourOnId, PluginTheme::kInk);
            break;

        case ButtonVariant::secondary:
            button.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
            button.setColour(juce::TextButton::textColourOffId, PluginTheme::kForeground);
            button.setColour(juce::TextButton::textColourOnId, PluginTheme::kForeground);
            button.setColour(buttonOutlineColourId, PluginTheme::kForeground.withAlpha(0.26f));
            break;

        case ButtonVariant::ghost:
        case ButtonVariant::tab:
            button.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
            button.setColour(juce::TextButton::textColourOffId, PluginTheme::kForegroundSubtle);
            button.setColour(juce::TextButton::textColourOnId, PluginTheme::kForeground);
            break;

        case ButtonVariant::link:
            button.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
            button.setColour(juce::TextButton::textColourOffId, PluginTheme::kForegroundSubtle);
            button.setColour(juce::TextButton::textColourOnId, PluginTheme::kForeground);
            break;
    }

    button.repaint();
}

void stylePrimaryButton(juce::TextButton& button) { styleButton(button, ButtonVariant::primary); }
void styleSecondaryButton(juce::TextButton& button) { styleButton(button, ButtonVariant::secondary); }
void styleGhostButton(juce::TextButton& button) { styleButton(button, ButtonVariant::ghost); }

void styleTabButton(juce::TextButton& button, bool selected)
{
    styleButton(button, ButtonVariant::tab);
    button.setToggleState(selected, juce::dontSendNotification);
}

void styleLinkButton(juce::TextButton& button, juce::Colour colour, juce::Colour hoverColour)
{
    styleButton(button, ButtonVariant::link);
    button.setColour(juce::TextButton::textColourOffId, colour);
    button.setColour(juce::TextButton::textColourOnId, hoverColour);
}

void setButtonBusy(juce::TextButton& button, bool busy)
{
    button.getProperties().set(kBusyProperty, busy);
    button.repaint();
}

void styleTextInput(juce::TextEditor& input, const juce::String& emptyText)
{
    input.setMultiLine(false);
    input.setFont(mediumFont(13.5f));
    input.setJustification(juce::Justification::centredLeft);
    input.setIndents(12, 0);
    input.setTextToShowWhenEmpty(emptyText, PluginTheme::kForegroundTertiary);
    input.setColour(juce::TextEditor::backgroundColourId, PluginTheme::kSurface);
    input.setColour(juce::TextEditor::textColourId, PluginTheme::kForeground);
    input.setColour(juce::TextEditor::outlineColourId, PluginTheme::kSurfaceBorder);
    input.setColour(juce::TextEditor::focusedOutlineColourId, PluginTheme::kAccent);
    input.setColour(juce::TextEditor::highlightColourId, PluginTheme::kAccent.withAlpha(0.32f));
    input.setColour(juce::TextEditor::highlightedTextColourId, PluginTheme::kForeground);
    input.setColour(juce::CaretComponent::caretColourId, PluginTheme::kAccent);
}

void stylePaperTextInput(juce::TextEditor& input, const juce::String& emptyText)
{
    styleTextInput(input, emptyText);
    input.setTextToShowWhenEmpty(emptyText, PluginTheme::kInkSubtle.withAlpha(0.7f));
    input.setColour(juce::TextEditor::backgroundColourId, PluginTheme::kPaperInput);
    input.setColour(juce::TextEditor::textColourId, PluginTheme::kInk);
    input.setColour(juce::TextEditor::outlineColourId, PluginTheme::kPaperBorder);
    input.setColour(juce::TextEditor::focusedOutlineColourId, PluginTheme::kInk);
    input.setColour(juce::TextEditor::highlightColourId, PluginTheme::kAccent.withAlpha(0.35f));
    input.setColour(juce::TextEditor::highlightedTextColourId, PluginTheme::kInk);
    input.setColour(juce::CaretComponent::caretColourId, PluginTheme::kInk);
}

void styleComboBox(juce::ComboBox& combo)
{
    combo.setColour(juce::ComboBox::backgroundColourId, PluginTheme::kSurface);
    combo.setColour(juce::ComboBox::textColourId, PluginTheme::kForeground);
    combo.setColour(juce::ComboBox::outlineColourId, PluginTheme::kSurfaceBorder);
    combo.setColour(juce::ComboBox::arrowColourId, PluginTheme::kForegroundSubtle);
    combo.setColour(juce::ComboBox::focusedOutlineColourId, PluginTheme::kAccent);
    combo.setColour(juce::ComboBox::buttonColourId, PluginTheme::kSurfaceElevated);
    combo.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void styleStatusLabel(juce::Label& label, const juce::String& message, MessageStatus severity)
{
    const auto accent = statusColour(severity);
    label.getProperties().set(kStatusProperty, true);
    label.setText(message, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setMinimumHorizontalScale(1.0f);
    label.setFont(mediumFont(12.0f));
    label.setColour(juce::Label::backgroundColourId, accent.withAlpha(0.11f));
    label.setColour(juce::Label::textColourId, PluginTheme::kForeground);
    label.setColour(juce::Label::outlineColourId, accent);
    label.setColour(juce::Label::outlineWhenEditingColourId, accent);
    label.setColour(juce::Label::textWhenEditingColourId, PluginTheme::kForeground);
    label.repaint();
}

void adaptStatusLabelForPaper(juce::Label& label, MessageStatus severity)
{
    const auto accent = severity == MessageStatus::neutral ? PluginTheme::kInkSubtle : statusColour(severity);
    label.setColour(juce::Label::backgroundColourId, accent.withAlpha(0.14f));
    label.setColour(juce::Label::textColourId, PluginTheme::kInk);
    label.setColour(juce::Label::outlineColourId, accent);
    label.repaint();
}

void makeStatusChip(juce::Label& label)
{
    label.getProperties().set(kChipProperty, true);
}

void makeInlineStatus(juce::Label& label)
{
    label.getProperties().set(kInlineStatusProperty, true);
}

void styleMetaLabel(juce::Label& label, const juce::String& text, juce::Colour colour, float size)
{
    label.setText(text.toUpperCase(), juce::dontSendNotification);
    label.setFont(labelFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setMinimumHorizontalScale(1.0f);
    label.setBorderSize({});
}

//==============================================================================
void paintMetaText(juce::Graphics& g,
                   const juce::String& text,
                   juce::Rectangle<int> area,
                   juce::Colour colour,
                   juce::Justification justification,
                   float size)
{
    g.setColour(colour);
    g.setFont(labelFont(size));
    g.drawText(text.toUpperCase(), area, justification, true);
}

void paintTag(juce::Graphics& g,
              const juce::String& text,
              juce::Rectangle<float> area,
              juce::Colour fill,
              juce::Colour textColour,
              bool outlined)
{
    g.setColour(fill);
    if (outlined)
        g.drawRect(area, 1.0f);
    else
        g.fillRect(area);

    g.setColour(textColour);
    g.setFont(labelFont(9.5f));
    g.drawText(text.toUpperCase(), area.reduced(6.0f, 0.0f), juce::Justification::centred, true);
}

float tagWidth(const juce::String& text)
{
    return std::ceil(juce::GlyphArrangement::getStringWidth(labelFont(9.5f), text.toUpperCase())) + 16.0f;
}

int paintDisplayText(juce::Graphics& g,
                     const juce::String& text,
                     juce::Rectangle<int> area,
                     float fontSize,
                     juce::Colour colour,
                     int maxLines,
                     bool anchorBottom)
{
    if (text.isEmpty() || area.isEmpty() || maxLines <= 0)
        return 0;

    const auto font = displayFont(fontSize);
    const auto maxWidth = static_cast<float>(area.getWidth());
    const auto widthOf = [&font](const juce::String& line) { return juce::GlyphArrangement::getStringWidth(font, line); };

    juce::StringArray words;
    words.addTokens(text, " ", "");
    words.removeEmptyStrings();

    juce::StringArray lines;
    juce::String current;
    for (const auto& word : words)
    {
        const auto candidate = current.isEmpty() ? word : current + " " + word;
        if (current.isEmpty() || widthOf(candidate) <= maxWidth)
        {
            current = candidate;
        }
        else
        {
            lines.add(current);
            current = word;
        }
    }
    if (current.isNotEmpty())
        lines.add(current);

    if (lines.size() > maxLines)
    {
        juce::StringArray overflow;
        for (int i = maxLines - 1; i < lines.size(); ++i)
            overflow.add(lines[i]);

        lines.removeRange(maxLines - 1, lines.size());
        lines.add(overflow.joinIntoString(" "));
    }

    for (auto& line : lines)
    {
        if (widthOf(line) <= maxWidth)
            continue;

        while (line.length() > 1 && widthOf(line + ellipsis()) > maxWidth)
            line = line.dropLastCharacters(1).trimEnd();
        line += ellipsis();
    }

    // Syne's cap height is ~0.54 of the JUCE font height; lines step at ~0.96 em.
    const auto capHeight = fontSize * 0.545f;
    const auto lineStep = std::round(fontSize * 0.8f);
    const auto totalHeight = capHeight + lineStep * static_cast<float>(lines.size() - 1);
    auto baseline = anchorBottom ? static_cast<float>(area.getBottom()) - lineStep * static_cast<float>(lines.size() - 1)
                                 : static_cast<float>(area.getY()) + capHeight;

    g.setColour(colour);
    g.setFont(font);
    for (const auto& line : lines)
    {
        g.drawSingleLineText(line, area.getX(), static_cast<int>(std::round(baseline)));
        baseline += lineStep;
    }

    return static_cast<int>(std::ceil(totalHeight));
}

void paintLogoMark(juce::Graphics& g,
                   juce::Rectangle<float> bounds,
                   juce::Colour colour,
                   juce::RectanglePlacement placement)
{
    const juce::Rectangle<float> figureBounds { 16.0f, 5.0f, 70.0f, 88.0f };
    fillLogoParts(g, placement.getTransformToFit(figureBounds, bounds), colour);
}

void paintLogoTile(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour tileColour, juce::Colour markColour)
{
    const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto tile = bounds.withSizeKeepingCentre(side, side);

    g.setColour(tileColour);
    g.fillRoundedRectangle(tile, side * 0.24f);
    fillLogoParts(g,
                  juce::AffineTransform::scale(side / 100.0f).translated(tile.getX(), tile.getY()),
                  markColour);
}

void paintBlockPattern(juce::Graphics& g,
                       juce::Rectangle<float> bounds,
                       const juce::String& seed,
                       int steps,
                       juce::Colour blockColour,
                       juce::Colour accentColour)
{
    if (steps <= 0 || bounds.isEmpty())
        return;

    constexpr std::array<float, 4> levels { 0.2f, 0.42f, 0.68f, 1.0f };
    juce::Random random(seed.hashCode64());
    const auto stepWidth = bounds.getWidth() / static_cast<float>(steps);
    const auto gap = juce::jmax(1.0f, std::round(stepWidth * 0.22f));
    const auto firstAccent = random.nextInt(steps);
    const auto secondAccent = random.nextInt(steps);

    for (int step = 0; step < steps; ++step)
    {
        // Rising four-step phrases with a little jitter, like the sequencer rows on the board.
        const auto level = juce::jlimit(0, 3, (step % 4) + random.nextInt(3) - 1);
        const auto height = std::round(bounds.getHeight() * levels[static_cast<size_t>(level)]);
        const auto x = std::round(bounds.getX() + stepWidth * static_cast<float>(step));
        const auto nextX = std::round(bounds.getX() + stepWidth * static_cast<float>(step + 1));
        const auto y = std::round(bounds.getCentreY() - height * 0.5f);

        g.setColour(step == firstAccent || step == secondAccent ? accentColour : blockColour);
        g.fillRect(x, y, juce::jmax(1.0f, nextX - x - gap), height);
    }
}

void paintSequencerArt(juce::Graphics& g,
                       juce::Rectangle<float> bounds,
                       const juce::String& seed,
                       int tracks,
                       int steps,
                       juce::Colour blockColour,
                       juce::Colour accentColour)
{
    if (tracks <= 0 || bounds.isEmpty())
        return;

    const auto gap = 4.0f;
    const auto trackHeight = (bounds.getHeight() - gap * static_cast<float>(tracks - 1)) / static_cast<float>(tracks);
    juce::Random random(seed.hashCode64());
    const auto accentTrack = random.nextInt(tracks);

    for (int track = 0; track < tracks; ++track)
    {
        const auto row = juce::Rectangle<float>(bounds.getX(),
                                                bounds.getY() + (trackHeight + gap) * static_cast<float>(track),
                                                bounds.getWidth(),
                                                trackHeight);
        paintBlockPattern(g, row, seed + ":" + juce::String(track), steps, blockColour,
                          track == accentTrack ? accentColour : blockColour);
    }
}
}
