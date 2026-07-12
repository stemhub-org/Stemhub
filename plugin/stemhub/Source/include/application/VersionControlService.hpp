#pragma once

#include <JuceHeader.h>
#include "network/ApiClient.hpp"
#include "application/SnapshotBundler.hpp"
#include "application/VersionControlUtils.hpp"

struct PushVersionCasRequest
{
    juce::String projectId;
    juce::String branchId;
    juce::String commitMessage;
    juce::String parentVersionId;
    ContentAddressedManifest manifest;
};

class VersionControlService
{
    public:
        VersionControlService() noexcept = default;
        explicit VersionControlService(IProjectApi& apiClientIn) noexcept
            : apiClient(&apiClientIn) {}

        void setApiClient(IProjectApi& apiClientIn) noexcept
        {
            apiClient = &apiClientIn;
        }
    
        juce::Result pushVersion(const PushVersionRequest& request);

        // Content-addressed push: hash → check-missing → upload novel blobs →
        // create version from manifest. See docs/content-addressed-storage.md.
        // Only re-uploads blobs the server doesn't already have.
        juce::Result pushVersionContentAddressed(const PushVersionCasRequest& request);

        // Content-addressed pull: fetch the version's manifest, download each
        // referenced blob into restoreDirectory (verifying SHA-256 per file),
        // and return the path of the project file entry via outProjectFile.
        juce::Result restoreVersionFromManifest(const juce::String& projectId,
                                                const juce::String& versionId,
                                                const juce::File& restoreDirectory,
                                                juce::File& outProjectFile);

        ApiResult<std::vector<VersionSummary>> fetchVersionHistory(
            const juce::String& branchId,
            const juce::String& accessToken) const;
        
        ApiResult<VersionSummary> fetchVersion(
            const juce::String& versionId,
            const juce::String& accessToken) const;
        
        juce::Result downloadVersion(
            const juce::String& versionId,
            const juce::File& destinationFile,
            const juce::String& accessToken) const;
        
        juce::Result restoreVersion(
            const juce::String& versionId,
            const juce::File& destinationFile,
            const juce::String& accessToken) const;
        
        void setCurrentProjectContext(ProjectVersionContext newContext) noexcept { context = std::move(newContext); }
        void clearProjectContext() noexcept { context = {}; }
        void setAccessToken(juce::String newAccessToken) { accessToken = std::move(newAccessToken); }
        void clearAccessToken() noexcept { accessToken.clear(); }
        
        [[nodiscard]] const ProjectVersionContext& getCurrentProjectContext() const noexcept { return context; }
        [[nodiscard]] const juce::String& getAccessToken() const noexcept { return accessToken; }
        [[nodiscard]] juce::String getCurrentProjectId() const { return context.projectId; }
        [[nodiscard]] juce::String getCurrentBranchId() const { return context.branchId; }
        [[nodiscard]] juce::String getLastVersionId() const { return context.lastVersionId; }
        
        void setLastVersionId(juce::String versionId) { context.lastVersionId = std::move(versionId); }
        
    private:
        [[nodiscard]] juce::String resolveParentVersionId(const PushVersionRequest& request) const;
        [[nodiscard]] const IProjectApi* getApiClient() const noexcept;

        IProjectApi* apiClient = nullptr;
        ProjectVersionContext context;
        juce::String accessToken;
};
