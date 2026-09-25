#include "application/PluginProcessor.hpp"
#include "application/PluginProcessorHelpers.hpp"
#include "application/SnapshotBundler.hpp"

using namespace stemhub::processorhelpers;

StemhubAudioProcessor::PushVersionJobResult StemhubAudioProcessor::performPushVersionRequest(
    const juce::File& projectFile,
    const juce::File& projectRootDirectory,
    const std::optional<Project>& project,
    const juce::String& branchId,
    const juce::String& parentVersionId,
    const juce::String& commitMessage,
    const juce::String& dawName,
    const juce::String& accessToken) const
{
    PushVersionJobResult result;

    if (!hasProjectAndBranchSelected(project, branchId))
    {
        result.errorMessage = "Choose or create a project before saving.";
        return result;
    }
    if (!projectFile.existsAsFile())
    {
        result.errorMessage = "Choose a project file before saving.";
        return result;
    }
    if (!projectRootDirectory.isDirectory())
    {
        result.errorMessage = "Choose a valid project file before saving.";
        return result;
    }

    // Taken before hashing: if the DAW saves again meanwhile, the next save sees a change.
    result.pushedProjectFile = projectFile;
    result.pushedFileSizeBytes = projectFile.getSize();
    result.pushedFileModTimeMs = projectFile.getLastModificationTime().toMilliseconds();

    SnapshotBundleRequest bundleRequest;
    bundleRequest.sourceProjectFile = projectFile;
    bundleRequest.sourceDaw = dawName;
    bundleRequest.projectRootDirectory = projectRootDirectory;

    SnapshotBundler bundler;
    ContentAddressedManifest manifest;
    const auto manifestStatus = bundler.buildManifest(bundleRequest, manifest);
    if (manifestStatus.failed())
    {
        result.errorMessage = manifestStatus.getErrorMessage();
        return result;
    }

    PushVersionRequest pushRequest;
    pushRequest.projectId = project->id;
    pushRequest.branchId = branchId;
    pushRequest.commitMessage = commitMessage;
    pushRequest.parentVersionId = parentVersionId;
    pushRequest.manifest = std::move(manifest);

    // A service of its own: the shared one belongs to the message thread.
    VersionControlService pushService(*apiClient);
    pushService.setAccessToken(accessToken);

    const auto pushStatus = pushService.pushVersion(pushRequest);
    if (pushStatus.failed())
    {
        result.errorMessage = pushStatus.getErrorMessage();
        return result;
    }

    result.pushedVersionId = pushService.getLastVersionId();

    auto versionsResult = pushService.fetchVersionHistory(branchId, accessToken);
    if (versionsResult.ok() && versionsResult.value.has_value())
    {
        sortVersionHistoryNewestFirst(*versionsResult.value);
        result.refreshedVersions = std::move(*versionsResult.value);
        result.activeProjectStatusMessage = "Version saved successfully.";
    }
    else
    {
        result.activeProjectStatusMessage = "Version saved. Sync to see it in the history ("
            + (versionsResult.error ? versionsResult.error->message : juce::String("history unavailable")) + ").";
    }

    return result;
}

StemhubAudioProcessor::RestoreVersionJobResult StemhubAudioProcessor::performRestoreVersionRequest(
    const juce::String& projectId,
    const juce::String& versionId,
    const juce::File& destinationFolder,
    const juce::String& accessToken) const
{
    RestoreVersionJobResult result;
    result.restoredVersionId = versionId;

    if (versionId.isEmpty())
    {
        result.errorMessage = "Select a version before restoring.";
        return result;
    }
    if (projectId.isEmpty())
    {
        result.errorMessage = "Choose a project before restoring.";
        return result;
    }
    // The request picked a folder that did not exist. Anything there now is someone else's.
    if (destinationFolder.exists())
    {
        result.errorMessage = "The restore folder already exists: " + destinationFolder.getFullPathName();
        return result;
    }

    VersionControlService restoreService(*apiClient);
    restoreService.setAccessToken(accessToken);

    juce::File restoredProjectFile;
    const auto status = restoreService.restoreVersionFromManifest(
        projectId, versionId, destinationFolder, restoredProjectFile);
    if (status.failed())
    {
        result.errorMessage = status.getErrorMessage();
        return result;
    }

    result.restoredProjectFile = restoredProjectFile;
    result.activeProjectStatusMessage = "Version restored successfully: " + restoredProjectFile.getFileName();
    return result;
}
