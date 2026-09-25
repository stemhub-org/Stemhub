#include <algorithm>
#include <type_traits>

#include "application/PluginProcessor.hpp"
#include "application/PluginProcessorHelpers.hpp"
#include "application/ProjectFileService.hpp"
#include "application/SessionCache.hpp"

using namespace stemhub::processorhelpers;

namespace
{
juce::String resolveOpenedVersionFromPath(const juce::File& projectFile,
                                         const std::vector<VersionSummary>& versions,
                                         const juce::String& requestedVersionId)
{
    const auto resolvedVersionId = resolveVersionIdFromProjectPath(projectFile, versions);
    if (resolvedVersionId.isNotEmpty())
        return resolvedVersionId;

    if (requestedVersionId.isNotEmpty())
    {
        juce::Logger::writeToLog("[Restore] Processor -> using requested version id fallback="
                                 + requestedVersionId);
    }
    else
    {
        juce::Logger::writeToLog("[Restore] Processor -> no requested version id available for fallback");
    }

    return requestedVersionId;
}
}

void StemhubAudioProcessor::applyProjectActivationResult(ProjectActivationJobResult result)
{
    if (!isCurrentSelectionRequest(result.selectionRequestId))
        return;

    if (hasError(result))
    {
        if (result.fromCachedProjectRestore)
            stemhub::sessioncache::clearProjectContext();

        setOperationState(OperationState::error);
        projectSelectionStatusMessage = result.errorMessage;
        return;
    }

    if (result.refreshProjects)
    {
        const auto count = static_cast<int>(result.projects.size());
        projects = std::move(result.projects);
        projectSelectionStatusMessage = count == 0 ? "No projects found."
                                                   : "Loaded " + juce::String(count) + " project(s).";
    }

    branches = std::move(result.branches);
    versionHistory = std::move(result.versions);
    selectedVersionId = chooseSelectedVersionId(versionHistory, result.selectedVersionId);
    selectProject(*result.selectedProject, result.branchId, result.branchName,
            std::move(result.projectFile));
    if (result.didRestoreLatest)
        setWorkingCopyContext(selectedProjectFile,
                              result.workingVersionId,
                              result.restoredFileSizeBytes,
                              result.restoredFileModTimeMs);
    else if (result.workingVersionId.isNotEmpty())
        setWorkingCopyContext(selectedProjectFile, result.workingVersionId);
    else
        clearWorkingCopyContext();
    setCurrentOpenedVersionId({});

    // Only a copy that was just restored is opened in the DAW. A file the user linked is usually
    // the project the DAW already has open, and reopening it would reload the session.
    if (result.didRestoreLatest)
    {
        if (!openFileHandler(selectedProjectFile))
        {
            setOperationState(OperationState::error);
            activeProjectStatusMessage = "Latest version restored, but failed to open it: "
                + selectedProjectFile.getFullPathName();
            return;
        }

        setCurrentOpenedVersionId(result.workingVersionId);
    }
    if (selectedProjectFile.existsAsFile() && !selectedProjectFile.isDirectory())
        stemhub::sessioncache::saveLastOpenedProjectFilePath(selectedProjectFile.getFullPathName());

    setOperationState(OperationState::idle);
    activeProjectStatusMessage = std::move(result.activeProjectStatusMessage);
}

void StemhubAudioProcessor::applyBranchHistoryResult(BranchHistoryJobResult result)
{
    if (!isCurrentSelectionRequest(result.selectionRequestId))
        return;

    if (hasError(result))
    {
        setOperationState(OperationState::error);
        activeProjectStatusMessage = result.errorMessage;
        return;
    }

    const auto isSameBranch = result.branchId == selectedBranchId;
    selectedBranchId = std::move(result.branchId);
    selectedBranchName = std::move(result.branchName);
    versionHistory = std::move(result.versions);
    selectedVersionId = chooseSelectedVersionId(versionHistory, result.selectedVersionId);
    versionControlService.setCurrentProjectContext(makeProjectVersionContext(selectedProject,
                                                                             selectedBranchId,
                                                                             versionHistory));
    if (result.projectFile.existsAsFile() && result.workingVersionId.isNotEmpty())
    {
        setWorkingCopyContext(result.projectFile, result.workingVersionId);
        setCurrentOpenedVersionId({});
    }
    else if (!isSameBranch)
    {
        // The backend only accepts a parent from the same branch.
        clearWorkingCopyContext();
    }
    // A refresh of the same branch leaves the files on disk, and so the baseline, untouched.

    if (result.projectFile.existsAsFile())
    {
        selectedProjectFile = result.projectFile;
        pendingProjectFile = selectedProjectFile;

        if (!openFileHandler(selectedProjectFile))
        {
            setOperationState(OperationState::error);
            activeProjectStatusMessage = "Workspace loaded, but failed to open local project file: "
                + selectedProjectFile.getFullPathName();
            return;
        }
        if (result.workingVersionId.isNotEmpty())
            setCurrentOpenedVersionId(result.workingVersionId);
    }

    setOperationState(OperationState::idle);
    activeProjectStatusMessage = std::move(result.activeProjectStatusMessage);
}

