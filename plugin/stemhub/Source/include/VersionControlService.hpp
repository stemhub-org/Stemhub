#pragma once

#include <JuceHeader.h>
#include "ApiClient.hpp"
#include "VersionControlUtils.hpp"

class VersionControlService
{
    public:
        explicit VersionControlService(ApiClient& apiClientIn) noexcept
            : apiClient(apiClientIn) {}
    
        juce::Result pushVersion(const PushVersionRequest& request);
    
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
        
        ApiClient& apiClient;
        ProjectVersionContext context;
        juce::String accessToken;
};
