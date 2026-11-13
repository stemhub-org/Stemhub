#include <map>
#include <vector>

#include "application/VersionFiles.hpp"
#include "application/VersionTransfer.hpp"
#include "domain/Manifest.hpp"

namespace
{
namespace manifest = stemhub::manifest;
using stemhub::versiontransfer::ReportProgress;

ApiError withContext(ApiError error, const juce::String& context)
{
    // "Cancelled." says it all, and ApiClient falls back to the same words when the server gave
    // no detail: "Failed to create the version." needs no second "Failed to create the version".
    if (error.kind == ApiError::Kind::cancelled || error.message == context + ".")
        return error;

    error.message = context + (error.message.isNotEmpty() ? ": " + error.message : juce::String("."));
    return error;
}

ApiError localFileError(const juce::String& message)
{
    return { ApiError::Kind::localFile, message };
}

void reportIfWanted(const ReportProgress& report, const juce::String& text)
{
    if (report != nullptr)
        report(text);
}

juce::String countOf(size_t index, size_t total)
{
    return juce::String(static_cast<int>(index) + 1) + " of " + juce::String(static_cast<int>(total));
}

juce::String toManifestPath(const juce::File& file, const juce::File& root)
{
    return file.getRelativePathFrom(root).replaceCharacter('\\', '/');
}

// What an upload sends: the manifest, and the file on this machine behind each distinct hash.
struct PreparedUpload
{
    manifest::Manifest manifest;
    std::map<juce::String, juce::File> fileByHash;
    std::vector<juce::String> distinctHashes;
};

// Collects and hashes the project's files. Names and the file count are checked first: hashing a
// large project folder takes time.
ApiResult<PreparedUpload> prepareUpload(const juce::File& projectFile, const ReportProgress& report)
{
    using Result = ApiResult<PreparedUpload>;

    const auto files = stemhub::versionfiles::collect(projectFile);
    if (isJobCancelled())
        return Result::failure(ApiError::cancelled());
    if (files.empty())
        return Result::failure(localFileError(projectFile.getFileName() + " no longer exists."));

    const auto root = projectFile.getParentDirectory();
    for (const auto& file : files)
    {
        // Paths keep their folders: basenames alone made files from different folders collide.
        const auto path = toManifestPath(file, root);
        if (!manifest::isSafePath(path))
            return Result::failure({ ApiError::Kind::invalidRequest, "Can't save \"" + path + "\": rename this file and try again." });
    }

    const auto assetCount = files.size() - 1;
    if (assetCount > manifest::kMaxAssets)
        return Result::failure({ ApiError::Kind::invalidRequest,
                                 "This project folder has " + juce::String(static_cast<int>(assetCount))
                                     + " audio & MIDI files; a version can hold " + juce::String(static_cast<int>(manifest::kMaxAssets))
                                     + ". Move the ones the project file doesn't use out of its folder." });

    PreparedUpload upload;
    upload.manifest.sourceDaw = stemhub::versionfiles::dawNameFor(projectFile);

    for (size_t index = 0; index < files.size(); ++index)
    {
        const auto& file = files[index];
        const auto sha = stemhub::versionfiles::sha256OfFile(file);
        if (isJobCancelled())
            return Result::failure(ApiError::cancelled());
        if (sha.isEmpty())
            return Result::failure(localFileError("Couldn't read " + file.getFullPathName() + "."));

        reportIfWanted(report, "Preparing " + countOf(index, files.size()) + " files...");

        // collect() lists the project file first.
        const manifest::FileRef ref { sha, file.getSize(), toManifestPath(file, root) };
        if (index == 0)
            upload.manifest.projectFile = ref;
        else
            upload.manifest.assets.push_back(ref);

        // Identical files share one blob, so each distinct content is offered and uploaded once.
        if (upload.fileByHash.emplace(sha, file).second)
            upload.distinctHashes.push_back(sha);
    }

    return Result::success(std::move(upload));
}
}

