#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <JuceHeader.h>

#include "network/ApiClient.hpp"
#include "support/BlockingGate.hpp"

namespace stemhub::test
{
inline juce::String sha256Of(const juce::MemoryBlock& data)
{
    return juce::SHA256(data.getData(), data.getSize()).toHexString();
}

template <typename T>
ApiResult<T> fail(int statusCode, const juce::String& message)
{
    return ApiResult<T>::failure(ApiError::fromStatus(statusCode, message));
}

// In-memory StemHub backend. Worker threads call it while the test thread inspects it,
// so all state is behind one mutex; gates are always waited on outside of it.
class FakeProjectApi final : public IProjectApi
{
public:
    struct CreatedVersion
    {
        juce::String id;
        juce::String branchId;
        juce::String parentVersionId;
        juce::String commitMessage;
    };

    ApiResult<LoginResponse> login(const juce::String& email, const juce::String& password) const override
    {
        juce::ignoreUnused(email, password);
        return ApiResult<LoginResponse>::success({ "token" });
    }

    ApiResult<User> fetchCurrentUser(const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);
        if (offline)
            return ApiResult<User>::failure({ ApiError::Kind::network, 0, "Can't reach StemHub." });
        if (rejectToken || !cachedSessionIsValid)
            return fail<User>(401, "Could not validate credentials");

        return ApiResult<User>::success({ "user-1", "user@example.com", "stemhub" });
    }

    ApiResult<std::vector<Project>> fetchProjects(const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);
        if (rejectToken)
            return fail<std::vector<Project>>(401, "Could not validate credentials");
        if (failProjectList)
            return fail<std::vector<Project>>(500, "Failed to load projects.");