void StemhubAudioProcessor::applyPushVersionResult(PushVersionJobResult result)
{
    if (hasError(result))
    {
        setOperationState(OperationState::error);
        activeProjectStatusMessage = result.errorMessage;
        return;
    }

    // Copied, not moved: the id is used several times below.
    const auto pushedVersionId = result.pushedVersionId;

    if (result.refreshedVersions.has_value())
    {
        versionHistory = std::move(*result.refreshedVersions);
        versionControlService.setCurrentProjectContext(makeProjectVersionContext(selectedProject,
                                                                                 selectedBranchId,
                                                                                 versionHistory));
    }

    versionControlService.setLastVersionId(pushedVersionId);
    if (pushedVersionId.isNotEmpty())
    {
        selectedVersionId = pushedVersionId;
        // The pushed file is now the working copy of the new version: it becomes the next
        // save's parent, and the "no changes" check compares against it.
        setWorkingCopyContext(result.pushedProjectFile,
                              pushedVersionId,
                              result.pushedFileSizeBytes,
                              result.pushedFileModTimeMs);
        setCurrentOpenedVersionId(pushedVersionId);
        if (result.pushedProjectFile.existsAsFile())
            stemhub::sessioncache::saveLastOpenedProjectFilePath(result.pushedProjectFile.getFullPathName());
    }

    setOperationState(OperationState::idle);
    activeProjectStatusMessage = result.activeProjectStatusMessage.isNotEmpty()
        ? result.activeProjectStatusMessage
        : "Version saved successfully.";
}

void StemhubAudioProcessor::applyRestoreVersionResult(RestoreVersionJobResult result)
{
    juce::Logger::writeToLog("[Restore] Processor -> applyRestoreVersionResult start. restoredVersionId="
                             + result.restoredVersionId);
    if (result.restoredProjectFile.existsAsFile())
        juce::Logger::writeToLog("[Restore] Processor -> restored file=" + result.restoredProjectFile.getFullPathName());

    if (hasError(result))
    {
        juce::Logger::writeToLog("[Restore] Processor -> apply failed: " + result.errorMessage);
        setOperationState(OperationState::error);
        activeProjectStatusMessage = result.errorMessage;
        return;
    }

        if (result.restoredProjectFile.existsAsFile())
        {
            selectedProjectFile = result.restoredProjectFile;
            pendingProjectFile = selectedProjectFile;
            const auto previousOpenedVersionId = currentOpenedVersionId;
        const auto previousWorkingCopyFile = workingCopyProjectFile;
        const auto previousWorkingCopyVersionId = workingCopyVersionId;
        const auto previousWorkingCopyFileSize = workingCopyFileSize;
        const auto previousWorkingCopyModTime = workingCopyFileModTime;
        selectedVersionId = resolveOpenedVersionFromPath(result.restoredProjectFile, versionHistory, result.restoredVersionId);
        setWorkingCopyContext(selectedProjectFile, selectedVersionId);
        versionControlService.setLastVersionId(selectedVersionId);
        setCurrentOpenedVersionId(selectedVersionId);
        juce::Logger::writeToLog("[Restore] Processor -> resolved restoredVersionId="
                                 + selectedVersionId
                                 + " (requested="
                                 + result.restoredVersionId
                                 + ")");
        juce::Logger::writeToLog("[Restore] Processor -> applied selectedVersionId=" + selectedVersionId);
        if (!openFileHandler(selectedProjectFile))
        {
            juce::Logger::writeToLog("[Restore] Processor -> openInSystem failed: " + selectedProjectFile.getFullPathName());
            setOperationState(OperationState::error);
            activeProjectStatusMessage = "Version restored, but failed to open project file: "
                + selectedProjectFile.getFullPathName();
            if (previousWorkingCopyVersionId.isNotEmpty() || previousWorkingCopyFile.existsAsFile())
            {
                workingCopyProjectFile = previousWorkingCopyFile;
                workingCopyVersionId = previousWorkingCopyVersionId;
                workingCopyFileSize = previousWorkingCopyFileSize;
                workingCopyFileModTime = previousWorkingCopyModTime;
            }
            setCurrentOpenedVersionId(previousOpenedVersionId);
            return;
        }
            juce::Logger::writeToLog("[Restore] Processor -> openInSystem succeeded");
            stemhub::sessioncache::saveLastOpenedProjectFilePath(selectedProjectFile.getFullPathName());
        }
    else
    {
        juce::Logger::writeToLog("[Restore] Processor -> no restoredProjectFile in result");
    }

    setOperationState(OperationState::idle);
    activeProjectStatusMessage = result.activeProjectStatusMessage;
}

