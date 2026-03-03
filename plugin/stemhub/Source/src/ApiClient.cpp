#include "../include/ApiClient.hpp"
#include "../include/ApiUtils.hpp"

namespace
{
    juce::String buildJsonHeaders(const juce::String& bearerToken)
    {
        juce::String headers;
        headers << "Content-Type: application/json\r\n";
        headers << "Accept: application/json\r\n";

        if (bearerToken.isNotEmpty())
            headers << "Authorization: Bearer " << bearerToken << "\r\n";

        return headers;
    }
}

ApiClient::ApiClient(juce::String apiBaseUrl)
    : baseUrl(std::move(apiBaseUrl))
{
}

ApiResult<juce::var> ApiClient::requestJson(const juce::String& path, const juce::String& httpMethod, const juce::String& requestBody, const juce::String& bearerToken) const
{
    auto url = juce::URL(baseUrl + path);

    if (requestBody.isNotEmpty())
        url = url.withPOSTData(requestBody);

    juce::StringPairArray responseHeaders;
    int statusCode = 0;

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withHttpRequestCmd(httpMethod)
        .withExtraHeaders(buildJsonHeaders(bearerToken))
        .withResponseHeaders(&responseHeaders)
        .withStatusCode(&statusCode)
        .withConnectionTimeoutMs(10000);

    auto stream = url.createInputStream(options);

    if (stream == nullptr)
        return { {}, ApiError { statusCode, "Failed to connect to backend." } };

    const auto responseText = stream->readEntireStreamAsString();
    const auto parsedJson = juce::JSON::parse(responseText);

    if (statusCode < 200 || statusCode >= 300)
    {
        return { {}, ApiError { statusCode, responseText.isNotEmpty() ? responseText
                                                                      : "Backend request failed." } };
    }

    if (parsedJson.isVoid())
        return { {}, ApiError { statusCode, "Backend returned invalid JSON." } };

    return { parsedJson, {} };
}

ApiResult<LoginResponse> ApiClient::login(const juce::String& email,
                                          const juce::String& password) const
{
    juce::DynamicObject::Ptr bodyObject = new juce::DynamicObject();
    bodyObject->setProperty("email", email);
    bodyObject->setProperty("password", password);

    const auto body = juce::JSON::toString(juce::var(bodyObject.get()));

    auto jsonResult = requestJson("/auth/login", "POST", body, {});
    if (!jsonResult.ok())
        return { {}, jsonResult.error };

    auto* object = jsonResult.value->getDynamicObject();
    if (object == nullptr)
        return { {}, ApiError { 200, "Login response is not a JSON object." } };

    LoginResponse response;
    response.accessToken = object->getProperty("access_token").toString();
    response.tokenType = object->getProperty("token_type").toString();

    if (response.accessToken.isEmpty())
        return { {}, ApiError { 200, "Login response did not contain an access token." } };

    return { response, {} };
}

ApiResult<User> ApiClient::fetchCurrentUser(const juce::String& accessToken) const
{
    auto jsonResult = requestJson("/auth/me", "GET", {}, accessToken);
    if (!jsonResult.ok())
        return { {}, jsonResult.error };

    auto* object = jsonResult.value->getDynamicObject();
    if (object == nullptr)
        return { {}, ApiError { 200, "User response is not a JSON object." } };

    User user;
    user.id = object->getProperty("id").toString();
    user.email = object->getProperty("email").toString();
    user.username = object->getProperty("username").toString();

    if (!user.isValid())
        return { {}, ApiError { 200, "User response is missing required fields." } };

    return { user, {} };
}
