#pragma once

#include <JuceHeader.h>

// The message earlier plugins saved when the user wrote none. The history shows those versions as
// untitled, like versions without a message; the plugin no longer sends it.
inline constexpr auto kLegacyUntitledMessage = "Save from plugin";
// Longest message the backend accepts (VersionFromManifestCreate.message).
inline constexpr int kMaxMessageLength = 500;

// One saved version of a branch, as the version list returns it.
struct VersionSummary
{
    juce::String id;
    juce::String branchId;
    juce::String parentVersionId;
    juce::String createdAt;
    juce::String message;
    juce::String sourceDaw;
    juce::String sourceProjectFilename;
    // Sum of the file sizes listed in the version's manifest; 0 when it has none.
    juce::int64 totalSizeBytes { 0 };

    [[nodiscard]] bool isValid() const noexcept
    {
        return id.isNotEmpty() && branchId.isNotEmpty();
    }
};
