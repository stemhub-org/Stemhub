#pragma once

#include <optional>
#include <vector>

#include <JuceHeader.h>

#include "application/SnapshotSync.hpp"
#include "application/WorkingCopyIndex.hpp"
#include "domain/Branch.hpp"
#include "domain/Project.hpp"
#include "domain/Status.hpp"
#include "domain/User.hpp"
#include "domain/Version.hpp"
#include "domain/WorkingCopyBaseline.hpp"
#include "network/ApiClient.hpp"
#include "network/SignedInApi.hpp"

// The background work behind each user action. Every function takes an input built on the
// message thread and returns a result for the message thread to apply. They share no state, so
// they can run on any worker thread; the API must be safe to call from several threads. Signed-in
// work gets the API with the user's token (SignedInApi).
namespace stemhub::usecases
{
using ReportProgress = stemhub::snapshots::ReportProgress;

// How a job ended. Each job's result adds what it produced.
struct JobOutcome
{
    // Why the job failed; empty when it did what it was asked.
    juce::String errorMessage;
    // What to tell the user when it didn't fail: a success, or a warning about a later step.
    Status status;
    // The backend refused the token (HTTP 401): the session is over and the user has to sign in
    // again.
    bool sessionExpired { false };

    [[nodiscard]] bool failed() const noexcept { return errorMessage.isNotEmpty(); }
};

// status: about the project list, for the project grid.
struct AuthRequestResult : JobOutcome
{
    std::optional<User> user;
    std::vector<Project> projects;
    juce::String token;
    // Signed in with the saved token, as when the plugin window opens.
    bool fromSavedSession { false };
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

// Only a refused token counts as an expired session: offline, the token is worth keeping.
AuthRequestResult restoreSession(const IProjectApi& api, const RestoreSessionInput& input);

struct ProjectActivationJobResult : JobOutcome
{
    // Set as soon as the project exists, even when a later step fails.
    std::optional<Project> selectedProject;
    // The project list reloaded after creating a project, when that worked.
    std::optional<std::vector<Project>> refreshedProjects;
    std::vector<Branch> branches;
    std::vector<VersionSummary> versions;
    juce::String branchId;
    juce::String selectedVersionId;
    juce::File projectFile;
    // What projectFile holds, when known.
    WorkingCopyBaseline workingCopy;
    // The latest version, restored into a new folder for the DAW to open as its own project.
    // Unset when nothing was restored.
    WorkingCopyBaseline restoredCopy;
};

struct OpenProjectInput
{
    juce::String projectId;
    // Opened when the project still has it; otherwise "main", or the first branch.
    juce::String preferredBranchId;
    juce::File localProjectFile;
    std::vector<Project> availableProjects;
    // An explicit open from the project grid: restore the latest version when doing so
    // replaces nothing (see planLatestVersion).
    bool restoreLatestIfSafe { false };
    // Says which version localProjectFile holds, when this machine saved or restored it for
    // this project and branch. A restored copy is recorded there too.
    WorkingCopyIndex workingCopies;
    // Where the latest version is restored when there is no local copy.
    juce::File restoredProjectsFolder;
};

// Reports the download when it restores the latest version.
ProjectActivationJobResult openProject(const SignedInApi& api, const OpenProjectInput& input, const ReportProgress& report = {});

// What opening a project from the grid does about its latest version.
enum class LatestVersionPlan
{
    keepLocalFile,    // nothing is recorded about the local file, so it stays as it is
    keepLocalChanges, // the local file changed since it was saved or restored: never replaced
    alreadyLatest,    // the local file is the latest version, unchanged
    restoreLatest     // no local file, or an unchanged copy of an older version
};

// localCopy is what is recorded about the local file, and isUnchanged whether the file still
// matches it (WorkingCopyBaseline::isUnchanged, which reads the disk).
[[nodiscard]] LatestVersionPlan planLatestVersion(bool hasLocalFile,
                                                  const WorkingCopyBaseline& localCopy,
                                                  bool isUnchanged,
                                                  const juce::String& latestVersionId);

struct CreateProjectInput
{
    juce::File localProjectFile;
};

ProjectActivationJobResult createProject(const SignedInApi& api, const CreateProjectInput& input);

struct BranchHistoryJobResult : JobOutcome
{
    std::vector<VersionSummary> versions;
    juce::String branchId;
    juce::String selectedVersionId;
};

struct FetchHistoryInput
{
    juce::String branchId;
    // For the status line.
    juce::String branchName;
    juce::String preferredVersionId;
};

BranchHistoryJobResult fetchHistory(const SignedInApi& api, const FetchHistoryInput& input);

struct PushVersionJobResult : JobOutcome
{
    // The pushed file as the new version, with its size and modification time from before it
    // was hashed: if the DAW saves again meanwhile, the next save sees a change.
    WorkingCopyBaseline pushedCopy;
    // Branch history fetched right after the push, so the new version shows up at once.
    std::optional<std::vector<VersionSummary>> refreshedVersions;
};

struct PushInput
{
    juce::File projectFile;
    juce::String projectId;
    juce::String branchId;
    juce::String parentVersionId;
    // Empty gets kDefaultSaveNote.
    juce::String commitMessage;
    // The saved file is recorded there with its new version.
    WorkingCopyIndex workingCopies;
};

PushVersionJobResult pushVersion(const SignedInApi& api, const PushInput& input, const ReportProgress& report = {});

struct RestoreVersionJobResult : JobOutcome
{
    // The restored project file and its version.
    WorkingCopyBaseline restoredCopy;
};

struct RestoreInput
{
    juce::String projectId;
    juce::String versionId;
    juce::String branchId;
    // A folder that doesn't exist yet: restores never write into existing folders.
    juce::File destinationFolder;
    // The restored project file is recorded there with its version.
    WorkingCopyIndex workingCopies;
};

RestoreVersionJobResult restoreVersion(const SignedInApi& api, const RestoreInput& input, const ReportProgress& report = {});
}