void StemhubAudioProcessor::applyBackgroundResult(BackgroundJobResult result)
{
    std::visit([this](auto&& payload)
    {
        using Payload = std::decay_t<decltype(payload)>;

        if constexpr (std::is_same_v<Payload, AuthRequestResult>)
            applyAuthRequestResult(std::move(payload));
        else if constexpr (std::is_same_v<Payload, ProjectActivationJobResult>)
            applyProjectActivationResult(std::move(payload));
        else if constexpr (std::is_same_v<Payload, BranchHistoryJobResult>)
            applyBranchHistoryResult(std::move(payload));
        else if constexpr (std::is_same_v<Payload, PushVersionJobResult>)
            applyPushVersionResult(std::move(payload));
        else if constexpr (std::is_same_v<Payload, RestoreVersionJobResult>)
            applyRestoreVersionResult(std::move(payload));
    }, std::move(result.payload));
}

void StemhubAudioProcessor::setActiveProjectStatusMessage(juce::String message)
{
    activeProjectStatusMessage = std::move(message);
    sendChangeMessage();
}

void StemhubAudioProcessor::setPendingProjectFile(const juce::File& file)
{
    pendingProjectFile = file;
    sendChangeMessage();
}

void StemhubAudioProcessor::selectProject(Project project, juce::String branchId, juce::String branchName, juce::File projectFile)
{
    versionControlService.clearProjectContext();
    selectedProject = std::move(project);
    selectedBranchId = std::move(branchId);
    selectedBranchName = std::move(branchName);
    selectedProjectFile = std::move(projectFile);
    pendingProjectFile = selectedProjectFile;
    if (selectedProject.has_value())
        stemhub::sessioncache::saveProjectId(selectedProject->id);
    versionControlService.setCurrentProjectContext(makeProjectVersionContext(selectedProject,
                                                                             selectedBranchId,
                                                                             versionHistory));
    sessionState.uiState = UIState::dashboard;
    sendChangeMessage();
}

void StemhubAudioProcessor::clearSelectedProject() noexcept
{
    selectedProject.reset();
    selectedBranchId.clear();
    selectedBranchName.clear();
    selectedVersionId.clear();
    selectedProjectFile = juce::File();
    clearWorkingCopyContext();
    branches.clear();
    versionHistory.clear();
    versionControlService.clearProjectContext();
    activeProjectStatusMessage.clear();
}

void StemhubAudioProcessor::setWorkingCopyContext(const juce::File& workingFile, const juce::String& versionId)
{
    setWorkingCopyContext(workingFile,
                          versionId,
                          workingFile.getSize(),
                          workingFile.getLastModificationTime().toMilliseconds());
}

void StemhubAudioProcessor::setWorkingCopyContext(const juce::File& workingFile,
                                                  const juce::String& versionId,
                                                  const juce::int64 fileSizeBytes,
                                                  const juce::int64 fileModTimeMs)
{
    if (!workingFile.existsAsFile() || versionId.isEmpty())
    {
        clearWorkingCopyContext();
        return;
    }

    workingCopyProjectFile = workingFile;
    workingCopyVersionId = versionId;
    workingCopyFileSize = fileSizeBytes;
    workingCopyFileModTime = fileModTimeMs;
}

bool StemhubAudioProcessor::isWriteOperationInProgress() const noexcept
{
    return sessionState.operationState == OperationState::committing
        || sessionState.operationState == OperationState::restoring;
}

void StemhubAudioProcessor::setCurrentOpenedVersionId(juce::String versionId)
{
    const auto previousVersion = currentOpenedVersionId;
    currentOpenedVersionId = std::move(versionId);
    juce::Logger::writeToLog("[Restore] Processor -> setCurrentOpenedVersionId old="
                             + previousVersion
                             + " new="
                             + currentOpenedVersionId
                             + " changed="
                             + (previousVersion == currentOpenedVersionId ? "false" : "true"));
}

void StemhubAudioProcessor::clearWorkingCopyContext()
{
    workingCopyProjectFile = juce::File();
    workingCopyVersionId.clear();
    workingCopyFileSize = 0;
    workingCopyFileModTime = 0;
    currentOpenedVersionId.clear();
}

