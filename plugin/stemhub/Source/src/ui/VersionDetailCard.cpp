#include "ui/PluginTheme.hpp"
#include "ui/UiFormat.hpp"
#include "ui/VersionDetailCard.hpp"

namespace
{
namespace theme = stemhub::plugin::theme;
namespace uiformat = stemhub::uiformat;
using Theme = theme::PluginTheme;

constexpr int kCardPadding = 20;
// The restore button and its hint, from the bottom of the card.
constexpr int kRestoreHintHeight = 30;
constexpr int kRestoreButtonHeight = 40;
}

VersionDetailCard::VersionDetailCard()
{
    addChildComponent(restoreHintLabel);
    restoreHintLabel.setText("Opens in your DAW as a new copy. Your project stays as it is.",
                             juce::dontSendNotification);
    restoreHintLabel.setFont(theme::bodyFont(11.5f));
    restoreHintLabel.setColour(juce::Label::textColourId, Theme::kInkSubtle);
    restoreHintLabel.setJustificationType(juce::Justification::topLeft);
    restoreHintLabel.setMinimumHorizontalScale(1.0f);
    restoreHintLabel.setBorderSize({});

    addChildComponent(restoreButton);
    restoreButton.setButtonText("Restore this version");
    theme::stylePrimaryButton(restoreButton);
    restoreButton.setColour(juce::TextButton::buttonColourId, Theme::kInk);
    restoreButton.setColour(juce::TextButton::textColourOffId, Theme::kPaper);
    restoreButton.setColour(juce::TextButton::textColourOnId, Theme::kPaper);
    restoreButton.onClick = [this]
    {
        if (onRestore != nullptr)
            onRestore();
    };
}

void VersionDetailCard::setVersion(const VersionListItem& versionToShow, const int number, const int count)
{
    if (version.has_value() && *version == versionToShow && versionNumber == number && versionCount == count)
        return;

    version = versionToShow;
    versionNumber = number;
    versionCount = count;
    restoreButton.setVisible(true);
    restoreHintLabel.setVisible(true);
    repaint();
}

void VersionDetailCard::clearVersion()
{
    if (!version.has_value())
        return;

    version.reset();
    restoreButton.setVisible(false);
    restoreHintLabel.setVisible(false);
    repaint();
}

void VersionDetailCard::setActivity(const SessionActivity activity)
{
    const auto isRestoring = activity == SessionActivity::restoring;
    restoreButton.setEnabled(activity == SessionActivity::idle);
    theme::setButtonBusy(restoreButton, isRestoring);
    restoreButton.setButtonText(isRestoring ? "Restoring..." : "Restore this version");
}

void VersionDetailCard::resized()
{
    auto card = getLocalBounds().reduced(kCardPadding);
    restoreHintLabel.setBounds(card.removeFromBottom(kRestoreHintHeight));
    card.removeFromBottom(8);
    restoreButton.setBounds(card.removeFromBottom(kRestoreButtonHeight));
}

void VersionDetailCard::paint(juce::Graphics& g)
{
    // Paper on Ink, like the sign-in card.
    g.setColour(Theme::kPaper);
    g.fillRect(getLocalBounds());

    auto card = getLocalBounds().reduced(kCardPadding);
    card.removeFromBottom(version.has_value() ? kRestoreHintHeight + 8 + kRestoreButtonHeight + 14 : 0);

    auto metaRow = card.removeFromTop(16);
    g.setColour(Theme::kAccent);
    g.fillRect(metaRow.removeFromLeft(8).withSizeKeepingCentre(8, 8));
    metaRow.removeFromLeft(8);

    if (version.has_value() && version->isOpenInDaw)
    {
        const auto label = juce::String("In your DAW");
        const auto width = static_cast<int>(theme::tagWidth(label));
        theme::paintTag(g, label, metaRow.removeFromRight(width).withSizeKeepingCentre(width, 18).toFloat(),
                        Theme::kAccent, Theme::kInk);
        metaRow.removeFromRight(8);
    }

    theme::paintMetaText(g,
                         version.has_value() ? "Version " + uiformat::twoDigits(versionNumber) + " / " + uiformat::twoDigits(versionCount)
                                             : juce::String("Version --"),
                         metaRow, Theme::kInk);

    card.removeFromTop(16);

    if (!version.has_value())
    {
        const auto height = theme::paintDisplayText(g, "NOTHING SAVED YET.", card, 22.0f, Theme::kInk, 2);
        card.removeFromTop(height + 18);
        theme::paintSequencerArt(g, card.removeFromTop(30).toFloat(), "empty-history", 2, 14,
                                 Theme::kInk.withAlpha(0.1f), Theme::kInk.withAlpha(0.2f));
        card.removeFromTop(18);
        g.setColour(Theme::kInkSubtle);
        g.setFont(theme::bodyFont(12.5f));
        g.drawFittedText("Write what changed, then Save snapshot to start this branch's history.",
                         card.removeFromTop(54), juce::Justification::topLeft, 3, 1.0f);
        return;
    }

    const auto title = uiformat::versionTitle(*version).toUpperCase();
    juce::StringArray words;
    words.addTokens(title, " ", "");
    const auto titleSize = theme::fitDisplayFontSize(words, static_cast<float>(card.getWidth()), 22.0f, 15.0f);
    const auto titleHeight = theme::paintDisplayText(g, title, card.withHeight(80), titleSize, Theme::kInk, 3);
    card.removeFromTop(titleHeight + 18);

    theme::paintBlockPattern(g, card.removeFromTop(28).toFloat(), version->id, 18, Theme::kInk, Theme::kAccent);
    card.removeFromTop(16);

    const auto addFact = [&g, &card](const juce::String& key, const juce::String& value, bool mono)
    {
        if (value.isEmpty() || card.getHeight() < 26)
            return;

        auto row = card.removeFromTop(26);
        g.setColour(Theme::kPaperBorder);
        g.fillRect(row.removeFromTop(1));
        theme::paintMetaText(g, key, row.removeFromLeft(58), Theme::kInkSubtle, juce::Justification::centredLeft, 9.5f);
        g.setColour(Theme::kInk);
        g.setFont(mono ? theme::monoFont(11.5f) : theme::mediumFont(12.0f));
        g.drawText(value, row, juce::Justification::centredLeft, true);
    };

    addFact("Saved", uiformat::timestamp(version->createdAt, true), false);
    addFact("DAW", version->sourceDaw, false);
    addFact("File", version->sourceFilename, false);
    addFact("Size", version->sizeBytes > 0 ? juce::File::descriptionOfSizeInBytes(version->sizeBytes) : juce::String(), false);
    addFact("ID", version->id.substring(0, 8), true);
}
