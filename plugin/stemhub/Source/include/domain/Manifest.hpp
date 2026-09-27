#pragma once

#include <vector>

#include <JuceHeader.h>

// A version's manifest (VersionManifestV1 on the backend): which files the version has, by
// content. The plugin that saves a version writes it; every collaborator who restores the version
// reads it, so reading checks everything a hostile manifest could get wrong.
namespace stemhub::manifest
{
// Backend limits (schemas.py): files besides the project file, and the length of a path.
constexpr size_t kMaxTracks = 500;
constexpr int kMaxPathLength = 255;

struct FileRef
{
    juce::String sha256;          // lowercase hex, 64 characters
    juce::int64 sizeBytes { 0 };
    juce::String path;            // relative to the project file's folder, '/'-separated
};

struct Manifest
{
    juce::String sourceDaw;
    FileRef projectFile;
    std::vector<FileRef> tracks;  // the other files, one per path
};

[[nodiscard]] juce::var toJson(const Manifest& manifest);

// Only manifest_version 1. Rejects bad hashes and sizes, unsafe paths, and two different files at
// one path; paths are compared case-insensitively, as macOS and Windows file systems do. The same
// file listed twice is kept once.
[[nodiscard]] juce::Result fromJson(const juce::var& json, Manifest& out);

// True for a relative, '/'-separated path whose segments are all plain names, so that
// folder.getChildFile(path) stays inside folder on every OS.
[[nodiscard]] bool isSafePath(const juce::String& path);

// What a manifest's files add up to, for the history. 0 when it has none or can't be read.
[[nodiscard]] juce::int64 totalSize(const juce::var& json);
}
