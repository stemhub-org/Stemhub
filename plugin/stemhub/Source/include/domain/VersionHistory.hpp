#pragma once

#include <vector>

#include <JuceHeader.h>

#include "domain/Version.hpp"

// Rules about a workspace's list of versions.
namespace stemhub::versionhistory
{
// Newest first, as the history shows them.
void sortNewestFirst(std::vector<VersionSummary>& versions);

// The preferred version while the list still has it, else the newest; empty for an empty list.
[[nodiscard]] juce::String chooseSelected(const std::vector<VersionSummary>& versions, const juce::String& preferredVersionId);
}
