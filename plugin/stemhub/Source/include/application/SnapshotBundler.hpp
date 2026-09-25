#pragma once

#include <functional>

#include <JuceHeader.h>

// The files come from stemhub::snapshotfiles::collect; paths are relative to the project file's folder.
struct SnapshotBundleRequest
{
    juce::File sourceProjectFile;
    juce::String sourceDaw;
};

struct ContentAddressedFileEntry
{
    juce::File file;          // absolute local path
    juce::String sha256;      // lowercase hex, 64 chars
    juce::int64 sizeBytes { 0 };
    juce::String filename;    // path relative to the snapshot root, '/'-separated
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
    juce::String filename;    // validated relative path inside the restore directory
    bool isProjectFile { false };
};

struct ParsedManifest
{
    int manifestVersion { 0 };
    juce::String sourceDaw;
    juce::String sourceProjectFilename;
    std::vector<ParsedManifestEntry> entries;   // project file first; one entry per path
};

class SnapshotBundler
{
    public:
        // Hash every included file (project + assets), build a VersionManifestV1-shaped
        // juce::var, and return both the manifest and the entries so the caller can call
        // check-missing + uploadBlob. onFileHashed(done, total) follows the hashing; a
        // cancelled job stops between two files.
        [[nodiscard]] juce::Result buildManifest(const SnapshotBundleRequest& request,
                                                 ContentAddressedManifest& outResult,
                                                 const std::function<void(int, int)>& onFileHashed = {}) const;

        // Parse a v1 manifest_json blob (as returned by GET /versions/{vid})
        // into a flat list of entries the caller can iterate to download blobs.
        // Only accepts manifest_version == 1. Paths and hashes are validated here because a
        // manifest can be written by any collaborator of the project.
        [[nodiscard]] static juce::Result parseManifest(const juce::var& manifestJson,
                                                        ParsedManifest& outResult);

        // True for a relative, '/'-separated path whose segments are all plain names, so that
        // restoreDirectory.getChildFile(path) always stays inside restoreDirectory on every OS.
        [[nodiscard]] static bool isSafeManifestPath(const juce::String& path);

        // Lowercase hex SHA-256 of a file's bytes; empty when it can't be read.
        [[nodiscard]] static juce::String sha256OfFile(const juce::File& file);
};
