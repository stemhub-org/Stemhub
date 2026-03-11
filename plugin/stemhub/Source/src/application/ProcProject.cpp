#include <algorithm>
#include <type_traits>

#include "application/PluginProcessor.hpp"
#include "application/PluginProcessorHelpers.hpp"
#include "application/ProjectFileService.hpp"
#include "application/SessionCache.hpp"

using namespace stemhub::processorhelpers;

namespace
{
juce::String extractVersionPrefixFromPathPart(const juce::String& value)
{
    const auto lastDash = value.lastIndexOfChar('-');
    if (lastDash < 0 || lastDash + 8 >= value.length())
        return {};

    const auto candidate = value.substring(lastDash + 1, lastDash + 9);
    for (int i = 0; i < candidate.length(); ++i)
    {
        const auto ch = candidate.toLowerCase()[i];
        const bool isHexChar = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
        if (!isHexChar)
            return {};
    }
    return candidate;
}

juce::String resolveOpenedVersionFromPath(const juce::File& projectFile,
                                         const std::vector<VersionSummary>& versions,
                                         const juce::String& requestedVersionId)
{
    const auto fileName = projectFile.getFileNameWithoutExtension();
    const auto parentName = projectFile.getParentDirectory().getFileName();
    const juce::String candidates[] = { fileName, parentName };

    for (const auto& candidate : candidates)
    {
        const auto shortVersionId = extractVersionPrefixFromPathPart(candidate);
        if (shortVersionId.isEmpty())
            continue;

        const auto it = std::find_if(versions.begin(), versions.end(),
                                    [&shortVersionId](const VersionSummary& version)
                                    {
                                        if (version.id.length() < 8)
                                            return false;
                                        return version.id.substring(0, 8).compareIgnoreCase(shortVersionId) == 0;
                                    });
        if (it != versions.end())
        {
            juce::Logger::writeToLog("[Restore] Processor -> resolved version id from path segment="
                                     + candidate
                                     + ", prefix="
                                     + shortVersionId
                                     + ", full="
                                     + it->id);
            return it->id;
        }
        juce::Logger::writeToLog("[Restore] Processor -> no match for path segment="
                                 + candidate
                                 + ", prefix="
                                 + shortVersionId);
    }

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
    if (hasError(result))
    {
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
    if (result.workingVersionId.isNotEmpty())
        setWorkingCopyContext(selectedProjectFile, result.workingVersionId);
    else
        clearWorkingCopyContext();
    setCurrentOpenedVersionId({});

    if (result.shouldAutoOpenLocalFile
        && selectedProjectFile.existsAsFile()
        && !stemhub::projectfiles::openInSystem(selectedProjectFile))
    {
        setOperationState(OperationState::error);
        activeProjectStatusMessage = "Project loaded, but failed to open local project file: "
            + selectedProjectFile.getFullPathName();
        return;
    }
    if (selectedProjectFile.existsAsFile() && result.shouldAutoOpenLocalFile && result.workingVersionId.isNotEmpty())
        setCurrentOpenedVersionId(result.workingVersionId);

    setOperationState(OperationState::idle);
    activeProjectStatusMessage = std::move(result.activeProjectStatusMessage);
}

void StemhubAudioProcessor::applyBranchHistoryResult(BranchHistoryJobResult result)
{
    if (hasError(result))
    {
        setOperationState(OperationState::error);
        activeProjectStatusMessage = result.errorMessage;
        return;
    }

    selectedBranchId = std::move(result.branchId);
    selectedBranchName = std::move(result.branchName);
    versionHistory = std::move(result.versions);
    selectedVersionId = chooseSelectedVersionId(versionHistory, result.selectedVersionId);
    versionControlService.setCurrentProjectContext(makeProjectVersionContext(selectedProject,
                                                                             selectedBranchId,
                                                                             versionHistory));
    if (result.workingVersionId.isNotEmpty())
        setWorkingCopyContext(result.projectFile, result.workingVersionId);
    else
        clearWorkingCopyContext();
    setCurrentOpenedVersionId({});
    if (result.projectFile.existsAsFile())
    {
        selectedProjectFile = result.projectFile;
        pendingProjectFile = selectedProjectFile;

        if (!stemhub::projectfiles::openInSystem(selectedProjectFile))
        {
            setOperationState(OperationState::error);
            activeProjectStatusMessage = "Branch loaded, but failed to open local project file: "
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

    versionControlService.setLastVersionId(std::move(result.pushedVersionId));
    if (result.pushedVersionId.isNotEmpty())
    {
        selectedVersionId = result.pushedVersionId;
        setWorkingCopyContext(selectedProjectFile, result.pushedVersionId);
        setCurrentOpenedVersionId(result.pushedVersionId);
    }

    setOperationState(OperationState::idle);
    activeProjectStatusMessage = result.activeProjectStatusMessage.isNotEmpty()
        ? result.activeProjectStatusMessage
        : "Version pushed successfully.";
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
        if (!stemhub::projectfiles::openInSystem(selectedProjectFile))
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

void StemhubAudioProcessor::setProjectSelectionStatusMessage(juce::String message)
{
    projectSelectionStatusMessage = std::move(message);
    sendChangeMessage();
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
    if (!workingFile.existsAsFile() || versionId.isEmpty())
    {
        clearWorkingCopyContext();
        return;
    }

    workingCopyProjectFile = workingFile;
    workingCopyVersionId = versionId;
    workingCopyFileSize = workingFile.getSize();
    workingCopyFileModTime = workingFile.getLastModificationTime().toMilliseconds();
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

juce::String StemhubAudioProcessor::getCurrentOpenedVersionLabel() const
{
    const auto fileToInspect = stemhub::projectfiles::resolveEffectiveProjectFile(selectedProjectFile,
                                                                              pendingProjectFile);
    if (!fileToInspect.existsAsFile())
        return "Current version: not available";

    if (currentOpenedVersionId.isNotEmpty() && currentOpenedVersionId == workingCopyVersionId
        && workingCopyProjectFile == fileToInspect)
    {
        if (hasCleanWorkingCopy(fileToInspect))
            return "Current version: " + currentOpenedVersionId;

        return "Current version: " + currentOpenedVersionId + " (modified locally)";
    }

    if (currentOpenedVersionId.isNotEmpty())
    {
        if (workingCopyProjectFile != fileToInspect || workingCopyVersionId != currentOpenedVersionId)
            return "Current version: " + currentOpenedVersionId + " (working copy out of sync)";

        if (hasCleanWorkingCopy(fileToInspect))
            return "Current version: " + currentOpenedVersionId;

        return "Current version: " + currentOpenedVersionId + " (modified locally)";
    }

    if (workingCopyProjectFile != fileToInspect)
        return "Current version: unknown";

    if (workingCopyVersionId.isEmpty())
        return "Current version: unknown";

    if (hasCleanWorkingCopy(fileToInspect))
        return "Current version: " + workingCopyVersionId;

    return "Current version: " + workingCopyVersionId + " (modified locally)";
}

void StemhubAudioProcessor::requestOpenProject(juce::String projectId, juce::File localProjectFile, const bool preferRemoteLatest)
{
    setOperationState(OperationState::loadingProjects);
    sendChangeMessage();

    const auto projectsSnapshot = projects;
    const auto token = access_tkn;
    enqueueBackgroundTask([this,
                           requestedProjectId = std::move(projectId),
                           requestedProjectFile = std::move(localProjectFile),
                           requestedPreferRemoteLatest = preferRemoteLatest,
                           projectsSnapshot,
                           token]() -> BackgroundJobPayload
    {
        return performOpenProjectRequest(requestedProjectId,
                                        requestedProjectFile,
                                        projectsSnapshot,
                                        token,
                                        requestedPreferRemoteLatest);
    });
}

void StemhubAudioProcessor::requestCreateProject(juce::File localProjectFile)
{
    setOperationState(OperationState::loadingProjects);
    sendChangeMessage();

    const auto token = access_tkn;
    enqueueBackgroundTask([this,
                           requestedProjectFile = std::move(localProjectFile),
                           token]() -> BackgroundJobPayload
    {
        return performCreateProjectRequest(requestedProjectFile, token);
    });
}

void StemhubAudioProcessor::requestSelectBranch(juce::String branchId)
{
    if (!selectedProject.has_value())
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Choose a project before selecting a branch.");
        return;
    }

    const auto branchIt = std::find_if(branches.begin(), branches.end(), [&branchId](const Branch& branch)
    {
        return branch.id == branchId;
    });

    if (branchIt == branches.end())
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Selected branch is no longer available.");
        return;
    }

    setOperationState(OperationState::pulling);
    setActiveProjectStatusMessage("Loading branch history...");

    const auto token = access_tkn;
    const auto branchName = branchIt->name;
    enqueueBackgroundTask([this,
                           requestedBranchId = std::move(branchId),
                           requestedBranchName = std::move(branchName),
                           token]() -> BackgroundJobPayload
    {
        return performFetchBranchHistoryRequest(requestedBranchId, requestedBranchName, {}, token);
    });
}

void StemhubAudioProcessor::requestRefreshVersionHistory()
{
    if (!hasProjectAndBranchSelected(selectedProject, selectedBranchId))
    {
        setOperationState(OperationState::error);
        setActiveProjectStatusMessage("Choose a project and branch before refreshing history.");
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
    const auto effectiveProjectFile = stemhub::projectfiles::resolveEffectiveProjectFile(selectedProjectFile, pendingProjectFile);
    const auto isWorkingCopyClean = !effectiveProjectFile.existsAsFile()
        ? true
        : hasCleanWorkingCopy(effectiveProjectFile);
    const auto preferredVersionId = isWorkingCopyClean ? juce::String() : selectedVersionId;
    enqueueBackgroundTask([this,
                           branchId,
                           branchName,
                           preferredVersionId,
                           token]() -> BackgroundJobPayload
    {
        return performFetchBranchHistoryRequest(branchId, branchName, preferredVersionId, token);
    });
}

void StemhubAudioProcessor::requestPushVersion(juce::String commitMessage, juce::String dawName)
{
    setOperationState(OperationState::committing);
    sendChangeMessage();

    const auto projectFile = stemhub::projectfiles::resolveEffectiveProjectFile(selectedProjectFile, pendingProjectFile);
    const auto projectRootDirectory = projectFile.existsAsFile() ? projectFile.getParentDirectory() : juce::File();
    const auto project = selectedProject;
    const auto branchId = selectedBranchId;
    enqueueBackgroundTask([this,
                           selectedFile = std::move(projectFile),
                           selectedProjectRoot = std::move(projectRootDirectory),
                           project,
                           selectedBranch = std::move(branchId),
                           requestedCommitMessage = std::move(commitMessage),
                           requestedDawName = std::move(dawName)]() mutable -> BackgroundJobPayload
    {
        return performPushVersionRequest(selectedFile,
                                         selectedProjectRoot,
                                         project,
                                         selectedBranch,
                                         requestedCommitMessage,
                                         requestedDawName);
    });
}

void StemhubAudioProcessor::requestRestoreVersion(const juce::String& versionId, const juce::File& projectFolder)
{
    juce::Logger::writeToLog("[Restore] Processor -> requestRestoreVersion called. versionId=" + versionId
                             + ", projectFolder=" + projectFolder.getFullPathName()
                             + ", hasProjectAndBranch=" + (hasProjectAndBranchSelected(selectedProject, selectedBranchId) ? "true" : "false"));

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

    const auto projectName = selectedProject ? selectedProject->name : juce::String();
    const auto restoredProjectBase = stemhub::projectfiles::resolveRestoreProjectName(versionHistory, versionId, projectName);
    const auto fileName = restoredProjectBase + "-" + versionId.substring(0, juce::jmin(8, versionId.length())) + ".zip";
    const auto restoreFile = projectFolder.getChildFile(fileName);
    juce::Logger::writeToLog("[Restore] Processor -> resolved restore file path=" + restoreFile.getFullPathName()
                             + " (base=" + restoredProjectBase + ")");

    setOperationState(OperationState::pulling);
    setActiveProjectStatusMessage("Restoring selected version...");
    sendChangeMessage();

    const auto requestedVersionId = versionId;
    const auto requestedRestoreFile = restoreFile;
    enqueueBackgroundTask([this,
                           requestedVersionId,
                           requestedRestoreFile]() -> BackgroundJobPayload
    {
        return performRestoreVersionRequest(requestedVersionId, requestedRestoreFile);
    });
}

void StemhubAudioProcessor::setSelectedVersionId(juce::String versionId)
{
    selectedVersionId = std::move(versionId);
    sendChangeMessage();
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
