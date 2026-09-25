#include <set>

#include "application/VersionControlService.hpp"

namespace
{
juce::String buildApiErrorMessage(const std::optional<ApiError>& error,
                                  const juce::String& fallback)
{
    return error ? error->message : fallback;
}

juce::int64 sizeOf(const juce::var& blobRef)
{
    return juce::jmax<juce::int64>(0, static_cast<juce::int64>(blobRef.getProperty("size_bytes", 0)));
}

juce::int64 sumManifestSizes(const juce::var& manifest)
{
    if (!manifest.isObject())
        return 0;

    auto total = sizeOf(manifest.getProperty("project_file", {}));
    if (const auto* tracks = manifest.getProperty("tracks", {}).getArray())
        for (const auto& track : *tracks)
            total += sizeOf(track);

    return total;
}

ApiResult<VersionSummary> parseVersionSummary(const juce::var& value)
{
    auto* object = value.getDynamicObject();
    if (object == nullptr)
        return { {}, ApiError { 200, "Version response is not a JSON object." } };

    VersionSummary summary;
    summary.id = object->getProperty("id").toString();
    summary.branchId = object->getProperty("branch_id").toString();
    summary.parentVersionId = object->getProperty("parent_version_id").toString();
    summary.createdAt = object->getProperty("created_at").toString();
    summary.commitMessage = object->getProperty("commit_message").toString();
    summary.sourceDaw = object->getProperty("source_daw").toString();
    summary.sourceProjectFilename = object->getProperty("source_project_filename").toString();
    summary.totalSizeBytes = sumManifestSizes(object->getProperty("manifest_json"));

    if (!summary.isValid())
        return { {}, ApiError { 200, "Version response is missing required fields." } };

    return { summary, {} };
}

}

const IProjectApi* VersionControlService::getApiClient() const noexcept
{
    return apiClient;
}

juce::Result VersionControlService::pushVersion(const PushVersionRequest& request)
{
    if (accessToken.isEmpty())
        return juce::Result::fail("No access token is configured for version control.");

    const auto* api = getApiClient();
    if (api == nullptr)
        return juce::Result::fail("VersionControlService API client is not configured.");

    const auto projectId = request.projectId.isNotEmpty() ? request.projectId : context.projectId;
    const auto branchId  = request.branchId.isNotEmpty()  ? request.branchId  : context.branchId;
    if (projectId.isEmpty())
        return juce::Result::fail("A project ID is required to push a version.");
    if (branchId.isEmpty())
        return juce::Result::fail("A branch ID is required to push a version.");
    if (request.manifest.entries.empty())
        return juce::Result::fail("Manifest has no files to upload.");

    // Ask the server which blobs it already has.
    std::vector<juce::String> allShas;
    allShas.reserve(request.manifest.entries.size());
    for (const auto& e : request.manifest.entries)
        allShas.push_back(e.sha256);

    const auto missingResult = api->checkMissingBlobs(projectId, allShas, accessToken);
    if (!missingResult.ok())
        return juce::Result::fail(buildApiErrorMessage(missingResult.error, "Failed to check missing blobs."));

    // Upload only the blobs the server doesn't have.
    std::set<juce::String> missingSet(missingResult.value->begin(), missingResult.value->end());
    for (const auto& e : request.manifest.entries)
    {
        if (missingSet.find(e.sha256) == missingSet.end())
            continue;

        const auto up = api->uploadBlob(projectId, e.sha256, e.file, accessToken);
        if (!up.ok())
            return juce::Result::fail(buildApiErrorMessage(up.error,
                "Failed to upload blob " + e.filename + " (" + e.sha256.substring(0, 12) + "…)."));
    }

    // Create the version referencing the manifest.
    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    if (request.commitMessage.isNotEmpty())
        payload->setProperty("commit_message", request.commitMessage);
    const auto parentId = request.parentVersionId.isNotEmpty()
        ? request.parentVersionId
        : context.lastVersionId;
    if (parentId.isNotEmpty())
        payload->setProperty("parent_version_id", parentId);
    payload->setProperty("manifest", request.manifest.manifestJson);

    const auto createResult = api->createVersionFromManifest(branchId, juce::var(payload.get()), accessToken);
    if (!createResult.ok())
        return juce::Result::fail(buildApiErrorMessage(createResult.error, "Failed to create version from manifest."));

    const auto createdVersion = parseVersionSummary(*createResult.value);
    if (!createdVersion.ok())
        return juce::Result::fail(buildApiErrorMessage(createdVersion.error, "Failed to parse created version."));

    context.projectId = projectId;
    context.branchId = createdVersion.value->branchId;
    context.lastVersionId = createdVersion.value->id;
    return juce::Result::ok();
}

