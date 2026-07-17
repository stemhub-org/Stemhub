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

// Parsed entry from a v1 manifest — describes ONE blob the client needs to
// materialize during pull, plus where to put it.
struct ParsedManifestEntry
{
    juce::String sha256;
    juce::int64 sizeBytes { 0 };
    juce::String filename;    // basename; used relative to the restore directory
    bool isProjectFile { false };
};

struct ParsedManifest
{
    int manifestVersion { 0 };
    juce::String sourceDaw;
    juce::String sourceProjectFilename;
    std::vector<ParsedManifestEntry> entries;   // project file first if present
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

        // Parse a v1 manifest_json blob (as returned by GET /versions/{vid})
        // into a flat list of entries the caller can iterate to download blobs.
        // Only accepts manifest_version == 1.
        [[nodiscard]] static juce::Result parseContentAddressedManifest(const juce::var& manifestJson,
                                                                         ParsedManifest& outResult);
};
