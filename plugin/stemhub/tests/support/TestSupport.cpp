#include "support/TestSupport.hpp"

namespace stemhub::test
{
namespace
{
const char* toString(UIState state)
{
    switch (state)
    {
        case UIState::projectSelection: return "projectSelection";
        case UIState::dashboard: return "dashboard";
    }

    return "unknown";
}

const char* toString(OperationState state)
{
    switch (state)
    {
        case OperationState::idle: return "idle";
        case OperationState::signingIn: return "signingIn";
        case OperationState::loadingProjects: return "loadingProjects";
        case OperationState::saving: return "saving";
        case OperationState::loadingHistory: return "loadingHistory";
        case OperationState::restoring: return "restoring";
    }

    return "unknown";
}

// Manifest v1 named each file's path "filename" and gave assets a "name"; v2 has "path" only.
juce::var makeFileRef(const juce::String& path, const juce::String& sha, const int manifestVersion, const bool isAsset)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("sha256", sha);
    object->setProperty("size_bytes", 1);
    object->setProperty(manifestVersion == 1 ? "filename" : "path", path);
    if (manifestVersion == 1 && isAsset)
        object->setProperty("name", path);
    return juce::var(object);
}
}

Project makeProject(const juce::String& id, const juce::String& name)
{
    Project project;
    project.id = id;
    project.name = name;
    return project;
}

Branch makeBranch(const juce::String& id, const juce::String& projectId, const juce::String& name)
{
    Branch branch;
    branch.id = id;
    branch.projectId = projectId;
    branch.name = name;
    return branch;
}

VersionSummary makeVersion(const juce::String& id, const juce::String& branchId, const juce::String& message)
{
    VersionSummary version;
    version.id = id;
    version.branchId = branchId;
    version.message = message;
    version.createdAt = "2026-03-18T10:00:00Z";
    return version;
}

juce::String describe(const StemhubSession& session)
{
    const auto& state = session.getState();
    return "signedIn=" + juce::String(state.isSignedIn() ? "yes" : "no")
        + ", ui=" + juce::String(toString(state.uiState))
        + ", op=" + juce::String(toString(state.operationState))
        + ", authStatus=" + state.authStatus.text
        + ", projectsStatus=" + state.projectsStatus.text
        + ", dashboardStatus=" + state.dashboardStatus.text
        + ", selectedProject=" + (state.selectedProject.has_value() ? state.selectedProject->id : "<none>")
        + ", selectedBranch=" + state.selectedBranchId
        + ", selectedVersion=" + state.selectedVersionId;
}

bool waitUntil(StemhubSession& session, const std::function<bool()>& predicate, int timeoutMs)
{
    const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32>(timeoutMs);
    while (juce::Time::getMillisecondCounter() < deadline)
    {
        session.flushPendingResultsForTesting();
        if (predicate())
            return true;
        juce::Thread::sleep(5);
    }

    session.flushPendingResultsForTesting();
    return predicate();
}

bool waitForResults(StemhubSession& session, int count, int timeoutMs)
{
    int received = 0;
    const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32>(timeoutMs);
    while (received < count && juce::Time::getMillisecondCounter() < deadline)
    {
        received += session.flushPendingResultsForTesting();
        juce::Thread::sleep(5);
    }

    return received == count;
}

void simulateDawSave(const juce::File& projectFile, const juce::String& extraContent)
{
    const auto previousModTime = projectFile.getLastModificationTime();
    projectFile.appendText(extraContent);
    projectFile.setLastModificationTime(previousModTime + juce::RelativeTime::seconds(2));
}

juce::var makeManifest(const juce::String& projectPath,
                       const juce::String& projectSha,
                       const std::vector<std::pair<juce::String, juce::String>>& assets,
                       const int manifestVersion)
{
    juce::Array<juce::var> assetArray;
    for (const auto& [path, sha] : assets)
        assetArray.add(makeFileRef(path, sha, manifestVersion, true));

    auto* manifest = new juce::DynamicObject();
    manifest->setProperty("manifest_version", manifestVersion);
    manifest->setProperty("project_file", makeFileRef(projectPath, projectSha, manifestVersion, false));
    manifest->setProperty(manifestVersion == 1 ? "tracks" : "assets", assetArray);
    return juce::var(manifest);
}
}
