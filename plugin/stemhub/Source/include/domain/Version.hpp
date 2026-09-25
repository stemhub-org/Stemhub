#pragma once

#include <JuceHeader.h>

// The note a save gets when the user wrote none; the history shows those versions as untitled.
inline constexpr auto kDefaultSaveNote = "Save from plugin";
// Longest save note the backend accepts (VersionFromManifestCreate.commit_message).
inline constexpr int kMaxSaveNoteLength = 500;

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
