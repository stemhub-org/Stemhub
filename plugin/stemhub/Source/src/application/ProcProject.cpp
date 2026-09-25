#include <algorithm>
#include <type_traits>

#include "application/PluginProcessor.hpp"
#include "application/PluginProcessorHelpers.hpp"
#include "application/ProjectFileService.hpp"
#include "application/SessionCache.hpp"

using namespace stemhub::processorhelpers;

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
    workingCopy = std::move(result.workingCopy);
    setCurrentOpenedVersionId({});

    // Only a copy that was just restored is opened in the DAW. A file the user linked is usually
    // the project the DAW already has open, and reopening it would reload the session.
    if (result.didRestoreLatest)
    {
        if (!openFileHandler(selectedProjectFile))
        {
            setOperationState(OperationState::error);
            activeProjectStatusMessage = "Latest version restored to " + selectedProjectFile.getFullPathName()
                + ", but it could not be opened automatically. Open it from your DAW.";
            return;
        }

        setCurrentOpenedVersionId(workingCopy.versionId);
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

    // The backend only accepts a parent from the same branch. Refreshing the same branch leaves
    // the files on disk, and so the working copy, as they are.
    if (result.branchId != selectedBranchId)
        clearWorkingCopy();

    selectedBranchId = std::move(result.branchId);
    selectedBranchName = std::move(result.branchName);
    versionHistory = std::move(result.versions);
    selectedVersionId = chooseSelectedVersionId(versionHistory, result.selectedVersionId);

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

    if (result.refreshedVersions.has_value())
        versionHistory = std::move(*result.refreshedVersions);

    if (result.pushedCopy.isSet())
    {
        // The pushed file now holds the new version: it is the next save's parent, and what the
        // "no changes" check compares against.
        workingCopy = result.pushedCopy;
        selectedVersionId = workingCopy.versionId;
        setCurrentOpenedVersionId(workingCopy.versionId);
        stemhub::sessioncache::saveLastOpenedProjectFilePath(workingCopy.file.getFullPathName());
    }

    setOperationState(OperationState::idle);
    activeProjectStatusMessage = result.activeProjectStatusMessage.isNotEmpty()
        ? result.activeProjectStatusMessage
        : "Version saved successfully.";
}

void StemhubAudioProcessor::applyRestoreVersionResult(RestoreVersionJobResult result)
{
    juce::Logger::writeToLog("[Restore] Processor -> applyRestoreVersionResult restoredVersionId="
                             + result.restoredVersionId);

    if (hasError(result))
    {
        juce::Logger::writeToLog("[Restore] Processor -> apply failed: " + result.errorMessage);
        setOperationState(OperationState::error);
        activeProjectStatusMessage = result.errorMessage;
        return;
    }

    // The restored copy becomes the working file even if the DAW can't be asked to open it:
    // it is on disk, and the user can open it by hand.
    selectedProjectFile = result.restoredProjectFile;
    pendingProjectFile = selectedProjectFile;
    selectedVersionId = result.restoredVersionId;
    workingCopy = result.restoredCopy;
    stemhub::sessioncache::saveLastOpenedProjectFilePath(selectedProjectFile.getFullPathName());

    if (!openFileHandler(selectedProjectFile))
    {
        juce::Logger::writeToLog("[Restore] Processor -> opening the restored file failed: "
                                 + selectedProjectFile.getFullPathName());
        setOperationState(OperationState::error);
        activeProjectStatusMessage = "Version restored to " + selectedProjectFile.getFullPathName()
            + ", but it could not be opened automatically. Open it from your DAW.";
        return;
    }

    setCurrentOpenedVersionId(result.restoredVersionId);
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
    selectedProject = std::move(project);
    selectedBranchId = std::move(branchId);
    selectedBranchName = std::move(branchName);
    selectedProjectFile = std::move(projectFile);
    pendingProjectFile = selectedProjectFile;
    if (selectedProject.has_value())
        stemhub::sessioncache::saveProjectId(selectedProject->id);
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
    clearWorkingCopy();
    branches.clear();
    versionHistory.clear();
    activeProjectStatusMessage.clear();
}

void StemhubAudioProcessor::clearWorkingCopy()
{
    workingCopy = {};
    currentOpenedVersionId.clear();
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
    return workingCopy.describes(workingFile) && workingCopy.isUnchanged();
}