uint64_t StemhubAudioProcessor::beginSelectionRequest() noexcept
{
    return ++activeSelectionRequestId;
}

bool StemhubAudioProcessor::isCurrentSelectionRequest(uint64_t requestId) const noexcept
{
    return requestId != 0 && requestId == activeSelectionRequestId.load();
}

bool StemhubAudioProcessor::hasCleanWorkingCopy(const juce::File& workingFile) const
{
    if (!workingCopyVersionId.isNotEmpty())
        return false;

    if (workingFile != workingCopyProjectFile)
        return false;

    if (!workingFile.existsAsFile())
        return false;

    return workingCopyFileSize == workingFile.getSize()
        && workingCopyFileModTime == workingFile.getLastModificationTime().toMilliseconds();
}

void StemhubAudioProcessor::requestOpenProject(juce::String projectId, juce::File localProjectFile, const bool restoreLatestIfSafe)
{
    if (isWriteOperationInProgress())
        return;

    // The working file of the project open until now is not a copy of another project.
    if (selectedProject.has_value() && selectedProject->id != projectId && localProjectFile == selectedProjectFile)
        localProjectFile = juce::File();

    LocalCopyState localCopy;
    if (localProjectFile.existsAsFile() && localProjectFile == workingCopyProjectFile)
    {
        localCopy.baseVersionId = workingCopyVersionId;
        localCopy.isUnchanged = hasCleanWorkingCopy(localProjectFile);
    }

    setOperationState(OperationState::loadingProjects);
    sendChangeMessage();

    const auto projectsSnapshot = projects;
    const auto token = access_tkn;
    const auto selectionRequestId = beginSelectionRequest();
    enqueueBackgroundTask([this,
                           requestedProjectId = std::move(projectId),
                           requestedProjectFile = std::move(localProjectFile),
                           restoreLatestIfSafe,
                           localCopy,
                           workingCopyBase = managedWorkingCopyFolder,
                           projectsSnapshot,
                           selectionRequestId,
                           token]() -> BackgroundJobPayload
    {
        auto result = performOpenProjectRequest(requestedProjectId,
                                                requestedProjectFile,
                                                projectsSnapshot,
                                                token,
                                                restoreLatestIfSafe,
                                                localCopy,
                                                workingCopyBase);
        result.selectionRequestId = selectionRequestId;
        return result;
    });
}

void StemhubAudioProcessor::requestCreateProject(juce::File localProjectFile)
{
    if (isWriteOperationInProgress())
        return;

    setOperationState(OperationState::loadingProjects);
    sendChangeMessage();

    const auto token = access_tkn;
    const auto selectionRequestId = beginSelectionRequest();
    enqueueBackgroundTask([this,
                           requestedProjectFile = std::move(localProjectFile),
                           selectionRequestId,
                           token]() -> BackgroundJobPayload
    {
        auto result = performCreateProjectRequest(requestedProjectFile, token);
        result.selectionRequestId = selectionRequestId;
        return result;
    });
}

void StemhubAudioProcessor::requestSelectBranch(juce::String branchId)
{
    if (isWriteOperationInProgress())
        return;

    if (!selectedProject.has_value())
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Choose a project before selecting a workspace.");
        return;
    }

    const auto branchIt = std::find_if(branches.begin(), branches.end(), [&branchId](const Branch& branch)
    {
        return branch.id == branchId;
    });

    if (branchIt == branches.end())
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Selected workspace is no longer available.");
        return;
    }

    setOperationState(OperationState::pulling);
    setActiveProjectStatusMessage("Loading workspace history...");

    const auto token = access_tkn;
    const auto branchName = branchIt->name;
    const auto localProjectFile = stemhub::projectfiles::resolveEffectiveProjectFile(selectedProjectFile, pendingProjectFile);
    const auto selectionRequestId = beginSelectionRequest();
    enqueueBackgroundTask([this,
                           requestedBranchId = std::move(branchId),
                           requestedBranchName = std::move(branchName),
                           localProjectFile,
                           selectionRequestId,
                           token]() -> BackgroundJobPayload
    {
        auto result = performFetchBranchHistoryRequest(requestedBranchId, requestedBranchName, {}, token, localProjectFile);
        result.selectionRequestId = selectionRequestId;
        return result;
    });
}

