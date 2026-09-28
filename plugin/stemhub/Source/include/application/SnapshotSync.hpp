#pragma once

#include <functional>

#include <JuceHeader.h>

#include "domain/Version.hpp"
#include "network/SignedInApi.hpp"

// Sending a snapshot to StemHub and bringing one back, on top of the typed API. See
// docs/content-addressed-storage.md. Stateless: every call gets what it needs as arguments, so
// any thread can run it.
namespace stemhub::snapshots
{
// A line for the user about how far a job got ("Uploading 3 of 12 files..."), sent from the
// job's thread. May be empty.
using ReportProgress = std::function<void(const juce::String&)>;

struct PushRequest
{
    juce::String projectId;
    juce::String branchId;
    // The snapshot is this file and the files around it: see stemhub::snapshotfiles::collect.
    juce::File projectFile;
    juce::String commitMessage;
    juce::String parentVersionId;
};

// Hashes the project's files, uploads the ones the server doesn't have (identical files once),
// then creates the version. A snapshot beyond the backend's limits is refused before anything is
// uploaded. A cancelled job stops between two files, or during an upload.
ApiResult<VersionSummary> pushSnapshot(const SignedInApi& api, const PushRequest& request, const ReportProgress& report = {});

struct RestoreRequest
{
    juce::String projectId;
    juce::String versionId;
    // Must not exist yet.
    juce::File destinationFolder;
};

// Downloads the version's files, checking each one's SHA-256, and returns the restored project
// file. The files land in a hidden folder next to destinationFolder, which takes its name only
// once all of them are there: a failure, a cancel or a crash never leaves a copy that looks
// complete. A cancelled job stops between two downloads, or during one.
ApiResult<juce::File> restoreSnapshot(const SignedInApi& api, const RestoreRequest& request, const ReportProgress& report = {});
}
