#include "ui/LoginView.hpp"

namespace
{
namespace theme = stemhub::plugin::theme;
using Theme = theme::PluginTheme;

constexpr int kOuterPadding = 28;
constexpr int kCardPadding = 28;

struct LoginLayout
{
    juce::Rectangle<int> metaRow;
    int dividerY { 0 };
    juce::Rectangle<int> hero;
    juce::Rectangle<int> card;
};

LoginLayout computeLayout(const int width, const int height)
{
    LoginLayout layout;
    auto area = juce::Rectangle<int>(width, height).reduced(kOuterPadding);

    layout.metaRow = area.removeFromTop(14);
    area.removeFromTop(14);
    layout.dividerY = area.getY();
    area.removeFromTop(24);

    const auto cardWidth = juce::jlimit(300, 380, static_cast<int>(static_cast<float>(width) * 0.47f));
    layout.card = area.removeFromRight(cardWidth);
    area.removeFromRight(32);
    layout.hero = area;
    return layout;
}

juce::StringArray heroLines()
{
    return { "OPEN", "THE", "PROJECT." };
}

void styleFieldLabel(juce::Label& label, const juce::String& text)
{
    theme::styleMetaLabel(label, text, Theme::kInk);
}
}

LoginView::LoginView()
{
    addAndMakeVisible(authStateLabel);
    theme::styleStatusLabel(authStateLabel, {}, theme::MessageStatus::neutral);
    authStateLabel.setVisible(false);

    addAndMakeVisible(titleLabel);
    titleLabel.setText("Welcome back.", juce::dontSendNotification);
    titleLabel.setFont(theme::headingFont(25.0f));
    titleLabel.setColour(juce::Label::textColourId, Theme::kInk);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setMinimumHorizontalScale(1.0f);
    titleLabel.setBorderSize({});

    addAndMakeVisible(subtitleLabel);
    subtitleLabel.setText("Sign in to version your music projects.", juce::dontSendNotification);
    subtitleLabel.setFont(theme::bodyFont(13.0f));
    subtitleLabel.setColour(juce::Label::textColourId, Theme::kInkSubtle);
    subtitleLabel.setJustificationType(juce::Justification::centredLeft);
    subtitleLabel.setMinimumHorizontalScale(1.0f);
    subtitleLabel.setBorderSize({});

    addAndMakeVisible(emailLabel);
    styleFieldLabel(emailLabel, "Email");

    addAndMakeVisible(passwordLabel);
    styleFieldLabel(passwordLabel, "Password");

    const auto signIn = [this]
    {
        if (signInButton.isEnabled() && onSignIn != nullptr)
            onSignIn();
    };

    addAndMakeVisible(emailInput);
    theme::stylePaperTextInput(emailInput, "you@example.com");
    emailInput.setTitle("Email");
    emailInput.onReturnKey = signIn;

    addAndMakeVisible(passwordInput);
    theme::stylePaperTextInput(passwordInput, "Your password");
    passwordInput.setPasswordCharacter(static_cast<juce::juce_wchar>(0x2022));
    passwordInput.setTitle("Password");
    passwordInput.onReturnKey = signIn;

    addAndMakeVisible(signInButton);
    signInButton.setButtonText("Sign in  " + theme::arrowRight());
    theme::stylePrimaryButton(signInButton);
    signInButton.setTooltip("Sign in to access Stemhub projects.");
    signInButton.onClick = signIn;

    addChildComponent(cancelButton);
    cancelButton.setButtonText("Cancel");
    theme::styleLinkButton(cancelButton, Theme::kInkSubtle, Theme::kInk);
    cancelButton.getProperties().set("underlined", true);
    cancelButton.getProperties().set("stemhubAlignLeft", true);
    cancelButton.setTooltip("Stop signing in.");
    cancelButton.onClick = [this]
    {
        if (onCancel != nullptr)
            onCancel();
    };
}

void LoginView::show(const LoginModel& model)
{
    if (model.status == shown.status && model.isSigningIn == shown.isSigningIn)
        return;

    shown = model;
    showStatus(model.status.text, theme::messageStatusFor(model.status.severity), model.isSigningIn);
}

void LoginView::showFormMessage(const juce::String& message)
{
    showStatus(message, theme::MessageStatus::warning, false);
}

void LoginView::clearInputs()
{
    emailInput.clear();
    passwordInput.clear();
}

void LoginView::showStatus(const juce::String& message, stemhub::plugin::theme::MessageStatus status, bool isSigningIn)
{
    theme::styleStatusLabel(authStateLabel, message, status);
    theme::adaptStatusLabelForPaper(authStateLabel, status);
    authStateLabel.setTooltip(message);
    authStateLabel.setVisible(message.isNotEmpty());

    signInButton.setEnabled(!isSigningIn);
    emailInput.setEnabled(!isSigningIn);
    passwordInput.setEnabled(!isSigningIn);
    theme::setButtonBusy(signInButton, isSigningIn);
    signInButton.setButtonText(isSigningIn ? juce::String("Signing in...")
                                           : "Sign in  " + theme::arrowRight());
    cancelButton.setVisible(isSigningIn);
    resized();
    repaint();
}

