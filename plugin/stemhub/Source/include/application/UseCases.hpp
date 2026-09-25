#pragma once

#include <optional>
#include <vector>

#include <JuceHeader.h>

#include "application/VersionControlUtils.hpp"
#include "domain/Branch.hpp"
#include "domain/Project.hpp"
#include "domain/User.hpp"
#include "domain/WorkingCopyBaseline.hpp"
#include "network/ApiClient.hpp"

// The background work behind each user action. Every function takes an input built on the
// message thread and returns a result for the message thread to apply. They share no state, so
// they can run on any worker thread; the API must be safe to call from several threads.
namespace stemhub::usecases
{
struct AuthRequestResult
{
    std::optional<User> user;
    std::vector<Project> projects;
    juce::String token;
    juce::String authErrorMessage;
    juce::String projectSelectionStatusMessage;
    bool fromCachedSession { false };
};

struct SignInInput
{
    juce::String email;
    juce::String password;
};

AuthRequestResult signIn(const IProjectApi& api, const SignInInput& input);

struct RestoreSessionInput
{
    juce::String token;
};

AuthRequestResult restoreSession(const IProjectApi& api, const RestoreSessionInput& input);

struct ProjectActivationJobResult
{
    uint64_t selectionRequestId {};
    std::optional<Project> selectedProject;
    std::vector<Project> projects;
    std::vector<Branch> branches;
    std::vector<VersionSummary> versions;
    juce::String branchId;
    juce::String branchName;
    juce::String selectedVersionId;
    juce::File projectFile;
    // What projectFile holds, when known.
    WorkingCopyBaseline workingCopy;
    // The latest version was just restored into projectFile; it should be opened in the DAW.
    bool didRestoreLatest { false };
    juce::String errorMessage;
    juce::String activeProjectStatusMessage;
    bool refreshProjects { false };
    bool fromCachedProjectRestore { false };
};

struct OpenProjectInput
{
    juce::String projectId;
    juce::File localProjectFile;
    std::vector<Project> availableProjects;
    juce::String token;
    // An explicit open from the project grid: restore the latest version when doing so
    // replaces nothing (no local copy, or an unchanged copy of an older version).
    bool restoreLatestIfSafe { false };
    // What this instance recorded about localProjectFile; unset when it knows nothing.
    WorkingCopyBaseline localCopy;
    // Where the latest version is restored when there is no local copy.
    juce::File managedWorkingCopyFolder;
};

ProjectActivationJobResult openProject(const IProjectApi& api, const OpenProjectInput& input);

struct CreateProjectInput
{
    juce::File localProjectFile;
    juce::String token;
};

ProjectActivationJobResult createProject(const IProjectApi& api, const CreateProjectInput& input);

struct BranchHistoryJobResult
{
    uint64_t selectionRequestId {};
    std::vector<VersionSummary> versions;
    juce::String branchId;
    juce::String branchName;
    juce::String selectedVersionId;
    // The version the local file's restore folder names, if any.
    juce::String hintedVersionId;
    juce::String errorMessage;
    juce::String activeProjectStatusMessage;
};

struct FetchHistoryInput
{
    juce::String branchId;
    juce::String branchName;
    juce::String preferredVersionId;
    juce::String token;
    juce::File localProjectFile;
};

BranchHistoryJobResult fetchHistory(const IProjectApi& api, const FetchHistoryInput& input);

struct PushVersionJobResult
{
    juce::String pushedVersionId;
    // The pushed file as the new version, with its size and modification time from before it
    // was hashed: if the DAW saves again meanwhile, the next save sees a change.
    WorkingCopyBaseline pushedCopy;
    // Branch history fetched right after the push, so the new version shows up at once.
    std::optional<std::vector<VersionSummary>> refreshedVersions;
    juce::String errorMessage;
    juce::String activeProjectStatusMessage;
};

struct PushInput
{
    juce::File projectFile;
    juce::String projectId;
    juce::String branchId;
    juce::String parentVersionId;
    juce::String commitMessage;
    juce::String dawName;
    juce::String token;
};

PushVersionJobResult pushVersion(const IProjectApi& api, const PushInput& input);

struct RestoreVersionJobResult
{
    juce::String restoredVersionId;
    juce::File restoredProjectFile;
    WorkingCopyBaseline restoredCopy;
    juce::String errorMessage;
    juce::String activeProjectStatusMessage;
};

struct RestoreInput
{
    juce::String projectId;
    juce::String versionId;
    // A folder that doesn't exist yet: restores never write into existing folders.
    juce::File destinationFolder;
    juce::String token;
};

RestoreVersionJobResult restoreVersion(const IProjectApi& api, const RestoreInput& input);
}
