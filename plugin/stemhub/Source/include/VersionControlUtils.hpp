#pragma once
#include <JuceHeader.h>

struct VersionSummary
{
    juce::String id;
    juce::String branchId;
    juce::String parentVersionId;
<<<<<<< HEAD
=======
    juce::String createdAt;
>>>>>>> origin/dev
    juce::String commitMessage;
    juce::String sourceDaw;
    juce::String sourceProjectFilename;
    juce::String artifactPath;
    juce::String artifactChecksum;
    int64 artifactSizeBytes { 0 };
    bool hasArtifact { false };

    [[nodiscard]] bool isValid() const noexcept
    {
        return id.isNotEmpty() && branchId.isNotEmpty();
    }
};

struct ProjectVersionContext
{
    juce::String projectId;
    juce::String branchId;
    juce::String lastVersionId;

    [[nodiscard]] bool isValid() const noexcept
    {
        return projectId.isNotEmpty() && branchId.isNotEmpty();
    }
};

struct PushVersionRequest
{
    juce::String branchId;
    juce::File localProjectFile;
    juce::String commitMessage;
    juce::String dawName;
    juce::String parentVersionId;
<<<<<<< HEAD
=======
    juce::String sourceProjectFilename;
    juce::var snapshotManifest;
>>>>>>> origin/dev
};
