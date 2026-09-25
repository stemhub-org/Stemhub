#pragma once

#include <JuceHeader.h>

#include "application/SnapshotBundler.hpp"
#include "domain/Version.hpp"
#include "network/ApiClient.hpp"

// Sending a snapshot to StemHub and bringing one back, on top of the typed API. See
// docs/content-addressed-storage.md. Stateless: every call gets what it needs as arguments, so
// any thread can run it.
namespace stemhub::snapshots
{
struct PushRequest
{
    juce::String projectId;
    juce::String branchId;
    juce::String commitMessage;
    juce::String parentVersionId;
    ContentAddressedManifest manifest;
};

// Uploads the blobs the server doesn't have (identical files once), then creates the version.
ApiResult<VersionSummary> pushSnapshot(const IProjectApi& api, const juce::String& token, const PushRequest& request);

struct RestoreRequest
{
    juce::String projectId;
    juce::String versionId;
    // Must not exist yet. It is created, and removed again if the restore fails.
    juce::File destinationFolder;
};

// Downloads the version's files into destinationFolder, checking each one's SHA-256, and
// returns the restored project file.
ApiResult<juce::File> restoreSnapshot(const IProjectApi& api, const juce::String& token, const RestoreRequest& request);
}
