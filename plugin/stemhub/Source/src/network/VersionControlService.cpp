#include "application/VersionControlService.hpp"

namespace
{
juce::String buildApiErrorMessage(const std::optional<ApiError>& error,
                                  const juce::String& fallback)
{
    return error ? error->message : fallback;
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
    summary.artifactPath = object->getProperty("artifact_path").toString();
    summary.artifactChecksum = object->getProperty("artifact_checksum").toString();
    summary.artifactSizeBytes = static_cast<int64>(object->getProperty("artifact_size_bytes"));
    summary.hasArtifact = summary.artifactPath.isNotEmpty();

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

    const auto branchId = request.branchId.isNotEmpty() ? request.branchId : context.branchId;
    if (branchId.isEmpty())
        return juce::Result::fail("A branch ID is required to push a version.");

    if (!request.localProjectFile.existsAsFile())
        return juce::Result::fail("Local project file does not exist.");

    juce::DynamicObject::Ptr bodyObject = new juce::DynamicObject();

    if (request.commitMessage.isNotEmpty())
        bodyObject->setProperty("commit_message", request.commitMessage);

    const auto parentVersionId = resolveParentVersionId(request);
    if (parentVersionId.isNotEmpty())
        bodyObject->setProperty("parent_version_id", parentVersionId);

    if (request.dawName.isNotEmpty())
        bodyObject->setProperty("source_daw", request.dawName);

    const auto sourceProjectFilename = request.sourceProjectFilename.isNotEmpty()
        ? request.sourceProjectFilename
        : request.localProjectFile.getFileName();
    bodyObject->setProperty("source_project_filename", sourceProjectFilename);

    if (!request.snapshotManifest.isVoid())
        bodyObject->setProperty("snapshot_manifest", request.snapshotManifest);

    const auto body = juce::JSON::toString(juce::var(bodyObject.get()));
    const auto createResult = api->requestJson("/branches/" + branchId + "/versions/", "POST", body, accessToken);
    if (!createResult.ok())
        return juce::Result::fail(buildApiErrorMessage(createResult.error, "Failed to create version."));

    const auto createdVersion = parseVersionSummary(*createResult.value);
    if (!createdVersion.ok())
        return juce::Result::fail(buildApiErrorMessage(createdVersion.error, "Failed to parse created version."));

    const auto uploadResult = api->uploadFile("/versions/" + createdVersion.value->id + "/artifact",
                                                   request.localProjectFile,
                                                   "artifact",
                                                   accessToken);
    if (!uploadResult.ok())
        return juce::Result::fail(buildApiErrorMessage(uploadResult.error, "Failed to upload snapshot artifact."));

    const auto uploadedVersion = parseVersionSummary(*uploadResult.value);
    if (!uploadedVersion.ok())
        return juce::Result::fail(buildApiErrorMessage(uploadedVersion.error, "Failed to parse uploaded version."));

    context.branchId = uploadedVersion.value->branchId;
    context.lastVersionId = uploadedVersion.value->id;
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

ApiResult<VersionSummary> VersionControlService::fetchVersion(
    const juce::String& versionId,
    const juce::String& bearerToken) const
{
    if (versionId.isEmpty())
        return { {}, ApiError { 0, "A version ID is required to fetch version details." } };

    if (bearerToken.isEmpty())
        return { {}, ApiError { 0, "An access token is required to fetch version details." } };

    const auto* api = getApiClient();
    if (api == nullptr)
        return { {}, ApiError { 0, "VersionControlService API client is not configured." } };

    const auto jsonResult = api->requestJson("/versions/" + versionId, "GET", {}, bearerToken);
    if (!jsonResult.ok())
        return { {}, jsonResult.error };

    return parseVersionSummary(*jsonResult.value);
}

juce::Result VersionControlService::downloadVersion(
    const juce::String& versionId,
    const juce::File& destinationFile,
    const juce::String& bearerToken) const
{
    if (versionId.isEmpty())
        return juce::Result::fail("A version ID is required to download a snapshot.");

    if (bearerToken.isEmpty())
        return juce::Result::fail("An access token is required to download a snapshot.");

    if (destinationFile.isDirectory())
        return juce::Result::fail("Destination path must be a file, not a directory.");

    const auto* api = getApiClient();
    if (api == nullptr)
        return juce::Result::fail("VersionControlService API client is not configured.");

    return api->downloadFile("/versions/" + versionId + "/artifact", destinationFile, bearerToken);
}

juce::Result VersionControlService::restoreVersion(
    const juce::String& versionId,
    const juce::File& destinationFile,
    const juce::String& bearerToken) const
{
    return downloadVersion(versionId, destinationFile, bearerToken);
}

juce::String VersionControlService::resolveParentVersionId(const PushVersionRequest& request) const
{
    if (request.parentVersionId.isNotEmpty())
        return request.parentVersionId;

    return context.lastVersionId;
}
