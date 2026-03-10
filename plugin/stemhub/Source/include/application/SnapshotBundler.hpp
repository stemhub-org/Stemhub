#pragma once

#include <JuceHeader.h>

struct SnapshotBundleRequest
{
    juce::File sourceProjectFile;
    juce::File projectRootDirectory;
    juce::String sourceDaw;
    juce::File previewTrackFile;
};

struct SnapshotBundleResult
{
    juce::File bundleFile;
    juce::var manifest;
};

class SnapshotBundler
{
    public:
        [[nodiscard]] juce::Result bundleProject(const SnapshotBundleRequest& request,
                                                    SnapshotBundleResult& outResult) const;
};