        const std::lock_guard<std::mutex> lock(mutex);
        auto all = projects;
        all.insert(all.end(), createdProjects.begin(), createdProjects.end());
        return ApiResult<std::vector<Project>>::success(all);
    }

    // Created projects are named "created-1", "created-2"... and get a "main" branch, as on the
    // backend.
    ApiResult<Project> createProject(const juce::String& name, const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);

        const std::lock_guard<std::mutex> lock(mutex);
        Project project;
        project.id = "created-" + juce::String(static_cast<int>(createdProjects.size()) + 1);
        project.name = name;
        createdProjects.push_back(project);
        return ApiResult<Project>::success(project);
    }

    ApiResult<std::vector<Branch>> fetchBranches(const juce::String& projectId, const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);

        if (auto gate = findGate(branchFetchGates, projectId))
        {
            gate->block();
            if (branchFetchReturned != nullptr)
                *branchFetchReturned = true;
        }

        if (rejectToken)
            return fail<std::vector<Branch>>(401, "Could not validate credentials");

        const std::lock_guard<std::mutex> lock(mutex);
        if (auto it = branchErrors.find(projectId); it != branchErrors.end())
            return fail<std::vector<Branch>>(500, it->second);

        if (auto it = projectBranches.find(projectId); it != projectBranches.end())
            return ApiResult<std::vector<Branch>>::success(it->second);

        for (const auto& created : createdProjects)
            if (created.id == projectId)
                return ApiResult<std::vector<Branch>>::success({ Branch { projectId + "-main", projectId, "main" } });

        return ApiResult<std::vector<Branch>>::success({});
    }

    ApiResult<std::vector<VersionSummary>> fetchVersions(const juce::String& branchId,
                                                         const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);

        if (auto gate = findGate(versionFetchGates, branchId))
            gate->block();

        if (rejectToken)
            return fail<std::vector<VersionSummary>>(401, "Could not validate credentials");

        const std::lock_guard<std::mutex> lock(mutex);
        if (auto it = versionErrors.find(branchId); it != versionErrors.end())
            return fail<std::vector<VersionSummary>>(500, it->second);

        if (auto it = branchVersions.find(branchId); it != branchVersions.end())
            return ApiResult<std::vector<VersionSummary>>::success(it->second);

        return ApiResult<std::vector<VersionSummary>>::success({});
    }

    ApiResult<juce::var> fetchVersionManifest(const juce::String& versionId, const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);
        if (rejectToken)
            return fail<juce::var>(401, "Could not validate credentials");

        const std::lock_guard<std::mutex> lock(mutex);
        const auto manifest = manifestsByVersion.find(versionId);
        if (manifest == manifestsByVersion.end())
            return fail<juce::var>(404, "Version not found.");

        return ApiResult<juce::var>::success(juce::JSON::parse(manifest->second));
    }

    ApiResult<std::vector<juce::String>> checkMissingBlobs(const juce::String& projectId,
                                                            const std::vector<juce::String>& sha256s,
                                                            const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);

        if (auto gate = findGate(checkMissingGates, projectId))
            gate->block();

        if (rejectToken)
            return fail<std::vector<juce::String>>(401, "Could not validate credentials");

        const std::lock_guard<std::mutex> lock(mutex);
        std::vector<juce::String> missing;
        for (const auto& sha : sha256s)
            if (blobs.find(sha) == blobs.end())
                missing.push_back(sha);

        return ApiResult<std::vector<juce::String>>::success(missing);
    }

    ApiResult<Unit> uploadBlob(const juce::String& projectId,
                               const juce::String& sha256,
                               const juce::File& file,
                               const juce::String& accessToken) const override
    {
        juce::ignoreUnused(projectId, accessToken);

        juce::MemoryBlock data;
        if (!file.loadFileAsData(data))
            return ApiResult<Unit>::failure({ ApiError::Kind::localFile, 0, "Blob source file does not exist." });

        if (sha256Of(data) != sha256)
            return fail<Unit>(400, "SHA-256 mismatch.");

        const std::lock_guard<std::mutex> lock(mutex);
        blobs[sha256] = data;
        ++uploadsBySha[sha256];
        return ApiResult<Unit>::success({});
    }

    ApiResult<VersionSummary> createVersionFromManifest(const juce::String& branchId,
                                                        const CreateVersionRequest& request,
                                                        const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);

        const std::lock_guard<std::mutex> lock(mutex);
        if (createVersionError.isNotEmpty())
            return fail<VersionSummary>(500, createVersionError);

        const auto number = static_cast<int>(createdVersions.size()) + 1;

        VersionSummary version;
        version.id = juce::String::toHexString(number).paddedLeft('0', 8) + "-0000-4000-8000-000000000000";
        version.branchId = branchId;
        version.parentVersionId = request.parentVersionId;
        version.commitMessage = request.commitMessage;
        version.createdAt = "2026-03-19T10:00:" + juce::String(number).paddedLeft('0', 2) + "Z";
        version.sourceProjectFilename = request.manifest.getProperty("source_project_filename", {}).toString();

        createdVersions.push_back({ version.id, branchId, version.parentVersionId, version.commitMessage });
        manifestsByVersion[version.id] = juce::JSON::toString(request.manifest);
        branchVersions[branchId].push_back(version);
        return ApiResult<VersionSummary>::success(version);
    }

    ApiResult<Unit> downloadBlob(const juce::String& projectId,
                                 const juce::String& sha256,
                                 const juce::File& destinationFile,
                                 const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);

        if (auto gate = findGate(downloadGates, projectId))
            gate->block();

        juce::MemoryBlock data;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            const auto blob = blobs.find(sha256);
            if (blob == blobs.end())
                return fail<Unit>(404, "Blob not found");

            data = blob->second;
            if (corruptDownloads)
                data.append("!", 1);
        }

        if (!destinationFile.replaceWithData(data.getData(), data.getSize()))
            return ApiResult<Unit>::failure({ ApiError::Kind::localFile, 0, "Could not write " + destinationFile.getFullPathName() });

        return ApiResult<Unit>::success({});
    }

    // Stores a version made of (relative path, content) files, the first being the project
    // file, as if another collaborator had saved it. Returns its id.
    juce::String addVersion(const juce::String& branchId,
                            const juce::String& commitMessage,
                            const std::vector<std::pair<juce::String, juce::String>>& files)
    {
        juce::Array<juce::var> tracks;
        juce::var projectFileRef;

        for (size_t index = 0; index < files.size(); ++index)
        {
            const auto& [path, content] = files[index];
            const juce::MemoryBlock data(content.toRawUTF8(), content.getNumBytesAsUTF8());
            const auto sha = sha256Of(data);
            {
                const std::lock_guard<std::mutex> lock(mutex);
                blobs[sha] = data;
            }

            auto* ref = new juce::DynamicObject();
            ref->setProperty("sha256", sha);
            ref->setProperty("size_bytes", static_cast<juce::int64>(data.getSize()));
            ref->setProperty("filename", path);

            if (index == 0)
            {
                projectFileRef = juce::var(ref);
            }
            else
            {
                ref->setProperty("name", path);
                tracks.add(juce::var(ref));
            }
        }

        auto* manifest = new juce::DynamicObject();
        manifest->setProperty("manifest_version", 1);
        manifest->setProperty("source_project_filename", files.front().first.fromLastOccurrenceOf("/", false, false));
        manifest->setProperty("project_file", projectFileRef);
        manifest->setProperty("tracks", tracks);

        CreateVersionRequest request;
        request.commitMessage = commitMessage;
        request.manifest = juce::var(manifest);
        return createVersionFromManifest(branchId, request, "token").value->id;
    }

    std::vector<CreatedVersion> getCreatedVersions() const
    {
        const std::lock_guard<std::mutex> lock(mutex);
        return createdVersions;
    }

    int getUploadCount(const juce::String& sha) const
    {
        const std::lock_guard<std::mutex> lock(mutex);
        const auto count = uploadsBySha.find(sha);
        return count != uploadsBySha.end() ? count->second : 0;
    }

    void setCheckMissingGate(const juce::String& projectId, std::shared_ptr<BlockingGate> gate)
    {
        const std::lock_guard<std::mutex> lock(mutex);
        checkMissingGates[projectId] = std::move(gate);
    }

    void setDownloadGate(const juce::String& projectId, std::shared_ptr<BlockingGate> gate)
    {
        const std::lock_guard<std::mutex> lock(mutex);
        downloadGates[projectId] = std::move(gate);
    }

    // Configuration: written by the test thread while no job is running.
    bool cachedSessionIsValid { true };
    std::vector<Project> projects;
    std::map<juce::String, std::vector<Branch>> projectBranches;
    // Also appended to by createVersionFromManifest, under the mutex.
    mutable std::map<juce::String, std::vector<VersionSummary>> branchVersions;
    std::map<juce::String, juce::String> branchErrors;
    std::map<juce::String, juce::String> versionErrors;
    juce::String createVersionError; // creating a version answers 500 with this message
    std::map<juce::String, std::shared_ptr<BlockingGate>> branchFetchGates;
    std::map<juce::String, std::shared_ptr<BlockingGate>> versionFetchGates;
    std::shared_ptr<std::atomic<bool>> branchFetchReturned;
    std::atomic<bool> rejectToken { false };     // every call answers 401
    std::atomic<bool> offline { false };         // fetchCurrentUser gets no response
    std::atomic<bool> corruptDownloads { false }; // downloaded blobs don't match their hash
    std::atomic<bool> failProjectList { false };  // fetchProjects answers 500

private:
    std::shared_ptr<BlockingGate> findGate(const std::map<juce::String, std::shared_ptr<BlockingGate>>& gates,
                                           const juce::String& key) const
    {
        const std::lock_guard<std::mutex> lock(mutex);
        const auto it = gates.find(key);
        return it != gates.end() ? it->second : nullptr;
    }

    mutable std::mutex mutex;
    mutable std::map<juce::String, juce::MemoryBlock> blobs;
    mutable std::map<juce::String, int> uploadsBySha;
    mutable std::map<juce::String, juce::String> manifestsByVersion;
    mutable std::vector<CreatedVersion> createdVersions;
    mutable std::vector<Project> createdProjects;
    std::map<juce::String, std::shared_ptr<BlockingGate>> checkMissingGates;
    std::map<juce::String, std::shared_ptr<BlockingGate>> downloadGates;
};
}
