#include <algorithm>

#include "application/PluginProcessor.hpp"
#include "application/PluginProcessorHelpers.hpp"
#include "application/ProjectFileService.hpp"
#include "application/SnapshotBundler.hpp"

using namespace stemhub::processorhelpers;

namespace
{
juce::File findPreviewWavSource(const juce::File& projectRootDirectory)
{
    if (!projectRootDirectory.isDirectory())
        return {};

    juce::Array<juce::File> wavFiles;
    projectRootDirectory.findChildFiles(wavFiles, juce::File::findFiles, true, "*.wav");

    if (wavFiles.isEmpty())
        return {};

    std::sort(wavFiles.begin(), wavFiles.end(), [](const juce::File& lhs, const juce::File& rhs)
    {
        return lhs.getFullPathName() < rhs.getFullPathName();
    });

    return wavFiles[0];
}

juce::File copyPreviewTrackToTemp(const juce::File& sourceFile)
{
    if (!sourceFile.existsAsFile())
        return {};

    const auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("stemhub-previews");

    if ((!tempRoot.exists() && !tempRoot.createDirectory()) || !tempRoot.isDirectory())
        return {};

    const auto previewFile = tempRoot.getChildFile("preview_" + juce::Uuid().toString() + ".wav");
    if (previewFile.existsAsFile())
        previewFile.deleteFile();

    if (!sourceFile.copyFileTo(previewFile))
        return {};

    return previewFile;
}
}

StemhubAudioProcessor::PushVersionJobResult StemhubAudioProcessor::performPushVersionRequest(
    const juce::File& projectFile,
    const juce::File& projectRootDirectory,
    const std::optional<Project>& project,
    const juce::String& branchId,
    const juce::String& commitMessage,
    const juce::String& dawName)
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

    ProjectVersionContext context;
    context.projectId = project->id;
    context.branchId = branchId;
    context.lastVersionId = workingCopyVersionId.isNotEmpty() ? workingCopyVersionId
                                                             : versionControlService.getLastVersionId();
    versionControlService.setCurrentProjectContext(context);

    PushVersionRequest request;
    request.branchId = branchId;
    request.commitMessage = commitMessage;
    request.dawName = dawName;

    const auto sourcePreview = findPreviewWavSource(projectRootDirectory);
    juce::File previewTrackFile = copyPreviewTrackToTemp(sourcePreview);

    SnapshotBundleRequest bundleRequest;
    bundleRequest.sourceProjectFile = projectFile;
    bundleRequest.sourceDaw = dawName;
    bundleRequest.projectRootDirectory = projectRootDirectory;
    bundleRequest.previewTrackFile = previewTrackFile;

    SnapshotBundler bundle;
    SnapshotBundleResult bundleOutput;

    const auto bundleStatus = bundle.bundleProject(bundleRequest, bundleOutput);
    if (bundleStatus.failed())
    {
        result.errorMessage = bundleStatus.getErrorMessage();
        if (previewTrackFile.existsAsFile())
            previewTrackFile.deleteFile();
        return result;
    }

    request.localProjectFile = bundleOutput.bundleFile;
    request.sourceProjectFilename = projectFile.getFileName();
    request.snapshotManifest = bundleOutput.manifest;

    const auto pushResult = versionControlService.pushVersion(request);
    if (pushResult.failed())
    {
        result.errorMessage = pushResult.getErrorMessage();
        if (bundleOutput.bundleFile.existsAsFile())
            bundleOutput.bundleFile.deleteFile();
        if (previewTrackFile.existsAsFile())
            previewTrackFile.deleteFile();
        return result;
    }

    const auto pushedVersionId = versionControlService.getLastVersionId();

    if (bundleOutput.bundleFile.existsAsFile())
        bundleOutput.bundleFile.deleteFile();
    if (previewTrackFile.existsAsFile())
        previewTrackFile.deleteFile();

    result.pushedVersionId = pushedVersionId;
    result.activeProjectStatusMessage = "Version pushed successfully.";
    return result;
}

StemhubAudioProcessor::RestoreVersionJobResult StemhubAudioProcessor::performRestoreVersionRequest(
    const juce::String& versionId,
    const juce::File& destinationFile) const
{
    juce::Logger::writeToLog("[Restore] Job -> start versionId=" + versionId
                             + ", destinationFile=" + destinationFile.getFullPathName());

    RestoreVersionJobResult result;
    result.restoredVersionId = versionId;

    if (versionId.isEmpty())
    {
        juce::Logger::writeToLog("[Restore] Job -> reject: empty versionId");
        result.errorMessage = "Select a version before restoring.";
        return result;
    }

    if (destinationFile.isDirectory())
    {
        juce::Logger::writeToLog("[Restore] Job -> reject: destination is directory");
        result.errorMessage = "Select a valid destination file for restore.";
        return result;
    }

    const auto restoreResult = versionControlService.restoreVersion(
        versionId,
        destinationFile,
        versionControlService.getAccessToken());
    if (restoreResult.failed())
    {
        juce::Logger::writeToLog("[Restore] Job -> download failed: " + restoreResult.getErrorMessage());
        result.errorMessage = restoreResult.getErrorMessage();
        return result;
    }
    juce::Logger::writeToLog("[Restore] Job -> download ok, file exists="
                             + juce::String(destinationFile.existsAsFile() ? "true" : "false"));
    juce::Logger::writeToLog("[Restore] Job -> destination size=" + juce::String(destinationFile.getSize()));

    const auto restoreDirectory = destinationFile.getParentDirectory().getChildFile(
        destinationFile.getFileNameWithoutExtension());
    if (restoreDirectory.exists())
    {
        if (!restoreDirectory.deleteRecursively())
        {
            juce::Logger::writeToLog("[Restore] Job -> failed to clear restore directory " + restoreDirectory.getFullPathName());
            result.errorMessage = "Failed to clear previous restore folder at " + restoreDirectory.getFullPathName();
            return result;
        }
    }

    juce::File restoredProjectFile;
    const auto extractResult = stemhub::projectfiles::resolveRestoreResult(destinationFile, restoreDirectory, restoredProjectFile);
    if (extractResult.failed())
    {
        juce::Logger::writeToLog("[Restore] Job -> extraction failed: " + extractResult.getErrorMessage());
        result.errorMessage = extractResult.getErrorMessage();
        return result;
    }
    juce::Logger::writeToLog("[Restore] Job -> restored project path=" + restoredProjectFile.getFullPathName()
                             + ", exists=" + juce::String(restoredProjectFile.existsAsFile() ? "true" : "false"));

    result.restoredProjectFile = std::move(restoredProjectFile);
    result.activeProjectStatusMessage = "Version restored successfully: " + result.restoredProjectFile.getFileName();
    return result;
}