void StemhubAudioProcessor::requestRefreshVersionHistory()
{
    if (isWriteOperationInProgress())
        return;

    if (!hasProjectAndBranchSelected(selectedProject, selectedBranchId))
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Choose a project and workspace before refreshing history.");
        return;
    }

    const auto branchNameIt = std::find_if(branches.begin(), branches.end(), [this](const Branch& branch)
    {
        return branch.id == selectedBranchId;
    });
    const auto branchName = branchNameIt != branches.end() ? branchNameIt->name : selectedBranchName;

    setOperationState(OperationState::pulling);
    setActiveProjectStatusMessage("Refreshing version history...");

    const auto token = access_tkn;
    const auto branchId = selectedBranchId;
    const auto preferredVersionId = selectedVersionId;
    const auto localProjectFile = stemhub::projectfiles::resolveEffectiveProjectFile(selectedProjectFile, pendingProjectFile);
    const auto selectionRequestId = beginSelectionRequest();
    enqueueBackgroundTask([this,
                           branchId,
                           branchName,
                           preferredVersionId,
                           localProjectFile,
                           selectionRequestId,
                           token]() -> BackgroundJobPayload
    {
        auto result = performFetchBranchHistoryRequest(branchId, branchName, preferredVersionId, token, localProjectFile);
        result.selectionRequestId = selectionRequestId;
        return result;
    });
}

void StemhubAudioProcessor::requestPushVersion(juce::String commitMessage, juce::String dawName)
{
    // One save at a time, and never while history or a project is loading: their results
    // would land on top of the pushed version.
    const auto operationState = sessionState.operationState;
    if (operationState != OperationState::idle && operationState != OperationState::error)
        return;

    const auto projectFile = stemhub::projectfiles::resolveEffectiveProjectFile(selectedProjectFile, pendingProjectFile);
    if (hasCleanWorkingCopy(projectFile))
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("No local project file changes detected on disk. Save the project in FL Studio first, then click Save again.");
        return;
    }

    setOperationState(OperationState::committing);
    sendChangeMessage();

    // Everything the job needs is captured here, on the message thread.
    const auto projectRootDirectory = projectFile.existsAsFile() ? projectFile.getParentDirectory() : juce::File();
    const auto parentVersionId = workingCopyVersionId.isNotEmpty() ? workingCopyVersionId
                                                                   : versionControlService.getLastVersionId();
    enqueueBackgroundTask([this,
                           selectedFile = projectFile,
                           selectedProjectRoot = projectRootDirectory,
                           project = selectedProject,
                           selectedBranch = selectedBranchId,
                           parentVersionId,
                           requestedCommitMessage = std::move(commitMessage),
                           requestedDawName = std::move(dawName),
                           token = access_tkn]() -> BackgroundJobPayload
    {
        return performPushVersionRequest(selectedFile,
                                                          selectedProjectRoot,
                                                          project,
                                                          selectedBranch,
                                                          parentVersionId,
                                                          requestedCommitMessage,
                                                          requestedDawName,
                                                          token);
    });
}

void StemhubAudioProcessor::requestRestoreVersion(const juce::String& versionId, const juce::File& projectFolder)
{
    const auto operationState = sessionState.operationState;
    if (operationState != OperationState::idle && operationState != OperationState::error)
        return;

    if (!hasProjectAndBranchSelected(selectedProject, selectedBranchId))
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Choose a project before restoring.");
        return;
    }
    if (!projectFolder.isDirectory())
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Choose a valid restore destination folder.");
        return;
    }
    if (versionId.isEmpty())
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Select a version before restoring.");
        return;
    }

    const auto restoreDir = stemhub::projectfiles::chooseRestoreFolder(
        projectFolder,
        stemhub::projectfiles::resolveRestoreProjectName(versionHistory, versionId, selectedProject->name),
        versionId);

    setOperationState(OperationState::restoring);
    setActiveProjectStatusMessage("Restoring selected version...");
    sendChangeMessage();

    enqueueBackgroundTask([this,
                           projectId = selectedProject->id,
                           requestedVersionId = versionId,
                           requestedRestoreDir = restoreDir,
                           token = access_tkn]() -> BackgroundJobPayload
    {
        return performRestoreVersionRequest(projectId, requestedVersionId, requestedRestoreDir, token);
    });
}

void StemhubAudioProcessor::setSelectedVersionId(juce::String versionId)
{
    selectedVersionId = std::move(versionId);
    sendChangeMessage();
}

void StemhubAudioProcessor::flushPendingBackgroundResultsForTesting()
{
    handleAsyncUpdate();
}

void StemhubAudioProcessor::handleAsyncUpdate()
{
    const auto didApply = backgroundJobs.flushResults([this](auto&& result)
    {
        applyBackgroundResult(std::forward<decltype(result)>(result));
    });

    if (didApply)
        sendChangeMessage();
}
