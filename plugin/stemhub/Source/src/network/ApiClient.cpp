#include "network/ApiClient.hpp"
#include "network/ApiConfig.hpp"
#include "network/ApiJson.hpp"

namespace
{
namespace json = stemhub::api::json;

// How long a request may stall (connecting, or waiting for bytes) before it fails. JUCE applies
// this to each phase of a request, not to its total duration.
constexpr int kRequestTimeoutMs = 10000;
constexpr int kDownloadTimeoutMs = 30000;
constexpr int kUploadTimeoutMs = 120000;

juce::String authorizationHeader(const juce::String& accessToken)
{
    return accessToken.isNotEmpty() ? "Authorization: Bearer " + accessToken + "\r\n" : juce::String();
}

struct HttpResponse
{
    std::unique_ptr<juce::InputStream> body; // null when nothing came back
    int statusCode { 0 };
    juce::StringPairArray headers;
};

HttpResponse send(const juce::URL& url,
                  const juce::String& method,
                  const juce::String& extraHeaders,
                  int timeoutMs,
                  int maxRedirects = 5)
{
    HttpResponse response;
    response.body = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                              .withHttpRequestCmd(method)
                                              .withExtraHeaders(extraHeaders)
                                              .withResponseHeaders(&response.headers)
                                              .withStatusCode(&response.statusCode)
                                              .withNumRedirectsToFollow(maxRedirects)
                                              .withConnectionTimeoutMs(timeoutMs));
    return response;
}

bool isSuccessStatus(const int statusCode)
{
    return statusCode >= 200 && statusCode < 300;
}

bool isRedirectStatus(const int statusCode)
{
    return statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 307 || statusCode == 308;
}

ApiError networkError()
{
    return { ApiError::Kind::network, 0, "Can't reach StemHub. Check your connection and try again." };
}

// Absolute locations are used as they are; "/path" is resolved against the request's origin.
juce::String resolveRedirectLocation(const juce::String& requestUrl, const juce::String& location)
{
    if (location.startsWithIgnoreCase("https://") || location.startsWithIgnoreCase("http://"))
        return location;

    if (!location.startsWithChar('/'))
        return {};

    const auto schemeEnd = requestUrl.indexOf("://");
    if (schemeEnd < 0)
        return {};

    const auto pathStart = requestUrl.indexOfChar(schemeEnd + 3, '/');
    return (pathStart < 0 ? requestUrl : requestUrl.substring(0, pathStart)) + location;
}

// Reads a response body as JSON, or turns a failed response into an ApiError.
ApiResult<juce::var> readJson(HttpResponse response, const juce::String& failureMessage)
{
    if (response.body == nullptr)
        return ApiResult<juce::var>::failure(networkError());

    const auto text = response.body->readEntireStreamAsString();
    const auto parsed = juce::JSON::parse(text);

    if (!isSuccessStatus(response.statusCode))
        return ApiResult<juce::var>::failure(
            ApiError::fromStatus(response.statusCode, json::extractErrorMessage(parsed, text, failureMessage)));

    if (parsed.isVoid())
        return ApiResult<juce::var>::failure({ ApiError::Kind::invalidResponse, response.statusCode, "StemHub returned invalid JSON." });

    return ApiResult<juce::var>::success(parsed);
}

// Chains a JSON response into one of the json:: parsers.
template <typename Parser>
auto parseResponse(const ApiResult<juce::var>& response, Parser&& parse) -> decltype(parse(juce::var()))
{
    using Result = decltype(parse(juce::var()));
    if (!response.ok())
        return Result::failure(*response.error);

    return parse(*response.value);
}

juce::var makeObject(std::initializer_list<std::pair<const char*, juce::var>> properties)
{
    auto* object = new juce::DynamicObject();
    for (const auto& [name, value] : properties)
        object->setProperty(name, value);

    return juce::var(object);
}
}

