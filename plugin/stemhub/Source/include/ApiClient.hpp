#pragma once

#include <JuceHeader.h>
#include "User.hpp"
#include "ApiUtils.hpp"

class ApiClient
{
    public:
        explicit ApiClient(juce::String baseUrl = "http://localhost:8000");

        ApiResult<juce::var> requestJson(const juce::String& path,
                                        const juce::String& httpMethod,
                                        const juce::String& requestBody,
                                        const juce::String& bearerToken) const;

        ApiResult<LoginResponse> login(const juce::String& email, const juce::String& password) const;
        ApiResult<User> fetchCurrentUser(const juce::String& accessToken) const;
    private:
        juce::String baseUrl;
};