ApiResult<std::vector<VersionSummary>> VersionControlService::fetchVersionHistory(
    const juce::String& branchId,
    const juce::String& bearerToken) const
{
    if (branchId.isEmpty())
        return { {}, ApiError { 0, "A branch ID is required to fetch version history." } };

    if (bearerToken.isEmpty())
        return { {}, ApiError { 0, "An access token is required to fetch version history." } };

    const auto* api = getApiClient();
    if (api == nullptr)
        return { {}, ApiError { 0, "VersionControlService API client is not configured." } };

    const auto jsonResult = api->requestJson("/branches/" + branchId + "/versions/", "GET", {}, bearerToken);
    if (!jsonResult.ok())
        return { {}, jsonResult.error };

    if (!jsonResult.value->isArray())
        return { {}, ApiError { 200, "Version history response is not a JSON array." } };

    std::vector<VersionSummary> versions;
    const auto* array = jsonResult.value->getArray();
    versions.reserve(static_cast<size_t>(array->size()));

    for (const auto& item : *array)
    {
        const auto summary = parseVersionSummary(item);
        if (!summary.ok())
            return { {}, summary.error };

        versions.push_back(*summary.value);
    }

    return { versions, {} };
}

namespace
{
juce::String sha256HexOfLocalFile(const juce::File& file)
{
    juce::FileInputStream stream(file);
    if (!stream.openedOk())
        return {};
    return juce::SHA256(stream).toHexString();
}
}

juce::Result VersionControlService::restoreVersionFromManifest(const juce::String& projectId,
                                                                 const juce::String& versionId,
                                                                 const juce::File& restoreDirectory,
                                                                 juce::File& outProjectFile)
{
    outProjectFile = juce::File();

    if (accessToken.isEmpty())
        return juce::Result::fail("No access token is configured for version control.");
    if (versionId.isEmpty())
        return juce::Result::fail("A version ID is required to restore a snapshot.");
    if (projectId.isEmpty())
        return juce::Result::fail("A project ID is required to restore a snapshot.");

    const auto* api = getApiClient();
    if (api == nullptr)
        return juce::Result::fail("VersionControlService API client is not configured.");

    // Fetch the version detail — we want manifest_json (added to
    // VersionResponse in the CAS integration PR).
    const auto detailResult = api->requestJson("/versions/" + versionId, "GET", {}, accessToken);
    if (!detailResult.ok())
        return juce::Result::fail(buildApiErrorMessage(detailResult.error, "Failed to fetch version detail."));

    auto* detailObj = detailResult.value->getDynamicObject();
    if (detailObj == nullptr)
        return juce::Result::fail("Version detail is not a JSON object.");

    const auto manifestJson = detailObj->getProperty("manifest_json");
    if (manifestJson.isVoid() || !manifestJson.isObject())
        return juce::Result::fail("Version has no content-addressed manifest.");

    ParsedManifest parsed;
    const auto parseStatus = SnapshotBundler::parseManifest(manifestJson, parsed);
    if (parseStatus.failed())
        return parseStatus;

    if (!restoreDirectory.exists() && !restoreDirectory.createDirectory())
        return juce::Result::fail("Failed to create restore directory: " + restoreDirectory.getFullPathName());
    if (!restoreDirectory.isDirectory())
        return juce::Result::fail("Restore path is not a directory: " + restoreDirectory.getFullPathName());

    std::vector<juce::File> writtenFiles;
    writtenFiles.reserve(parsed.entries.size());

    for (const auto& entry : parsed.entries)
    {
        // Paths were validated when the manifest was parsed; this re-check is what guarantees
        // nothing is ever written outside the restore folder.
        auto dest = restoreDirectory.getChildFile(entry.filename);
        if (!dest.isAChildOf(restoreDirectory) || !dest.getParentDirectory().createDirectory())
        {
            for (auto& f : writtenFiles) f.deleteFile();
            return juce::Result::fail("Could not create " + entry.filename + " inside the restore folder.");
        }

        if (dest.existsAsFile())
            dest.deleteFile();

        const auto downloadStatus = api->downloadBlob(projectId, entry.sha256, dest, accessToken);
        if (downloadStatus.failed())
        {
            for (auto& f : writtenFiles) f.deleteFile();
            return juce::Result::fail("Failed to download blob " + entry.filename
                                       + " (" + entry.sha256.substring(0, 12) + "..): "
                                       + downloadStatus.getErrorMessage());
        }

        const auto localSha = sha256HexOfLocalFile(dest);
        if (localSha != entry.sha256)
        {
            for (auto& f : writtenFiles) f.deleteFile();
            dest.deleteFile();
            return juce::Result::fail("SHA-256 mismatch for " + entry.filename
                                       + ": expected " + entry.sha256.substring(0, 12)
                                       + ", got " + localSha.substring(0, 12));
        }

        writtenFiles.push_back(dest);
        if (entry.isProjectFile)
            outProjectFile = dest;
    }

    if (!outProjectFile.existsAsFile())
        return juce::Result::fail("Manifest did not include a project file entry.");

    context.projectId = projectId;
    context.lastVersionId = versionId;
    return juce::Result::ok();
}