ApiClient::ApiClient(juce::String apiBaseUrl)
    : baseUrl(apiBaseUrl.isNotEmpty() ? std::move(apiBaseUrl) : stemhub::api::resolveBaseUrl())
{
}

ApiResult<juce::var> ApiClient::requestJson(const juce::String& method,
                                            const juce::String& path,
                                            const juce::var& body,
                                            const juce::String& accessToken,
                                            const juce::String& failureMessage) const
{
    auto url = juce::URL(baseUrl + path);
    if (!body.isVoid())
        url = url.withPOSTData(juce::JSON::toString(body));

    const auto headers = "Content-Type: application/json\r\nAccept: application/json\r\n" + authorizationHeader(accessToken);
    return readJson(send(url, method, headers, kRequestTimeoutMs), failureMessage);
}

ApiResult<LoginResponse> ApiClient::login(const juce::String& email, const juce::String& password) const
{
    return parseResponse(requestJson("POST", "/auth/login", makeObject({ { "email", email }, { "password", password } }), {}, "Failed to sign in."),
                         json::parseLogin);
}

ApiResult<User> ApiClient::fetchCurrentUser(const juce::String& accessToken) const
{
    return parseResponse(requestJson("GET", "/auth/me", {}, accessToken, "Failed to load your user profile."),
                         json::parseUser);
}

ApiResult<std::vector<Project>> ApiClient::fetchProjects(const juce::String& accessToken) const
{
    return parseResponse(requestJson("GET", "/projects/", {}, accessToken, "Failed to load projects."),
                         json::parseProjects);
}

ApiResult<Project> ApiClient::createProject(const juce::String& name, const juce::String& accessToken) const
{
    const auto body = makeObject({ { "name", name },
                                   { "description", juce::var() },
                                   { "category", "General" },
                                   { "is_public", false } });
    return parseResponse(requestJson("POST", "/projects/", body, accessToken, "Failed to create the project."),
                         json::parseProject);
}

ApiResult<std::vector<Branch>> ApiClient::fetchBranches(const juce::String& projectId, const juce::String& accessToken) const
{
    return parseResponse(requestJson("GET", "/projects/" + projectId + "/branches/", {}, accessToken, "Failed to load workspaces."),
                         json::parseBranches);
}

ApiResult<std::vector<VersionSummary>> ApiClient::fetchVersions(const juce::String& branchId,
                                                                const juce::String& accessToken) const
{
    return parseResponse(requestJson("GET", "/branches/" + branchId + "/versions/", {}, accessToken, "Failed to load version history."),
                         json::parseVersions);
}

ApiResult<juce::var> ApiClient::fetchVersionManifest(const juce::String& versionId, const juce::String& accessToken) const
{
    const auto version = requestJson("GET", "/versions/" + versionId, {}, accessToken, "Failed to load the version.");
    if (!version.ok())
        return version;

    const auto manifest = version.value->getProperty("manifest_json", {});
    if (!manifest.isObject())
        return ApiResult<juce::var>::failure({ ApiError::Kind::notFound, 404, "This version has no file list." });

    return ApiResult<juce::var>::success(manifest);
}

ApiResult<std::vector<juce::String>> ApiClient::checkMissingBlobs(const juce::String& projectId,
                                                                   const std::vector<juce::String>& sha256s,
                                                                   const juce::String& accessToken) const
{
    juce::Array<juce::var> hashes;
    for (const auto& sha : sha256s)
        hashes.add(sha);

    return parseResponse(requestJson("POST",
                                     "/projects/" + projectId + "/blobs/check-missing",
                                     makeObject({ { "sha256s", hashes } }),
                                     accessToken,
                                     "Failed to check which files StemHub already has."),
                         json::parseMissingBlobs);
}

