#pragma once

#include <utility>
#include <vector>

#include <JuceHeader.h>

#include "network/ApiClient.hpp"

// The backend as the signed-in user sees it: IProjectApi with their token on every call. A job
// gets one for the token the session had when it started the job, so no input carries a token.
// Holds a reference to the API, so it lives no longer than the job it was made for.
class SignedInApi
{
public:
    SignedInApi(const IProjectApi& apiToUse, juce::String accessToken)
        : api(apiToUse), token(std::move(accessToken))
    {
    }

    ApiResult<User> fetchCurrentUser() const { return api.fetchCurrentUser(token); }
    ApiResult<std::vector<Project>> fetchProjects() const { return api.fetchProjects(token); }
    ApiResult<Project> createProject(const juce::String& name) const { return api.createProject(name, token); }
    ApiResult<std::vector<Branch>> fetchBranches(const juce::String& projectId) const { return api.fetchBranches(projectId, token); }
    ApiResult<std::vector<VersionSummary>> fetchVersions(const juce::String& branchId) const { return api.fetchVersions(branchId, token); }
    ApiResult<juce::var> fetchVersionManifest(const juce::String& versionId) const { return api.fetchVersionManifest(versionId, token); }

    ApiResult<std::vector<juce::String>> checkMissingBlobs(const juce::String& projectId, const std::vector<juce::String>& sha256s) const
    {
        return api.checkMissingBlobs(projectId, sha256s, token);
    }

    ApiResult<Unit> uploadBlob(const juce::String& projectId, const juce::String& sha256, const juce::File& file) const
    {
        return api.uploadBlob(projectId, sha256, file, token);
    }

    ApiResult<VersionSummary> createVersionFromManifest(const juce::String& branchId, const CreateVersionRequest& request) const
    {
        return api.createVersionFromManifest(branchId, request, token);
    }

    ApiResult<Unit> downloadBlob(const juce::String& projectId, const juce::String& sha256, const juce::File& destinationFile) const
    {
        return api.downloadBlob(projectId, sha256, destinationFile, token);
    }

private:
    const IProjectApi& api;
    juce::String token;
};
