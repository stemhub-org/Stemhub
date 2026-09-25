#pragma once

#include <JuceHeader.h>

// One saved version of a branch, as the version list returns it.
struct VersionSummary
{
    juce::String id;
    juce::String branchId;
    juce::String parentVersionId;
    juce::String createdAt;
    juce::String commitMessage;
    juce::String sourceDaw;
    juce::String sourceProjectFilename;
    // Sum of the file sizes listed in the version's manifest; 0 when it has none.
    juce::int64 totalSizeBytes { 0 };

    [[nodiscard]] bool isValid() const noexcept
    {
        return id.isNotEmpty() && branchId.isNotEmpty();
    }
};
