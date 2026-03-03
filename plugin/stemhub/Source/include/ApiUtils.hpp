#pragma once
#include <JuceHeader.h>

struct ApiError
{
    int statusCode {};
    juce::String message;
};

template <typename T>
struct ApiResult
{
    std::optional<T> value;
    std::optional<ApiError> error;

    bool ok() const noexcept { return value.has_value(); }
};

struct LoginResponse
{
    juce::String accessToken;
    juce::String tokenType;
};
