#pragma once

#include <JuceHeader.h>
#include "User.hpp"
#include "Project.hpp"
#include "ApiUtils.hpp"

class ApiClient
{
public:
    explicit ApiClient(juce::String baseUrl = "http://localhost:8000");

    ApiResult<juce::var> requestJson(const juce::String& path,
                                     const juce::String& httpMethod,
                                     const juce::String& requestBody,
                                     const juce::String& bearerToken) const;
    ApiResult<juce::var> uploadFile(const juce::String& path,
                                    const juce::File& file,
                                    const juce::String& formFieldName,
                                    const juce::String& bearerToken) const;
    juce::Result downloadFile(const juce::String& path,
                              const juce::File& destinationFile,
                              const juce::String& bearerToken) const;

    ApiResult<LoginResponse> login(const juce::String& email, const juce::String& password) const;
    ApiResult<User> fetchCurrentUser(const juce::String& accessToken) const;
    ApiResult<std::vector<Project>> fetchProjects(const juce::String& accessToken) const;

private:
    juce::String baseUrl;
};