namespace stemhub::versiontransfer
{
ApiResult<VersionSummary> uploadVersion(const SignedInApi& api, const UploadRequest& request, const ReportProgress& report)
{
    using Result = ApiResult<VersionSummary>;
    jassert(request.projectId.isNotEmpty() && request.branchId.isNotEmpty());

    const auto prepared = prepareUpload(request.projectFile, report);
    if (!prepared.ok())
        return Result::failure(*prepared.error);

    const auto missing = api.checkMissingBlobs(request.projectId, prepared.value->distinctHashes);
    if (!missing.ok())
        return Result::failure(withContext(*missing.error, "Failed to check which files StemHub already has"));

    const auto& missingHashes = *missing.value;
    for (size_t index = 0; index < missingHashes.size(); ++index)
    {
        if (isJobCancelled())
            return Result::failure(ApiError::cancelled());

        const auto file = prepared.value->fileByHash.find(missingHashes[index]);
        if (file == prepared.value->fileByHash.end())
            continue;

        reportIfWanted(report, "Uploading " + countOf(index, missingHashes.size()) + " new files...");
        const auto uploaded = api.uploadBlob(request.projectId, missingHashes[index], file->second);
        if (!uploaded.ok())
            return Result::failure(withContext(*uploaded.error, "Failed to upload " + file->second.getFileName()));
    }

    if (isJobCancelled())
        return Result::failure(ApiError::cancelled());

    reportIfWanted(report, "Creating the version...");

    CreateVersionRequest createRequest;
    createRequest.message = request.message;
    createRequest.parentVersionId = request.parentVersionId;
    createRequest.manifest = manifest::toJson(prepared.value->manifest);

    auto created = api.createVersionFromManifest(request.branchId, createRequest);
    if (!created.ok())
        return Result::failure(withContext(*created.error, "Failed to create the version"));

    return created;
}

ApiResult<juce::File> restoreVersion(const SignedInApi& api, const RestoreRequest& request, const ReportProgress& report)
{
    using Result = ApiResult<juce::File>;
    jassert(request.projectId.isNotEmpty() && request.versionId.isNotEmpty());

    const auto& folder = request.destinationFolder;
    if (folder.exists())
        return Result::failure(localFileError("The restore folder already exists: " + folder.getFullPathName()));

    const auto manifestJson = api.fetchVersionManifest(request.versionId);
    if (!manifestJson.ok())
        return Result::failure(withContext(*manifestJson.error, "Failed to load the version"));

    manifest::Manifest version;
    if (const auto status = manifest::fromJson(*manifestJson.value, version); status.failed())
        return Result::failure({ ApiError::Kind::invalidResponse, status.getErrorMessage() });

    const auto partialFolder = folder.getSiblingFile("." + folder.getFileName() + ".partial-" + juce::Uuid().toString().substring(0, 8));
    if (!partialFolder.createDirectory())
        return Result::failure(localFileError("Could not create a folder in " + folder.getParentDirectory().getFullPathName()));

    // Only ever deletes the hidden folder this restore created.
    const auto fail = [&partialFolder](ApiError error)
    {
        partialFolder.deleteRecursively();
        return Result::failure(std::move(error));
    };

    std::vector<const manifest::FileRef*> files { &version.projectFile };
    for (const auto& asset : version.assets)
        files.push_back(&asset);

    for (size_t index = 0; index < files.size(); ++index)
    {
        if (isJobCancelled())
            return fail(ApiError::cancelled());

        const auto& file = *files[index];
        reportIfWanted(report, "Downloading " + countOf(index, files.size()) + " files...");

        // Paths were checked with the manifest; this re-check is what guarantees nothing is ever
        // written outside the folder.
        const auto destination = partialFolder.getChildFile(file.path);
        if (!destination.isAChildOf(partialFolder) || !destination.getParentDirectory().createDirectory())
            return fail(localFileError("Could not create " + file.path + " inside the restore folder."));

        const auto download = api.downloadBlob(request.projectId, file.sha256, destination);
        if (!download.ok())
            return fail(withContext(*download.error, "Failed to download " + file.path));

        const auto sha = stemhub::versionfiles::sha256OfFile(destination);
        if (isJobCancelled())
            return fail(ApiError::cancelled());

        if (sha != file.sha256)
            return fail(destination.getSize() < file.sizeBytes
                            ? ApiError { ApiError::Kind::network, "The download of " + file.path + " stopped early. Try again." }
                            : ApiError { ApiError::Kind::invalidResponse, "The downloaded " + file.path + " doesn't match its checksum." });
    }

    // Marks the folder as a restored copy, which a save from a project folder around it leaves out.
    if (!partialFolder.getChildFile(stemhub::versionfiles::kRestoredCopyMarker).replaceWithText(request.versionId))
        return fail(localFileError("Could not write in " + partialFolder.getFullPathName()));

    if (!partialFolder.moveFileTo(folder))
        return fail(localFileError("Could not move the restored files to " + folder.getFullPathName()));

    return Result::success(folder.getChildFile(version.projectFile.path));
}
}
