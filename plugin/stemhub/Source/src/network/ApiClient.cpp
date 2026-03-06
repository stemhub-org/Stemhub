#include "network/ApiClient.hpp"
#include "network/ApiUtils.hpp"

namespace
{
juce::String resolveApiBaseUrl(juce::String configuredBaseUrl)
{
    if (configuredBaseUrl.isNotEmpty())
        return configuredBaseUrl;

    const auto fromEnvironment = juce::SystemStats::getEnvironmentVariable("STEMHUB_API_BASE_URL", {});
    if (fromEnvironment.isNotEmpty())
        return fromEnvironment;

    return "http://localhost:8000";
}

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
        const auto detail = object->getProperty("detail");
        if (detail.isString())
        {
            const auto detailString = detail.toString();
            if (detailString.isNotEmpty())
                return detailString;
        }

        if (detail.isArray())
        {
            if (const auto* array = detail.getArray(); array != nullptr && !array->isEmpty())
            {
                if (auto* firstObject = array->getReference(0).getDynamicObject())
                {
                    const auto message = firstObject->getProperty("msg").toString();
                    if (message.isNotEmpty())
                        return message;
                }
            }
        }
    }

    return responseText.isNotEmpty() ? responseText : fallback;
}

ApiResult<Project> parseProject(const juce::var& value)
{
    auto* object = value.getDynamicObject();
    if (object == nullptr)
        return { {}, ApiError { 200, "Project response is not a JSON object." } };

    Project project;
    project.id = object->getProperty("id").toString();
    project.ownerId = object->getProperty("owner_id").toString();
    project.name = object->getProperty("name").toString();
    project.description = object->getProperty("description").toString();
    project.category = object->getProperty("category").toString();
    project.isPublic = static_cast<bool>(object->getProperty("is_public"));
    project.isDeleted = static_cast<bool>(object->getProperty("is_deleted"));

    if (!project.isValid())
        return { {}, ApiError { 200, "Project response is missing required fields." } };

    return { project, {} };
}

ApiResult<Branch> parseBranch(const juce::var& value)
{
    auto* object = value.getDynamicObject();
    if (object == nullptr)
        return { {}, ApiError { 200, "Branch response is not a JSON object." } };

    Branch branch;
    branch.id = object->getProperty("id").toString();
    branch.projectId = object->getProperty("project_id").toString();
    branch.name = object->getProperty("name").toString();
    branch.isDeleted = static_cast<bool>(object->getProperty("is_deleted"));

    if (!branch.isValid())
        return { {}, ApiError { 200, "Branch response is missing required fields." } };

    return { branch, {} };
}
}

ApiClient::ApiClient(juce::String apiBaseUrl)
    : baseUrl(resolveApiBaseUrl(std::move(apiBaseUrl)))
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
        const auto project = parseProject(item);
        if (!project.ok())
            return { {}, project.error };

        projects.push_back(*project.value);
    }

    return { projects, {} };
}

ApiResult<Project> ApiClient::createProject(const juce::String& name, const juce::String& accessToken) const
{
    juce::DynamicObject::Ptr bodyObject = new juce::DynamicObject();
    bodyObject->setProperty("name", name);
    bodyObject->setProperty("description", {});
    bodyObject->setProperty("category", "General");
    bodyObject->setProperty("is_public", false);

    const auto body = juce::JSON::toString(juce::var(bodyObject.get()));
    const auto jsonResult = requestJson("/projects/", "POST", body, accessToken);
    if (!jsonResult.ok())
        return { {}, jsonResult.error };

    return parseProject(*jsonResult.value);
}

ApiResult<std::vector<Branch>> ApiClient::fetchBranches(const juce::String& projectId, const juce::String& accessToken) const
{
    auto jsonResult = requestJson("/projects/" + projectId + "/branches/", "GET", {}, accessToken);
    if (!jsonResult.ok())
        return { {}, jsonResult.error };

    if (!jsonResult.value->isArray())
        return { {}, ApiError { 200, "Branches response is not a JSON array." } };

    std::vector<Branch> branches;
    const auto* array = jsonResult.value->getArray();
    branches.reserve(static_cast<size_t>(array->size()));

    for (const auto& item : *array)
    {
        const auto branch = parseBranch(item);
        if (!branch.ok())
            return { {}, branch.error };

        branches.push_back(*branch.value);
    }

    return { branches, {} };
}
