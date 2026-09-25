#pragma once

#include <vector>

#include <JuceHeader.h>
#include "domain/Branch.hpp"
#include "domain/Project.hpp"
#include "domain/User.hpp"
#include "domain/Version.hpp"
#include "network/ApiTypes.hpp"

struct CreateVersionRequest
{
    juce::String commitMessage;
    juce::String parentVersionId;
    juce::var manifest; // VersionManifestV1, see docs/content-addressed-storage.md
};

// The StemHub backend: one typed call per endpoint. Calls are const and keep no state, so jobs
// on several threads can share one instance.
class IProjectApi
{
public:
    virtual ~IProjectApi() = default;

    virtual ApiResult<LoginResponse> login(const juce::String& email, const juce::String& password) const = 0;
    virtual ApiResult<User> fetchCurrentUser(const juce::String& accessToken) const = 0;
    virtual ApiResult<std::vector<Project>> fetchProjects(const juce::String& accessToken) const = 0;
    virtual ApiResult<Project> createProject(const juce::String& name, const juce::String& accessToken) const = 0;
    virtual ApiResult<std::vector<Branch>> fetchBranches(const juce::String& projectId, const juce::String& accessToken) const = 0;
    virtual ApiResult<std::vector<VersionSummary>> fetchVersions(const juce::String& branchId,
                                                                 const juce::String& accessToken) const = 0;
    // The version's content-addressed manifest (manifest_json); notFound when it has none.
    virtual ApiResult<juce::var> fetchVersionManifest(const juce::String& versionId,
                                                      const juce::String& accessToken) const = 0;

    // ── Content-addressed storage (see docs/content-addressed-storage.md) ──
    // The subset of these blobs the server doesn't have, so only those are uploaded.
    virtual ApiResult<std::vector<juce::String>> checkMissingBlobs(const juce::String& projectId,
                                                                    const std::vector<juce::String>& sha256s,
                                                                    const juce::String& accessToken) const = 0;
    // Idempotent. The server checks the bytes against sha256 and refuses a mismatch.
    virtual ApiResult<Unit> uploadBlob(const juce::String& projectId,
                                       const juce::String& sha256,
                                       const juce::File& file,
                                       const juce::String& accessToken) const = 0;
    // Creates a version whose files are all uploaded already.
    virtual ApiResult<VersionSummary> createVersionFromManifest(const juce::String& branchId,
                                                                const CreateVersionRequest& request,
                                                                const juce::String& accessToken) const = 0;
    // The backend may answer with a 307 to a presigned URL, which is followed without our token.
    virtual ApiResult<Unit> downloadBlob(const juce::String& projectId,
                                         const juce::String& sha256,
                                         const juce::File& destinationFile,
                                         const juce::String& accessToken) const = 0;
};

class ApiClient final : public IProjectApi
{
public:
    // Without a base URL, the configured one is used (see network/ApiConfig.hpp).
    explicit ApiClient(juce::String baseUrl = {});

    ApiResult<LoginResponse> login(const juce::String& email, const juce::String& password) const override;
    ApiResult<User> fetchCurrentUser(const juce::String& accessToken) const override;
    ApiResult<std::vector<Project>> fetchProjects(const juce::String& accessToken) const override;
    ApiResult<Project> createProject(const juce::String& name, const juce::String& accessToken) const override;
    ApiResult<std::vector<Branch>> fetchBranches(const juce::String& projectId, const juce::String& accessToken) const override;
    ApiResult<std::vector<VersionSummary>> fetchVersions(const juce::String& branchId,
                                                         const juce::String& accessToken) const override;
    ApiResult<juce::var> fetchVersionManifest(const juce::String& versionId,
                                              const juce::String& accessToken) const override;

    ApiResult<std::vector<juce::String>> checkMissingBlobs(const juce::String& projectId,
                                                            const std::vector<juce::String>& sha256s,
                                                            const juce::String& accessToken) const override;
    ApiResult<Unit> uploadBlob(const juce::String& projectId,
                               const juce::String& sha256,
                               const juce::File& file,
                               const juce::String& accessToken) const override;
    ApiResult<VersionSummary> createVersionFromManifest(const juce::String& branchId,
                                                        const CreateVersionRequest& request,
                                                        const juce::String& accessToken) const override;
    ApiResult<Unit> downloadBlob(const juce::String& projectId,
                                 const juce::String& sha256,
                                 const juce::File& destinationFile,
                                 const juce::String& accessToken) const override;

private:
    ApiResult<juce::var> requestJson(const juce::String& method,
                                     const juce::String& path,
                                     const juce::var& body,
                                     const juce::String& accessToken,
                                     const juce::String& failureMessage) const;

    juce::String baseUrl;
};
