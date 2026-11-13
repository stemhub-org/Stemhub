#pragma once

#include <vector>

#include <JuceHeader.h>

#include "domain/Branch.hpp"
#include "domain/Project.hpp"
#include "domain/User.hpp"
#include "domain/Version.hpp"
#include "network/ApiTypes.hpp"

// Turns backend JSON into domain values. Pure functions: no I/O, easy to test.
namespace stemhub::api::json
{
ApiResult<LoginResponse> parseLogin(const juce::var& value);
ApiResult<User> parseUser(const juce::var& value);
ApiResult<Project> parseProject(const juce::var& value);
ApiResult<Branch> parseBranch(const juce::var& value);
ApiResult<VersionSummary> parseVersionSummary(const juce::var& value);

ApiResult<std::vector<Project>> parseProjects(const juce::var& value);
ApiResult<std::vector<Branch>> parseBranches(const juce::var& value);
ApiResult<std::vector<VersionSummary>> parseVersions(const juce::var& value);
// The "missing" list of POST /projects/{id}/blobs/check-missing.
ApiResult<std::vector<juce::String>> parseMissingBlobs(const juce::var& value);

// FastAPI puts the reason in "detail", as a string or as a list of validation errors.
juce::String extractErrorMessage(const juce::var& parsedJson,
                                 const juce::String& responseText,
                                 const juce::String& fallback);
}