ApiResult<Unit> ApiClient::uploadBlob(const juce::String& projectId,
                                      const juce::String& sha256,
                                      const juce::File& file,
                                      const juce::String& accessToken) const
{
    if (!file.existsAsFile())
        return ApiResult<Unit>::failure({ ApiError::Kind::localFile, 0, file.getFileName() + " no longer exists." });

    // Multipart/form-data with a "file" field, as routers/blobs.py::upload_blob expects.
    const auto url = juce::URL(baseUrl + "/projects/" + projectId + "/blobs/" + sha256)
                         .withFileToUpload("file", file, "application/octet-stream");
    const auto response = readJson(send(url, "PUT", "Accept: application/json\r\n" + authorizationHeader(accessToken), kUploadTimeoutMs),
                                   "Failed to upload " + file.getFileName() + ".");
    if (!response.ok())
        return ApiResult<Unit>::failure(*response.error);

    return ApiResult<Unit>::success({});
}

ApiResult<VersionSummary> ApiClient::createVersionFromManifest(const juce::String& branchId,
                                                               const CreateVersionRequest& request,
                                                               const juce::String& accessToken) const
{
    auto* body = new juce::DynamicObject();
    if (request.commitMessage.isNotEmpty())
        body->setProperty("commit_message", request.commitMessage);
    if (request.parentVersionId.isNotEmpty())
        body->setProperty("parent_version_id", request.parentVersionId);
    body->setProperty("manifest", request.manifest);

    return parseResponse(requestJson("POST",
                                     "/branches/" + branchId + "/versions/from-manifest",
                                     juce::var(body),
                                     accessToken,
                                     "Failed to create the version."),
                         json::parseVersionSummary);
}

ApiResult<Unit> ApiClient::downloadBlob(const juce::String& projectId,
                                        const juce::String& sha256,
                                        const juce::File& destinationFile,
                                        const juce::String& accessToken) const
{
    // The API answers with the bytes, or with a 307 to a presigned storage URL. That redirect
    // is followed here rather than by JUCE, which would send our bearer token to the storage
    // host as well (the Windows implementation re-sends every extra header).
    const auto apiUrl = baseUrl + "/projects/" + projectId + "/blobs/" + sha256;
    auto response = send(juce::URL(apiUrl),
                         "GET",
                         "Accept: application/octet-stream\r\n" + authorizationHeader(accessToken),
                         kDownloadTimeoutMs,
                         0);

    if (response.body != nullptr && isRedirectStatus(response.statusCode))
    {
        const auto storageUrl = resolveRedirectLocation(apiUrl, response.headers.getValue("Location", {}));
        if (storageUrl.isEmpty())
            return ApiResult<Unit>::failure({ ApiError::Kind::invalidResponse, response.statusCode,
                                              "The file download was redirected to an invalid location." });

        // Presigned URLs carry their own authorization; parsing them would re-encode the signature.
        response = send(juce::URL::createWithoutParsing(storageUrl), "GET", {}, kDownloadTimeoutMs);
    }

    if (response.body == nullptr)
        return ApiResult<Unit>::failure(networkError());

    if (!isSuccessStatus(response.statusCode))
    {
        const auto text = response.body->readEntireStreamAsString();
        return ApiResult<Unit>::failure(ApiError::fromStatus(
            response.statusCode,
            json::extractErrorMessage(juce::JSON::parse(text), {}, "File download failed (HTTP " + juce::String(response.statusCode) + ").")));
    }

    juce::FileOutputStream output(destinationFile);
    if (!output.openedOk())
        return ApiResult<Unit>::failure({ ApiError::Kind::localFile, 0, "Could not write " + destinationFile.getFullPathName() });

    output.setPosition(0);
    output.truncate();
    if (output.writeFromInputStream(*response.body, -1) < 0 || !output.getStatus().wasOk())
        return ApiResult<Unit>::failure({ ApiError::Kind::localFile, 0, "Could not write " + destinationFile.getFullPathName() });

    output.flush();
    return ApiResult<Unit>::success({});
}
