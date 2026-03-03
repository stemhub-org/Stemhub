#include "../include/Views.hpp"

DashboardView::DashboardView()
{
    addAndMakeVisible(statusLabel);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setFont(juce::FontOptions(20.0f, juce::Font::bold));

    addAndMakeVisible(saveChanges);
    saveChanges.onClick = [this]
    {
        if (onSave != nullptr)
            onSave();
    };

    addAndMakeVisible(syncButton);
    syncButton.onClick = [this]
    {
        if (onSync != nullptr)
            onSync();
    };

    addAndMakeVisible(changeBranch);
    changeBranch.onClick = [this]
    {
        if (onBranchChange != nullptr)
            onBranchChange();
    };

    addAndMakeVisible(signOutButton);
    signOutButton.onClick = [this]
    {
        if (onSignOut != nullptr)
            onSignOut();
    };
}

void DashboardView::resized()
{
    auto area = getLocalBounds().reduced(20);
    const int fieldWidth = 220;
    const int x = (getWidth() - fieldWidth) / 2;

    auto labelRow = area.removeFromTop(56);
    statusLabel.setBounds(labelRow);

    area.removeFromTop(24);

    auto saveRow = area.removeFromTop(32);
    saveChanges.setBounds(x, saveRow.getY(), fieldWidth, saveRow.getHeight());

    area.removeFromTop(8);

    auto syncRow = area.removeFromTop(32);
    syncButton.setBounds(x, syncRow.getY(), fieldWidth, syncRow.getHeight());

    area.removeFromTop(8);

    auto branchRow = area.removeFromTop(32);
    changeBranch.setBounds(x, branchRow.getY(), fieldWidth, branchRow.getHeight());

    area.removeFromTop(24);

    auto signOutRow = area.removeFromTop(32);
    signOutButton.setBounds(x, signOutRow.getY(), fieldWidth, signOutRow.getHeight());
}
