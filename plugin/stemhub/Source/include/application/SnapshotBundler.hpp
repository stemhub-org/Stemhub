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

struct ContentAddressedFileEntry
{
    juce::File file;          // absolute local path
    juce::String sha256;      // lowercase hex, 64 chars
    juce::int64 sizeBytes { 0 };
    juce::String filename;    // basename (for display)
    bool isProjectFile { false };
};

struct ContentAddressedManifest
{
    juce::var manifestJson;   // matches VersionManifestV1 shape on the backend
    std::vector<ContentAddressedFileEntry> entries;
};

class SnapshotBundler
{
    public:
        [[nodiscard]] juce::Result bundleProject(const SnapshotBundleRequest& request,
                                                    SnapshotBundleResult& outResult) const;

        // Content-addressed variant: hash every included file (project + assets),
        // build a VersionManifestV1-shaped juce::var, and return both the manifest
        // and the entries so the caller can call check-missing + uploadBlob.
        // Does NOT write a zip.
        [[nodiscard]] juce::Result buildContentAddressedManifest(const SnapshotBundleRequest& request,
                                                                    ContentAddressedManifest& outResult) const;
};
