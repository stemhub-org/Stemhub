#pragma once

#include <functional>
#include <optional>

#include <JuceHeader.h>

#include "ui/ViewModels.hpp"

// The selected version on a Paper card: its title, pattern and facts, and the button that
// restores it. Without a version, it says how the history starts.
class VersionDetailCard : public juce::Component
{
public:
    VersionDetailCard();

    // number counts from the oldest version, which is 1; count is how many there are.
    void setVersion(const VersionListItem& version, int number, int count);
    void clearVersion();
    void setActivity(SessionActivity activity);

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onRestore;

private:
    std::optional<VersionListItem> version;
    int versionNumber { 0 };
    int versionCount { 0 };
    juce::TextButton restoreButton;
    juce::Label restoreHintLabel;
};
