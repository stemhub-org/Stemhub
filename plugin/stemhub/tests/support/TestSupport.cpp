#include "support/TestSupport.hpp"

namespace stemhub::test
{
namespace
{
const char* toString(AuthState state)
{
    switch (state)
    {
        case AuthState::signedOut: return "signedOut";
        case AuthState::signingIn: return "signingIn";
        case AuthState::signedIn: return "signedIn";
        case AuthState::authError: return "authError";
    }

    return "unknown";
}

const char* toString(UIState state)
{
    switch (state)
    {
        case UIState::login: return "login";
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
        case OperationState::loadingProjects: return "loadingProjects";
        case OperationState::committing: return "committing";
        case OperationState::pulling: return "pulling";
        case OperationState::restoring: return "restoring";
    }

    return "unknown";
}

juce::var makeBlobRef(const juce::String& filename, const juce::String& sha)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("sha256", sha);
    object->setProperty("size_bytes", 1);
    object->setProperty("filename", filename);
    object->setProperty("name", filename);
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

VersionSummary makeVersion(const juce::String& id, const juce::String& branchId, const juce::String& commit)
{
    VersionSummary version;
    version.id = id;
    version.branchId = branchId;
    version.commitMessage = commit;
    version.createdAt = "2026-03-18T10:00:00Z";
    return version;
}

juce::String describe(const StemhubSession& session)
{
    const auto& state = session.getState();
    return "auth=" + juce::String(toString(state.authState))
        + ", ui=" + juce::String(toString(state.uiState))
        + ", op=" + juce::String(toString(state.operationState))
        + ", authStatus=" + state.authStatus.text
        + ", projectsStatus=" + state.projectsStatus.text
        + ", sessionStatus=" + state.sessionStatus.text
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
                       const std::vector<std::pair<juce::String, juce::String>>& tracks)
{
    juce::Array<juce::var> trackArray;
    for (const auto& [path, sha] : tracks)
        trackArray.add(makeBlobRef(path, sha));

    auto* manifest = new juce::DynamicObject();
    manifest->setProperty("manifest_version", 1);
    manifest->setProperty("project_file", makeBlobRef(projectPath, projectSha));
    manifest->setProperty("tracks", trackArray);
    return juce::var(manifest);
}
}
