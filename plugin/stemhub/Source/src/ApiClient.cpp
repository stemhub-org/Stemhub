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

juce::String buildBinaryHeaders(const juce::String& bearerToken)
{
    juce::String headers;
    headers << "Accept: application/octet-stream\r\n";

    if (bearerToken.isNotEmpty())
        headers << "Authorization: Bearer " << bearerToken << "\r\n";

    return headers;
}

juce::String extractErrorMessage(const juce::var& parsedJson,
                                 const juce::String& responseText,
                                 const juce::String& fallback)
{
    if (auto* object = parsedJson.getDynamicObject())
    {
        const auto detail = object->getProperty("detail").toString();
        if (detail.isNotEmpty())
            return detail;
    }

    return responseText.isNotEmpty() ? responseText : fallback;
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
        return { {}, ApiError { statusCode, extractErrorMessage(parsedJson,
                                                                responseText,
                                                                "Backend request failed.") } };
    }

    if (parsedJson.isVoid())
        return { {}, ApiError { statusCode, "Backend returned invalid JSON." } };

    return { parsedJson, {} };
}

ApiResult<juce::var> ApiClient::uploadFile(const juce::String& path,
                                           const juce::File& file,
                                           const juce::String& formFieldName,
                                           const juce::String& bearerToken) const
{
    if (!file.existsAsFile())
        return { {}, ApiError { 0, "Snapshot file does not exist." } };

    auto url = juce::URL(baseUrl + path).withFileToUpload(formFieldName, file, "application/octet-stream");

    juce::StringPairArray responseHeaders;
    int statusCode = 0;

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withHttpRequestCmd("POST")
        .withExtraHeaders("Accept: application/json\r\nAuthorization: Bearer " + bearerToken + "\r\n")
        .withResponseHeaders(&responseHeaders)
        .withStatusCode(&statusCode)
        .withConnectionTimeoutMs(30000);

    auto stream = url.createInputStream(options);
    if (stream == nullptr)
        return { {}, ApiError { statusCode, "Failed to connect to backend." } };

    const auto responseText = stream->readEntireStreamAsString();
    const auto parsedJson = juce::JSON::parse(responseText);

    if (statusCode < 200 || statusCode >= 300)
    {
        return { {}, ApiError { statusCode, extractErrorMessage(parsedJson,
                                                                responseText,
                                                                "File upload failed.") } };
    }

    if (parsedJson.isVoid())
        return { {}, ApiError { statusCode, "Backend returned invalid JSON." } };

    return { parsedJson, {} };
}

juce::Result ApiClient::downloadFile(const juce::String& path,
                                     const juce::File& destinationFile,
                                     const juce::String& bearerToken) const
{
    auto url = juce::URL(baseUrl + path);

    juce::StringPairArray responseHeaders;
    int statusCode = 0;

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withHttpRequestCmd("GET")
        .withExtraHeaders(buildBinaryHeaders(bearerToken))
        .withResponseHeaders(&responseHeaders)
        .withStatusCode(&statusCode)
        .withConnectionTimeoutMs(30000);

    auto stream = url.createInputStream(options);
    if (stream == nullptr)
        return juce::Result::fail("Failed to connect to backend.");

    if (statusCode < 200 || statusCode >= 300)
    {
        const auto responseText = stream->readEntireStreamAsString();
        const auto parsedJson = juce::JSON::parse(responseText);
        return juce::Result::fail(extractErrorMessage(parsedJson, responseText, "File download failed."));
    }

    destinationFile.getParentDirectory().createDirectory();

    juce::FileOutputStream output(destinationFile);
    if (!output.openedOk())
        return juce::Result::fail("Failed to open destination file for writing.");

    if (output.writeFromInputStream(*stream, -1) < 0)
        return juce::Result::fail("Failed to write downloaded snapshot to disk.");

    output.flush();
    return juce::Result::ok();
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

ApiResult<std::vector<Project>> ApiClient::fetchProjects(const juce::String& accessToken) const
{
    auto jsonResult = requestJson("/projects/", "GET", {}, accessToken);
    if (!jsonResult.ok())
        return { {}, jsonResult.error };

    if (!jsonResult.value->isArray())
        return { {}, ApiError { 200, "Projects response is not a JSON array." } };

    std::vector<Project> projects;
    const auto* array = jsonResult.value->getArray();
    projects.reserve(static_cast<size_t>(array->size()));

    for (const auto& item : *array)
    {
        auto* object = item.getDynamicObject();
        if (object == nullptr)
            return { {}, ApiError { 200, "Projects response contains a non-object item." } };

        Project project;
        project.id = object->getProperty("id").toString();
        project.ownerId = object->getProperty("owner_id").toString();
        project.name = object->getProperty("name").toString();
        project.description = object->getProperty("description").toString();
        project.category = object->getProperty("category").toString();
        project.isPublic = static_cast<bool>(object->getProperty("is_public"));
        project.isDeleted = static_cast<bool>(object->getProperty("is_deleted"));

        if (!project.isValid())
            return { {}, ApiError { 200, "Projects response contains invalid project data." } };

        projects.push_back(std::move(project));
    }

    return { projects, {} };
}
