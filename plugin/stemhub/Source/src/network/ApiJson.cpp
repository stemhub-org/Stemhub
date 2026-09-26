#include "network/ApiJson.hpp"

namespace
{
ApiError invalidResponse(const juce::String& message)
{
    return { ApiError::Kind::invalidResponse, 200, message };
}

juce::int64 sizeOf(const juce::var& blobRef)
{
    return juce::jmax<juce::int64>(0, static_cast<juce::int64>(blobRef.getProperty("size_bytes", 0)));
}

juce::int64 sumManifestSizes(const juce::var& manifest)
{
    if (!manifest.isObject())
        return 0;

    auto total = sizeOf(manifest.getProperty("project_file", {}));
    if (const auto* tracks = manifest.getProperty("tracks", {}).getArray())
        for (const auto& track : *tracks)
            total += sizeOf(track);

    return total;
}

template <typename Item, typename Parser>
ApiResult<std::vector<Item>> parseList(const juce::var& value, Parser&& parseItem, const juce::String& what)
{
    const auto* array = value.getArray();
    if (array == nullptr)
        return ApiResult<std::vector<Item>>::failure(invalidResponse(what + " response is not a JSON array."));

    std::vector<Item> items;
    items.reserve(static_cast<size_t>(array->size()));

    for (const auto& element : *array)
    {
        auto item = parseItem(element);
        if (!item.ok())
            return ApiResult<std::vector<Item>>::failure(*item.error);

        items.push_back(std::move(*item.value));
    }

    return ApiResult<std::vector<Item>>::success(std::move(items));
}
}

namespace stemhub::api::json
{
ApiResult<LoginResponse> parseLogin(const juce::var& value)
{
    if (!value.isObject())
        return ApiResult<LoginResponse>::failure(invalidResponse("Login response is not a JSON object."));

    LoginResponse response;
    response.accessToken = value.getProperty("access_token", {}).toString();

    if (response.accessToken.isEmpty())
        return ApiResult<LoginResponse>::failure(invalidResponse("Login response did not contain an access token."));

    return ApiResult<LoginResponse>::success(response);
}

ApiResult<User> parseUser(const juce::var& value)
{
    if (!value.isObject())
        return ApiResult<User>::failure(invalidResponse("User response is not a JSON object."));

    User user;
    user.id = value.getProperty("id", {}).toString();
    user.email = value.getProperty("email", {}).toString();
    user.username = value.getProperty("username", {}).toString();

    if (!user.isValid())
        return ApiResult<User>::failure(invalidResponse("User response is missing required fields."));

    return ApiResult<User>::success(user);
}

ApiResult<Project> parseProject(const juce::var& value)
{
    if (!value.isObject())
        return ApiResult<Project>::failure(invalidResponse("Project response is not a JSON object."));

    Project project;
    project.id = value.getProperty("id", {}).toString();
    project.name = value.getProperty("name", {}).toString();
    project.description = value.getProperty("description", {}).toString();
    project.category = value.getProperty("category", {}).toString();
    project.isPublic = static_cast<bool>(value.getProperty("is_public", false));

    if (!project.isValid())
        return ApiResult<Project>::failure(invalidResponse("Project response is missing required fields."));

    return ApiResult<Project>::success(project);
}

ApiResult<Branch> parseBranch(const juce::var& value)
{
    if (!value.isObject())
        return ApiResult<Branch>::failure(invalidResponse("Branch response is not a JSON object."));

    Branch branch;
    branch.id = value.getProperty("id", {}).toString();
    branch.projectId = value.getProperty("project_id", {}).toString();
    branch.name = value.getProperty("name", {}).toString();

    if (!branch.isValid())
        return ApiResult<Branch>::failure(invalidResponse("Branch response is missing required fields."));

    return ApiResult<Branch>::success(branch);
}

ApiResult<VersionSummary> parseVersionSummary(const juce::var& value)
{
    if (!value.isObject())
        return ApiResult<VersionSummary>::failure(invalidResponse("Version response is not a JSON object."));

    VersionSummary summary;
    summary.id = value.getProperty("id", {}).toString();
    summary.branchId = value.getProperty("branch_id", {}).toString();
    summary.parentVersionId = value.getProperty("parent_version_id", {}).toString();
    summary.createdAt = value.getProperty("created_at", {}).toString();
    summary.commitMessage = value.getProperty("commit_message", {}).toString();
    summary.sourceDaw = value.getProperty("source_daw", {}).toString();
    summary.sourceProjectFilename = value.getProperty("source_project_filename", {}).toString();
    summary.totalSizeBytes = sumManifestSizes(value.getProperty("manifest_json", {}));

    if (!summary.isValid())
        return ApiResult<VersionSummary>::failure(invalidResponse("Version response is missing required fields."));

    return ApiResult<VersionSummary>::success(summary);
}

ApiResult<std::vector<Project>> parseProjects(const juce::var& value)
{
    return parseList<Project>(value, parseProject, "Projects");
}

ApiResult<std::vector<Branch>> parseBranches(const juce::var& value)
{
    return parseList<Branch>(value, parseBranch, "Branches");
}

ApiResult<std::vector<VersionSummary>> parseVersions(const juce::var& value)
{
    return parseList<VersionSummary>(value, parseVersionSummary, "Version history");
}

ApiResult<std::vector<juce::String>> parseMissingBlobs(const juce::var& value)
{
    const auto* missing = value.getProperty("missing", {}).getArray();
    if (missing == nullptr)
        return ApiResult<std::vector<juce::String>>::failure(invalidResponse("check-missing response has no 'missing' array."));

    std::vector<juce::String> shas;
    shas.reserve(static_cast<size_t>(missing->size()));
    for (const auto& sha : *missing)
        shas.push_back(sha.toString());

    return ApiResult<std::vector<juce::String>>::success(std::move(shas));
}

juce::String extractErrorMessage(const juce::var& parsedJson,
                                 const juce::String& responseText,
                                 const juce::String& fallback)
{
    const auto detail = parsedJson.getProperty("detail", {});

    if (detail.isString() && detail.toString().isNotEmpty())
        return detail.toString();

    if (const auto* errors = detail.getArray(); errors != nullptr && !errors->isEmpty())
    {
        const auto message = errors->getReference(0).getProperty("msg", {}).toString();
        if (message.isNotEmpty())
            return message;
    }

    // A short plain-text body is readable; an HTML error page or a long dump isn't.
    const auto text = responseText.trim();
    if (text.isNotEmpty() && text.length() <= 200 && !text.startsWithChar('<'))
        return text;

    return fallback;
}
}
