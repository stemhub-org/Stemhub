#pragma once

#include <JuceHeader.h>

#include "domain/Status.hpp"
#include "ui/ViewModels.hpp"

// How the UI writes values. No GUI code, so the tests can check it.
namespace stemhub::uiformat
{
juce::String middleDot();
juce::String ellipsis();
// "  ·  ", between two facts on one line.
juce::String metaSeparator();

// "01", "12", "123".
juce::String twoDigits(int value);

// "Just now", "5 min ago", "3 h ago", "Yesterday", "4 days ago", then "12 Mar 2026". Empty when
// the time is unknown.
juce::String relativeTime(juce::Time time, juce::Time now);

// "Wed 18 Mar, 10:00", or "Wed 18 Mar 2026, 10:00" with the year; "Unknown time" when unknown.
juce::String timestamp(juce::Time time, bool withYear);

// A version's title: its message, or "Untitled version" when it has none of its own.
juce::String versionTitle(const VersionListItem& version);

// Lowercase letters and digits, with runs of anything else as single dashes: "night-bus".
juce::String slug(const juce::String& text);

// What a save of the working copy takes, for the dashboard's footer: "3 files · 48.2 MB",
// "No working copy", or "Counting files…" while fileCount is negative.
juce::String workingCopySummary(bool hasWorkingCopy, int fileCount, juce::int64 totalBytes);

// The dashboard's status chip: "Working…", "Done", "Attention", "Error" or "Ready".
juce::String statusChipText(Status::Severity severity);
}