void LoginView::paint(juce::Graphics& g)
{
    g.fillAll(Theme::kBackground);

    const auto layout = computeLayout(getWidth(), getHeight());

    theme::paintMetaText(g, "StemHub plugin", layout.metaRow, Theme::kForeground);
    theme::paintMetaText(g, "Version control for music", layout.metaRow, Theme::kForegroundSubtle,
                         juce::Justification::centredRight);
    g.setColour(Theme::kSurfaceBorder);
    g.fillRect(layout.metaRow.getX(), layout.dividerY, layout.metaRow.getWidth(), 1);

    // Hero column: the figure catching the signal, then the campaign line.
    auto hero = layout.hero;
    const auto markHeight = 62.0f;
    theme::paintLogoMark(g,
                         hero.removeFromTop(static_cast<int>(markHeight)).toFloat(),
                         Theme::kForeground,
                         juce::RectanglePlacement(juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yTop));
    hero.removeFromTop(30);

    const auto lines = heroLines();
    const auto fontSize = theme::fitDisplayFontSize(lines, static_cast<float>(hero.getWidth()), 58.0f);
    const auto capHeight = fontSize * 0.545f;
    const auto lineStep = std::round(fontSize * 0.76f);
    auto baseline = static_cast<float>(hero.getY()) + capHeight;

    g.setColour(Theme::kForeground);
    g.setFont(theme::displayFont(fontSize));
    for (int i = 0; i < lines.size(); ++i)
    {
        g.setColour(i == lines.size() - 1 ? Theme::kAccent : Theme::kForeground);
        g.drawSingleLineText(lines[i], hero.getX(), static_cast<int>(baseline));
        if (i < lines.size() - 1)
            baseline += lineStep;
    }

    const auto tagText = "Save " + theme::middleDot() + " Refresh " + theme::middleDot() + " Restore";
    const auto tagWidth = juce::GlyphArrangement::getStringWidth(theme::labelFont(9.5f), tagText.toUpperCase()) + 20.0f;
    theme::paintTag(g, tagText,
                    { static_cast<float>(hero.getX()), baseline + 22.0f, tagWidth, 22.0f },
                    Theme::kForeground, Theme::kInk);

    auto footer = layout.hero.withTop(layout.hero.getBottom() - 90);
    theme::paintBlockPattern(g,
                             footer.removeFromTop(24).withWidth(juce::jmin(232, footer.getWidth())).toFloat(),
                             "stemhub-session",
                             16,
                             Theme::kForeground.withAlpha(0.88f),
                             Theme::kAccent);
    footer.removeFromTop(16);
    g.setColour(Theme::kForegroundSubtle);
    g.setFont(theme::bodyFont(13.0f));
    g.drawFittedText("Version every idea without leaving your DAW. Save versions, switch branches and pass the project on.",
                     footer, juce::Justification::topLeft, 3, 1.0f);

    // Sign-in card: a Paper block colliding with the Ink field.
    g.setColour(Theme::kPaper);
    g.fillRect(layout.card);

    auto eyebrow = layout.card.reduced(kCardPadding).removeFromTop(14);
    g.setColour(Theme::kAccent);
    g.fillRect(eyebrow.removeFromLeft(8).withSizeKeepingCentre(8, 8));
    eyebrow.removeFromLeft(8);
    theme::paintMetaText(g, "Sign in / 01", eyebrow, Theme::kInk);
    theme::paintMetaText(g, "Account", eyebrow, Theme::kInkSubtle, juce::Justification::centredRight);
}

void LoginView::resized()
{
    const auto layout = computeLayout(getWidth(), getHeight());
    auto content = layout.card.reduced(kCardPadding);

    cancelButton.setBounds(content.removeFromBottom(20).withWidth(140));

    content.removeFromTop(14); // eyebrow, painted
    content.removeFromTop(16);
    titleLabel.setBounds(content.removeFromTop(32));
    content.removeFromTop(2);
    subtitleLabel.setBounds(content.removeFromTop(20));

    content.removeFromTop(24);
    emailLabel.setBounds(content.removeFromTop(14));
    content.removeFromTop(6);
    emailInput.setBounds(content.removeFromTop(42));

    content.removeFromTop(14);
    passwordLabel.setBounds(content.removeFromTop(14));
    content.removeFromTop(6);
    passwordInput.setBounds(content.removeFromTop(42));

    content.removeFromTop(18);

    if (authStateLabel.isVisible())
    {
        authStateLabel.setBounds(content.removeFromTop(38));
        content.removeFromTop(10);
    }
    else
    {
        authStateLabel.setBounds(0, 0, 0, 0);
    }

    signInButton.setBounds(content.removeFromTop(46));
}
