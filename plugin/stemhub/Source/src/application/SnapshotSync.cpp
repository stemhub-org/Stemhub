#include <map>

#include "application/SnapshotSync.hpp"

namespace
{
ApiError withContext(ApiError error, const juce::String& context)
{
    // "Cancelled." says it all.
    if (error.kind != ApiError::Kind::cancelled)
        error.message = context + (error.message.isNotEmpty() ? ": " + error.message : juce::String("."));

    return error;
}

void reportIfWanted(const stemhub::snapshots::ReportProgress& report, const juce::String& text)
{
    if (report != nullptr)
        report(text);
}

juce::String countOf(size_t index, size_t total)
{
    return juce::String(static_cast<int>(index) + 1) + " of " + juce::String(static_cast<int>(total));
}
}

namespace stemhub::snapshots
{
ApiResult<VersionSummary> pushSnapshot(const IProjectApi& api,
                                       const juce::String& token,
                                       const PushRequest& request,
                                       const ReportProgress& report)
{
    using Result = ApiResult<VersionSummary>;

    if (request.projectId.isEmpty() || request.branchId.isEmpty())
        return Result::failure({ ApiError::Kind::invalidRequest, 0, "Choose or create a project before saving." });
    if (request.manifest.entries.empty())
        return Result::failure({ ApiError::Kind::invalidRequest, 0, "There are no files to save." });

    // Identical files share one blob, so each distinct content is offered and uploaded once.
    std::vector<juce::String> distinctHashes;
    std::map<juce::String, juce::File> fileByHash;
    for (const auto& entry : request.manifest.entries)
        if (fileByHash.emplace(entry.sha256, entry.file).second)
            distinctHashes.push_back(entry.sha256);

    const auto missing = api.checkMissingBlobs(request.projectId, distinctHashes, token);
    if (!missing.ok())
        return Result::failure(withContext(*missing.error, "Failed to check which files StemHub already has"));

    const auto& missingHashes = *missing.value;
    for (size_t index = 0; index < missingHashes.size(); ++index)
    {
        if (isJobCancelled())
            return Result::failure(ApiError::cancelled());

        const auto file = fileByHash.find(missingHashes[index]);
        if (file == fileByHash.end())
            continue;

        reportIfWanted(report, "Uploading " + countOf(index, missingHashes.size()) + " new files...");
        const auto upload = api.uploadBlob(request.projectId, missingHashes[index], file->second, token);
        if (!upload.ok())
            return Result::failure(withContext(*upload.error, "Failed to upload " + file->second.getFileName()));
    }

    if (isJobCancelled())
        return Result::failure(ApiError::cancelled());

    reportIfWanted(report, "Creating the version...");

    CreateVersionRequest createRequest;
    createRequest.commitMessage = request.commitMessage;
    createRequest.parentVersionId = request.parentVersionId;
    createRequest.manifest = request.manifest.manifestJson;

    auto created = api.createVersionFromManifest(request.branchId, createRequest, token);
    if (!created.ok())
        return Result::failure(withContext(*created.error, "Failed to create the version"));

    return created;
}

ApiResult<juce::File> restoreSnapshot(const IProjectApi& api,
                                      const juce::String& token,
                                      const RestoreRequest& request,
                                      const ReportProgress& report)
{
    using Result = ApiResult<juce::File>;

    if (request.projectId.isEmpty() || request.versionId.isEmpty())
        return Result::failure({ ApiError::Kind::invalidRequest, 0, "Select a version before restoring." });

    const auto& folder = request.destinationFolder;
    if (folder.exists())
        return Result::failure({ ApiError::Kind::localFile, 0, "The restore folder already exists: " + folder.getFullPathName() });

    const auto manifest = api.fetchVersionManifest(request.versionId, token);
    if (!manifest.ok())
        return Result::failure(manifest.error->kind == ApiError::Kind::notFound
                                   ? ApiError { ApiError::Kind::notFound, 404, "This version has no file list, so it can't be restored." }
                                   : withContext(*manifest.error, "Failed to load the version"));

    ParsedManifest parsed;
    if (const auto status = SnapshotBundler::parseManifest(*manifest.value, parsed); status.failed())
        return Result::failure({ ApiError::Kind::invalidResponse, 0, status.getErrorMessage() });

    if (!folder.createDirectory())
        return Result::failure({ ApiError::Kind::localFile, 0, "Could not create the restore folder " + folder.getFullPathName() });

    // Only ever deletes this folder: it didn't exist before this restore created it.
    const auto fail = [&folder](ApiError error)
    {
        folder.deleteRecursively();
        return Result::failure(std::move(error));
    };

    juce::File projectFile;
    for (size_t index = 0; index < parsed.entries.size(); ++index)
    {
        if (isJobCancelled())
            return fail(ApiError::cancelled());

        const auto& entry = parsed.entries[index];
        reportIfWanted(report, "Downloading " + countOf(index, parsed.entries.size()) + " files...");

        // Paths were validated when the manifest was parsed; this re-check is what guarantees
        // nothing is ever written outside the restore folder.
        const auto destination = folder.getChildFile(entry.filename);
        if (!destination.isAChildOf(folder) || !destination.getParentDirectory().createDirectory())
            return fail({ ApiError::Kind::localFile, 0, "Could not create " + entry.filename + " inside the restore folder." });

        const auto download = api.downloadBlob(request.projectId, entry.sha256, destination, token);
        if (!download.ok())
            return fail(withContext(*download.error, "Failed to download " + entry.filename));

        if (SnapshotBundler::sha256OfFile(destination) != entry.sha256)
            return fail({ ApiError::Kind::invalidResponse, 0, "The downloaded " + entry.filename + " doesn't match its checksum." });

        if (entry.isProjectFile)
            projectFile = destination;
    }

    if (!projectFile.existsAsFile())
        return fail({ ApiError::Kind::invalidResponse, 0, "This version has no project file." });

    return Result::success(projectFile);
}
}
