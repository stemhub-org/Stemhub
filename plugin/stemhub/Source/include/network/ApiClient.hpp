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

private:
    juce::String baseUrl;
};