juce::String StemhubAudioProcessor::getParentVersionForNextSave(const juce::File& projectFile) const
{
    if (workingCopy.describes(projectFile))
        return workingCopy.versionId;

    return versionHistory.empty() ? juce::String() : versionHistory.front().id;
}

void StemhubAudioProcessor::requestOpenProject(juce::String projectId, juce::File localProjectFile, const bool restoreLatestIfSafe)
{
    if (isWriteOperationInProgress())
        return;

    // The working file of the project open until now is not a copy of another project.
    if (selectedProject.has_value() && selectedProject->id != projectId && localProjectFile == selectedProjectFile)
        localProjectFile = juce::File();

    setOperationState(OperationState::loadingProjects);
    sendChangeMessage();

    stemhub::usecases::OpenProjectInput input;
    input.projectId = std::move(projectId);
    input.localProjectFile = std::move(localProjectFile);
    input.availableProjects = projects;
    input.token = access_tkn;
    input.restoreLatestIfSafe = restoreLatestIfSafe;
    input.localCopy = workingCopy;
    input.managedWorkingCopyFolder = managedWorkingCopyFolder;

    enqueueBackgroundTask([input, selectionRequestId = beginSelectionRequest()](const IProjectApi& api)
                              -> BackgroundJobPayload
    {
        auto result = stemhub::usecases::openProject(api, input);
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

    enqueueBackgroundTask([input = stemhub::usecases::CreateProjectInput { std::move(localProjectFile), access_tkn },
                           selectionRequestId = beginSelectionRequest()](const IProjectApi& api) -> BackgroundJobPayload
    {
        auto result = stemhub::usecases::createProject(api, input);
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

    stemhub::usecases::FetchHistoryInput input;
    input.branchId = std::move(branchId);
    input.branchName = branchIt->name;
    input.token = access_tkn;
    input.localProjectFile = stemhub::projectfiles::resolveEffectiveProjectFile(selectedProjectFile, pendingProjectFile);

    enqueueBackgroundTask([input, selectionRequestId = beginSelectionRequest()](const IProjectApi& api)
                              -> BackgroundJobPayload
    {
        auto result = stemhub::usecases::fetchHistory(api, input);
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

    setOperationState(OperationState::pulling);
    setActiveProjectStatusMessage("Refreshing version history...");

    stemhub::usecases::FetchHistoryInput input;
    input.branchId = selectedBranchId;
    input.branchName = branchNameIt != branches.end() ? branchNameIt->name : selectedBranchName;
    input.preferredVersionId = selectedVersionId;
    input.token = access_tkn;
    input.localProjectFile = stemhub::projectfiles::resolveEffectiveProjectFile(selectedProjectFile, pendingProjectFile);

    enqueueBackgroundTask([input, selectionRequestId = beginSelectionRequest()](const IProjectApi& api)
                              -> BackgroundJobPayload
    {
        auto result = stemhub::usecases::fetchHistory(api, input);
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

    stemhub::usecases::PushInput input;
    input.projectFile = projectFile;
    input.projectId = selectedProject.has_value() ? selectedProject->id : juce::String();
    input.branchId = selectedBranchId;
    input.parentVersionId = getParentVersionForNextSave(projectFile);
    input.commitMessage = std::move(commitMessage);
    input.dawName = std::move(dawName);
    input.token = access_tkn;

    enqueueBackgroundTask([input](const IProjectApi& api) -> BackgroundJobPayload
    {
        return stemhub::usecases::pushVersion(api, input);
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

    setOperationState(OperationState::restoring);
    setActiveProjectStatusMessage("Restoring selected version...");
    sendChangeMessage();

    stemhub::usecases::RestoreInput input;
    input.projectId = selectedProject->id;
    input.versionId = versionId;
    input.destinationFolder = stemhub::projectfiles::chooseRestoreFolder(
        projectFolder,
        stemhub::projectfiles::resolveRestoreProjectName(versionHistory, versionId, selectedProject->name),
        versionId);
    input.token = access_tkn;

    enqueueBackgroundTask([input](const IProjectApi& api) -> BackgroundJobPayload
    {
        return stemhub::usecases::restoreVersion(api, input);
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
