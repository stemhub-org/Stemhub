#include <algorithm>

#include "application/RestoreFolders.hpp"
#include "application/UseCases.hpp"
#include "domain/VersionHistory.hpp"

namespace stemhub::usecases
{
namespace
{
namespace versionhistory = stemhub::versionhistory;

void loadProjects(const SignedInApi& api, AuthRequestResult& result)
{
    auto projectsResult = api.fetchProjects();
    if (projectsResult.ok())
    {
        result.projects = std::move(*projectsResult.value);
        result.status = result.projects.empty()
            ? Status::warning("No projects found.")
            : Status::info("Loaded " + juce::String(static_cast<int>(result.projects.size())) + " project(s).");
    }
    else
    {
        result.status = Status::error(projectsResult.errorMessage("Failed to load projects."));
    }
}

// Records a failed call on a job result: its message, and whether the sign-in session is over.
template <typename Result, typename T>
Result& failWith(Result& result, const ApiResult<T>& call, const juce::String& fallback)
{
    result.errorMessage = call.errorMessage(fallback);
    result.sessionExpired = call.isUnauthorized();
    return result;
}

// The preferred branch when it still exists, else "main" when the project has one, else its
// first branch.
const Branch& chooseBranch(const std::vector<Branch>& branches, const juce::String& preferredBranchId)
{
    const auto preferredIt = std::find_if(branches.begin(), branches.end(), [&preferredBranchId](const Branch& branch)
    {
        return preferredBranchId.isNotEmpty() && branch.id == preferredBranchId;
    });
    if (preferredIt != branches.end())
        return *preferredIt;

    const auto mainIt = std::find_if(branches.begin(), branches.end(), [](const Branch& branch)
    {
        return branch.name == "main";
    });
    return mainIt != branches.end() ? *mainIt : branches.front();
}

// What the working copy holds, as recorded the last time this machine saved or restored it, when
// that was for this project and branch: a version from another branch can't be a save's parent.
WorkingCopyBaseline knownCopy(const OpenProjectInput& input, const juce::String& branchId)
{
    const auto recorded = input.workingCopies.find(input.workingFile);
    if (recorded.has_value() && recorded->projectId == input.projectId && recorded->branchId == branchId)
        return recorded->copy;

    return {};
}
}

AuthRequestResult signIn(const IProjectApi& api, const SignInInput& input)
{
    AuthRequestResult result;

    auto loginResult = api.login(input.email, input.password);
    if (!loginResult.ok())
    {
        // A refused password is not an expired sign-in session.
        result.errorMessage = loginResult.errorMessage("Failed to sign in.");
        return result;
    }

    const SignedInApi signedIn(api, loginResult.value->accessToken);
    auto userResult = signedIn.fetchCurrentUser();
    if (!userResult.ok())
    {
        result.errorMessage = userResult.errorMessage("Failed to load your user profile.");
        return result;
    }

    result.token = loginResult.value->accessToken;
    result.user = std::move(userResult.value);
    loadProjects(signedIn, result);
    return result;
}

AuthRequestResult resumeSignIn(const IProjectApi& api, const ResumeSignInInput& input)
{
    AuthRequestResult result;
    result.fromSavedToken = true;
    result.token = input.token;

    const SignedInApi signedIn(api, input.token);
    auto userResult = signedIn.fetchCurrentUser();
    if (!userResult.ok())
    {
        failWith(result, userResult, "Couldn't sign you back in.");
        result.sessionExpired = result.sessionExpired || userResult.error->kind == ApiError::Kind::forbidden;
        return result;
    }

    result.user = std::move(userResult.value);
    loadProjects(signedIn, result);
    return result;
}

LatestVersionPlan planLatestVersion(const bool hasWorkingCopy,
                                    const WorkingCopyBaseline& workingCopy,
                                    const bool isUnchanged,
                                    const juce::String& latestVersionId)
{
    if (!hasWorkingCopy)
        return LatestVersionPlan::restoreLatest;

    if (!workingCopy.hasRecordedState())
        return LatestVersionPlan::keepWorkingCopy;

    if (!isUnchanged)
        return LatestVersionPlan::keepUnsavedChanges;

    return workingCopy.versionId == latestVersionId ? LatestVersionPlan::alreadyLatest : LatestVersionPlan::restoreLatest;
}

ProjectActivationJobResult openProject(const SignedInApi& api, const OpenProjectInput& input, const ReportProgress& report)
{
    ProjectActivationJobResult result;
    result.projectFile = input.workingFile;

    const auto projectIt = std::find_if(input.availableProjects.begin(),
                                        input.availableProjects.end(),
                                        [&input](const Project& project) { return project.id == input.projectId; });
    if (input.projectId.isEmpty() || projectIt == input.availableProjects.end())
    {
        result.errorMessage = "The selected project is no longer available.";
        return result;
    }

    const auto branchesResult = api.fetchBranches(input.projectId);
    if (!branchesResult.ok())
        return failWith(result, branchesResult, "Failed to load branches.");
    if (branchesResult.value->empty())
    {
        result.errorMessage = "No branches found for this project.";
        return result;
    }

    result.branches = *branchesResult.value;
    const auto selectedBranch = chooseBranch(result.branches, input.preferredBranchId);
    result.selectedProject = *projectIt;
    result.branchId = selectedBranch.id;

    auto versions = api.fetchVersions(selectedBranch.id);
    if (!versions.ok())
    {
        result.sessionExpired = versions.isUnauthorized();
        result.status = Status::warning(versions.errorMessage("Project ready, but failed to load version history."));
        return result;
    }

    result.versions = std::move(*versions.value);
    versionhistory::sortNewestFirst(result.versions);

    const auto& workingFile = input.workingFile;
    const auto workingCopy = knownCopy(input, selectedBranch.id);
    result.workingCopy = workingCopy;
    result.selectedVersionId = versionhistory::chooseSelected(result.versions, workingCopy.versionId);

    const auto hasWorkingCopy = workingFile.existsAsFile();
    const auto usingWorkingCopyMessage = hasWorkingCopy
        ? "Project ready. Working copy: " + workingFile.getFileName()
        : juce::String("Project ready. Choose a project file to save from.");

    if (result.versions.empty())
    {
        result.status = Status::success((hasWorkingCopy ? usingWorkingCopyMessage : juce::String("Project ready.")) + " No versions yet.");
        return result;
    }

    if (!input.restoreLatestIfSafe)
    {
        result.status = Status::success(hasWorkingCopy ? usingWorkingCopyMessage
                                                       : "Project ready. Loaded "
                                                             + juce::String(static_cast<int>(result.versions.size()))
                                                             + " version(s).");
        return result;
    }

    const auto& latestVersion = result.versions.front();
    switch (planLatestVersion(hasWorkingCopy, workingCopy, workingCopy.isUnchanged(), latestVersion.id))
    {
        case LatestVersionPlan::keepWorkingCopy:
            result.status = Status::success(usingWorkingCopyMessage);
            return result;

        case LatestVersionPlan::keepUnsavedChanges:
            result.status = Status::warning("Changes in your working copy are not saved to StemHub yet, so the latest "
                                            "version was not restored. Save them, or restore a version into a new folder.");
            return result;

        case LatestVersionPlan::alreadyLatest:
            result.status = Status::success("Project ready. Your working copy is the latest version.");
            return result;

        case LatestVersionPlan::restoreLatest:
            break;
    }

    // It goes to a new folder, so nothing on disk is replaced, and opens as a separate copy.
    const auto restoreFolder = stemhub::restorefolders::newFolder(
        stemhub::restorefolders::projectRoot(input.restoredProjectsFolder, *projectIt, selectedBranch),
        stemhub::restorefolders::projectName(result.versions, latestVersion.id, projectIt->name),
        latestVersion.id);

    const auto restored = stemhub::versiontransfer::restoreVersion(api, { projectIt->id, latestVersion.id, restoreFolder }, report);
    if (!restored.ok())
    {
        result.sessionExpired = restored.isUnauthorized();
        result.status = Status::warning("Project ready, but the latest version couldn't be restored: "
                                        + restored.errorMessage("unknown error"));
        return result;
    }

    const auto& restoredProjectFile = *restored.value;
    result.restoredCopy = WorkingCopyBaseline::recordedNow(restoredProjectFile, latestVersion.id);
    input.workingCopies.record({ projectIt->id, selectedBranch.id, result.restoredCopy });
    result.selectedVersionId = latestVersion.id;
    result.status = Status::success("Project ready. Latest version restored to "
                                    + restoredProjectFile.getParentDirectory().getFullPathName() + ".");
    return result;
}

ProjectActivationJobResult createProject(const SignedInApi& api, const CreateProjectInput& input)
{
    ProjectActivationJobResult result;
    result.projectFile = input.projectFile;

    const auto projectName = input.projectFile.existsAsFile()
        ? input.projectFile.getFileNameWithoutExtension()
        : juce::String();

    if (projectName.isEmpty())
    {
        result.errorMessage = "Choose a project file first.";
        return result;
    }

    const auto createdProject = api.createProject(projectName);
    if (!createdProject.ok())
        return failWith(result, createdProject, "Failed to create project.");

    // From here on the project exists: whatever fails next, the grid must show it.
    result.selectedProject = *createdProject.value;

    if (auto projectsResult = api.fetchProjects(); projectsResult.ok())
        result.refreshedProjects = std::move(*projectsResult.value);

    const auto branchesResult = api.fetchBranches(createdProject.value->id);
    if (!branchesResult.ok())
    {
        result.sessionExpired = branchesResult.isUnauthorized();
        result.errorMessage = "Project \"" + projectName + "\" was created, but its branches couldn't be loaded: "
                            + branchesResult.errorMessage("unknown error");
        return result;
    }
    if (branchesResult.value->empty())
    {
        result.errorMessage = "Project \"" + projectName + "\" was created, but it has no branch.";
        return result;
    }

    result.branches = *branchesResult.value;
    const auto selectedBranch = chooseBranch(result.branches, {});
    result.branchId = selectedBranch.id;

    auto versions = api.fetchVersions(selectedBranch.id);
    if (!versions.ok())
    {
        result.sessionExpired = versions.isUnauthorized();
        result.status = Status::warning(versions.errorMessage("Project created, but failed to load version history."));
        return result;
    }

    result.versions = std::move(*versions.value);
    versionhistory::sortNewestFirst(result.versions);
    result.selectedVersionId = versionhistory::chooseSelected(result.versions, {});
    result.status = Status::success(result.versions.empty() ? "Project created and main branch selected. No versions yet."
                                                            : "Project created and main branch selected.");
    return result;
}

BranchHistoryJobResult fetchHistory(const SignedInApi& api, const FetchHistoryInput& input)
{
    BranchHistoryJobResult result;
    result.branchId = input.branchId;

    auto versions = api.fetchVersions(input.branchId);
    if (!versions.ok())
        return failWith(result, versions, "Failed to load version history.");

    result.versions = std::move(*versions.value);
    versionhistory::sortNewestFirst(result.versions);
    result.selectedVersionId = versionhistory::chooseSelected(result.versions, input.preferredVersionId);

    result.status = Status::success(result.versions.empty()
                                        ? "Loaded branch \"" + input.branchName + "\". No versions yet."
                                        : "Loaded " + juce::String(static_cast<int>(result.versions.size()))
                                              + " version(s) for branch \"" + input.branchName + "\".");
    return result;
}

SaveVersionJobResult saveVersion(const SignedInApi& api, const SaveInput& input, const ReportProgress& report)
{
    SaveVersionJobResult result;

    if (input.projectId.isEmpty() || input.branchId.isEmpty())
    {
        result.errorMessage = "Choose or create a project before saving.";
        return result;
    }
    if (!input.projectFile.existsAsFile())
    {
        result.errorMessage = "Choose a project file before saving.";
        return result;
    }

    const auto message = input.message.trim();
    if (message.length() > kMaxMessageLength)
    {
        result.errorMessage = "Messages are limited to " + juce::String(kMaxMessageLength) + " characters.";
        return result;
    }

    // Taken before hashing: if the DAW saves again meanwhile, the next save sees a change.
    const auto sizeBeforeHashing = input.projectFile.getSize();
    const auto modTimeBeforeHashing = input.projectFile.getLastModificationTime().toMilliseconds();

    stemhub::versiontransfer::UploadRequest uploadRequest;
    uploadRequest.projectId = input.projectId;
    uploadRequest.branchId = input.branchId;
    uploadRequest.projectFile = input.projectFile;
    uploadRequest.message = message;
    uploadRequest.parentVersionId = input.parentVersionId;

    const auto uploaded = stemhub::versiontransfer::uploadVersion(api, uploadRequest, report);
    if (!uploaded.ok())
        return failWith(result, uploaded, "Failed to save the version.");

    result.savedCopy = { input.projectFile, uploaded.value->id, sizeBeforeHashing, modTimeBeforeHashing };
    input.workingCopies.record({ input.projectId, input.branchId, result.savedCopy });

    auto versions = api.fetchVersions(input.branchId);
    if (versions.ok())
    {
        versionhistory::sortNewestFirst(*versions.value);
        result.refreshedVersions = std::move(*versions.value);
        result.status = Status::success("Version saved successfully.");
    }
    else
    {
        result.status = Status::warning("Version saved. Refresh to see it in the history ("
                                        + versions.errorMessage("history unavailable") + ").");
    }

    return result;
}

RestoreVersionJobResult restoreVersion(const SignedInApi& api, const RestoreInput& input, const ReportProgress& report)
{
    RestoreVersionJobResult result;

    // StemhubSession checked the version and the project before starting the job.
    const auto restored = stemhub::versiontransfer::restoreVersion(api, { input.projectId, input.versionId, input.destinationFolder }, report);
    if (!restored.ok())
        return failWith(result, restored, "Failed to restore the version.");

    const auto& restoredProjectFile = *restored.value;
    result.restoredCopy = WorkingCopyBaseline::recordedNow(restoredProjectFile, input.versionId);
    input.workingCopies.record({ input.projectId, input.branchId, result.restoredCopy });
    result.status = Status::success("Version restored to " + restoredProjectFile.getParentDirectory().getFullPathName() + ".");
    return result;
}
}
