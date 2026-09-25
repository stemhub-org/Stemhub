#pragma once

#include <optional>
#include <vector>

#include <JuceHeader.h>

struct ApiError
{
    enum class Kind
    {
        network,         // no response: offline, DNS, timeout
        unauthorized,    // 401: the session token is missing, invalid or expired
        forbidden,       // 403
        notFound,        // 404
        conflict,        // 409
        invalidRequest,  // other 4xx, e.g. 422 validation errors
        server,          // 5xx
        invalidResponse, // a response or file that doesn't match what was expected
        localFile,       // reading or writing a file on this machine failed
        cancelled        // the job making the call was asked to stop
    };

    Kind kind { Kind::network };
    int statusCode { 0 };
    juce::String message;

    static Kind kindForStatus(int statusCode) noexcept
    {
        if (statusCode == 401) return Kind::unauthorized;
        if (statusCode == 403) return Kind::forbidden;
        if (statusCode == 404) return Kind::notFound;
        if (statusCode == 409) return Kind::conflict;
        if (statusCode >= 500) return Kind::server;
        if (statusCode >= 400) return Kind::invalidRequest;
        return statusCode == 0 ? Kind::network : Kind::invalidResponse;
    }

    static ApiError fromStatus(int statusCode, juce::String message)
    {
        return { kindForStatus(statusCode), statusCode, std::move(message) };
    }

    static ApiError cancelled()
    {
        return { Kind::cancelled, 0, "Cancelled." };
    }

    [[nodiscard]] bool isUnauthorized() const noexcept { return kind == Kind::unauthorized; }
};

// Either a value or an error.
template <typename T>
struct ApiResult
{
    std::optional<T> value;
    std::optional<ApiError> error;

    static ApiResult success(T newValue) { return { std::move(newValue), {} }; }
    static ApiResult failure(ApiError newError) { return { {}, std::move(newError) }; }

    [[nodiscard]] bool ok() const noexcept { return value.has_value(); }
    [[nodiscard]] juce::String errorMessage(const juce::String& fallback) const
    {
        return error.has_value() && error->message.isNotEmpty() ? error->message : fallback;
    }
    [[nodiscard]] bool isUnauthorized() const noexcept { return error.has_value() && error->isUnauthorized(); }
};

// The value of a call that returns nothing but success or an error.
struct Unit
{
};

// True when the thread pool running the current job has asked it to stop: a cancel, or the
// plugin closing. Long work checks it between steps; outside a pool job it is always false.
inline bool isJobCancelled()
{
    const auto* job = juce::ThreadPoolJob::getCurrentThreadPoolJob();
    return job != nullptr && job->shouldExit();
}

struct LoginResponse
{
    juce::String accessToken;
};
