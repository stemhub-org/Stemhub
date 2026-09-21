#pragma once

#include <JuceHeader.h>
#include "domain/User.hpp"
#include "domain/Branch.hpp"
#include "domain/Project.hpp"
#include "network/ApiUtils.hpp"

class IProjectApi
{
public:
    virtual ~IProjectApi() = default;

    virtual ApiResult<juce::var> requestJson(const juce::String& path,
                                             const juce::String& httpMethod,
                                             const juce::String& requestBody,
                                             const juce::String& bearerToken) const = 0;
    virtual ApiResult<juce::var> uploadFile(const juce::String& path,
                                            const juce::File& file,
                                            const juce::String& formFieldName,
                                            const juce::String& bearerToken) const = 0;
    virtual juce::Result downloadFile(const juce::String& path,
                                     const juce::File& destinationFile,
                                     const juce::String& bearerToken) const = 0;

    virtual ApiResult<LoginResponse> login(const juce::String& email, const juce::String& password) const = 0;
    virtual ApiResult<User> fetchCurrentUser(const juce::String& accessToken) const = 0;
    virtual ApiResult<std::vector<Project>> fetchProjects(const juce::String& accessToken) const = 0;
    virtual ApiResult<Project> createProject(const juce::String& name, const juce::String& accessToken) const = 0;
    virtual ApiResult<std::vector<Branch>> fetchBranches(const juce::String& projectId, const juce::String& accessToken) const = 0;

    // ── Content-addressed storage (see docs/content-addressed-storage.md) ──
    // Batched "which of these blobs do you already have?" query. Returns the
    // subset the server does NOT have, so the client only uploads those.
    virtual ApiResult<std::vector<juce::String>> checkMissingBlobs(const juce::String& projectId,
                                                                    const std::vector<juce::String>& sha256s,
                                                                    const juce::String& accessToken) const = 0;
    // Idempotent PUT of a single blob. sha256 in URL is verified against the
    // received bytes server-side; mismatch → 400 and no row.
    virtual ApiResult<juce::var> uploadBlob(const juce::String& projectId,
                                             const juce::String& sha256,
                                             const juce::File& file,
                                             const juce::String& accessToken) const = 0;
    // Create a Version by referencing already-uploaded blobs.
    virtual ApiResult<juce::var> createVersionFromManifest(const juce::String& branchId,
                                                            const juce::var& payload,
                                                            const juce::String& accessToken) const = 0;
    // Download a single blob to a local file. Backend may 307-redirect to a
    // presigned URL (GCS) — juce::URL's input stream follows redirects by
    // default so the implementation is a normal GET.
    virtual juce::Result downloadBlob(const juce::String& projectId,
                                       const juce::String& sha256,
                                       const juce::File& destinationFile,
                                       const juce::String& accessToken) const = 0;
};

class ApiClient final : public IProjectApi
{
public:
    explicit ApiClient(juce::String baseUrl = {});

    ApiResult<juce::var> requestJson(const juce::String& path,
                                     const juce::String& httpMethod,
                                     const juce::String& requestBody,
                                     const juce::String& bearerToken) const override;
    ApiResult<juce::var> uploadFile(const juce::String& path,
                                    const juce::File& file,
                                    const juce::String& formFieldName,
                                    const juce::String& bearerToken) const override;
    juce::Result downloadFile(const juce::String& path,
                              const juce::File& destinationFile,
                              const juce::String& bearerToken) const override;

    ApiResult<LoginResponse> login(const juce::String& email, const juce::String& password) const override;
    ApiResult<User> fetchCurrentUser(const juce::String& accessToken) const override;
    ApiResult<std::vector<Project>> fetchProjects(const juce::String& accessToken) const override;
    ApiResult<Project> createProject(const juce::String& name, const juce::String& accessToken) const override;
    ApiResult<std::vector<Branch>> fetchBranches(const juce::String& projectId, const juce::String& accessToken) const override;

    ApiResult<std::vector<juce::String>> checkMissingBlobs(const juce::String& projectId,
                                                            const std::vector<juce::String>& sha256s,
                                                            const juce::String& accessToken) const override;
    ApiResult<juce::var> uploadBlob(const juce::String& projectId,
                                     const juce::String& sha256,
                                     const juce::File& file,
                                     const juce::String& accessToken) const override;
    ApiResult<juce::var> createVersionFromManifest(const juce::String& branchId,
                                                    const juce::var& payload,
                                                    const juce::String& accessToken) const override;
    juce::Result downloadBlob(const juce::String& projectId,
                               const juce::String& sha256,
                               const juce::File& destinationFile,
                               const juce::String& accessToken) const override;

private:
    juce::String baseUrl;
};
