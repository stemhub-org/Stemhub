#include <map>
#include <memory>
#include <utility>

#include <JuceHeader.h>

#include "application/PluginProcessor.hpp"
#include "application/SessionCache.hpp"

namespace
{
ApiResult<LoginResponse> makeLoginResult(const juce::String& accessToken)
{
    LoginResponse response;
    response.accessToken = accessToken;
    response.tokenType = "bearer";
    return { response, {} };
}

ApiResult<User> makeUserResult()
{
    User user;
    user.id = "user-1";
    user.email = "user@example.com";
    user.username = "stemhub";
    return { user, {} };
}

ApiResult<std::vector<Project>> makeProjectsResult(std::vector<Project> projects)
{
    return { std::move(projects), {} };
}

ApiResult<std::vector<Branch>> makeBranchesResult(std::vector<Branch> branches)
{
    return { std::move(branches), {} };
}

ApiResult<juce::var> makeVersionsResult(const std::vector<VersionSummary>& versions)
{
    juce::Array<juce::var> versionArray;
    for (const auto& version : versions)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty("id", version.id);
        object->setProperty("branch_id", version.branchId);
        object->setProperty("parent_version_id", version.parentVersionId);
        object->setProperty("created_at", version.createdAt);
        object->setProperty("commit_message", version.commitMessage);
        object->setProperty("source_daw", version.sourceDaw);
        object->setProperty("source_project_filename", version.sourceProjectFilename);
        object->setProperty("artifact_path", version.artifactPath);
        object->setProperty("artifact_checksum", version.artifactChecksum);
        object->setProperty("artifact_size_bytes", version.artifactSizeBytes);
        versionArray.add(juce::var(object));
    }

    return { juce::var(versionArray), {} };
}

ApiResult<juce::var> makeApiError(const juce::String& message, int statusCode = 500)
{
    return { {}, ApiError { statusCode, message } };
}

ApiResult<std::vector<Branch>> makeBranchError(const juce::String& message, int statusCode = 500)
{
    return { {}, ApiError { statusCode, message } };
}

ApiResult<User> makeUserError(const juce::String& message, int statusCode = 401)
{
    return { {}, ApiError { statusCode, message } };
}

class BlockingGate
{
public:
    void waitUntilEntered()
    {
        expectEntered.wait(2000);
    }

    void release()
    {
        allowContinue.signal();
    }

    void waitUntilFinished()
    {
        finished.wait(2000);
    }

    void block()
    {
        expectEntered.signal();
        allowContinue.wait(2000);
        finished.signal();
    }

private:
    juce::WaitableEvent expectEntered;
    juce::WaitableEvent allowContinue;
    juce::WaitableEvent finished;
};

class FakeProjectApi final : public IProjectApi
{
public:
    ApiResult<LoginResponse> login(const juce::String& email, const juce::String& password) const override
    {
        juce::ignoreUnused(email, password);
        return makeLoginResult("token");
    }

    ApiResult<User> fetchCurrentUser(const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);
        if (!cachedSessionIsValid)
            return makeUserError("expired");

        return makeUserResult();
    }

    ApiResult<std::vector<Project>> fetchProjects(const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);
        return makeProjectsResult(projects);
    }

    ApiResult<Project> createProject(const juce::String& name, const juce::String& accessToken) const override
    {
        juce::ignoreUnused(name, accessToken);
        return { {}, ApiError { 500, "not implemented in tests" } };
    }

    ApiResult<std::vector<Branch>> fetchBranches(const juce::String& projectId, const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);

        if (auto it = branchFetchGates.find(projectId); it != branchFetchGates.end() && it->second != nullptr)
            it->second->block();

        if (auto it = branchErrors.find(projectId); it != branchErrors.end())
            return makeBranchError(it->second);

        if (auto it = projectBranches.find(projectId); it != projectBranches.end())
            return makeBranchesResult(it->second);

        return makeBranchError("No workspaces found for this project.");
    }

    ApiResult<juce::var> requestJson(const juce::String& path,
                                     const juce::String& httpMethod,
                                     const juce::String& requestBody,
                                     const juce::String& bearerToken) const override
    {
        juce::ignoreUnused(requestBody, bearerToken);

        if (httpMethod == "GET" && path.startsWith("/branches/") && path.endsWith("/versions/"))
        {
            auto branchId = path.fromFirstOccurrenceOf("/branches/", false, false)
                .upToLastOccurrenceOf("/versions/", false, false);

            if (auto it = versionFetchGates.find(branchId); it != versionFetchGates.end() && it->second != nullptr)
                it->second->block();

            if (auto it = versionErrors.find(branchId); it != versionErrors.end())
                return makeApiError(it->second);

            if (auto it = branchVersions.find(branchId); it != branchVersions.end())
                return makeVersionsResult(it->second);

            return makeVersionsResult({});
        }

        return makeApiError("Unhandled request in test API.");
    }

    ApiResult<juce::var> uploadFile(const juce::String& path,
                                    const juce::File& file,
                                    const juce::String& formFieldName,
                                    const juce::String& bearerToken) const override
    {
        juce::ignoreUnused(path, file, formFieldName, bearerToken);
        return makeApiError("upload not implemented in tests");
    }

    juce::Result downloadFile(const juce::String& path,
                              const juce::File& destinationFile,
                              const juce::String& bearerToken) const override
    {
        juce::ignoreUnused(path, destinationFile, bearerToken);
        return juce::Result::fail("download not implemented in tests");
    }

    ApiResult<std::vector<juce::String>> checkMissingBlobs(const juce::String& projectId,
                                                            const std::vector<juce::String>& sha256s,
                                                            const juce::String& accessToken) const override
    {
        juce::ignoreUnused(projectId, sha256s, accessToken);
        return { std::vector<juce::String>{}, {} };
    }

    ApiResult<juce::var> uploadBlob(const juce::String& projectId,
                                     const juce::String& sha256,
                                     const juce::File& file,
                                     const juce::String& accessToken) const override
    {
        juce::ignoreUnused(projectId, sha256, file, accessToken);
        return makeApiError("uploadBlob not implemented in tests");
    }

    ApiResult<juce::var> createVersionFromManifest(const juce::String& branchId,
                                                    const juce::var& payload,
                                                    const juce::String& accessToken) const override
    {
        juce::ignoreUnused(branchId, payload, accessToken);
        return makeApiError("createVersionFromManifest not implemented in tests");
    }

    juce::Result downloadBlob(const juce::String& projectId,
                               const juce::String& sha256,
                               const juce::File& destinationFile,
                               const juce::String& accessToken) const override
    {
        juce::ignoreUnused(projectId, sha256, destinationFile, accessToken);
        return juce::Result::fail("downloadBlob not implemented in tests");
    }

    bool cachedSessionIsValid { true };
    std::vector<Project> projects;
    std::map<juce::String, std::vector<Branch>> projectBranches;
    std::map<juce::String, std::vector<VersionSummary>> branchVersions;
    std::map<juce::String, juce::String> branchErrors;
    std::map<juce::String, juce::String> versionErrors;
    std::map<juce::String, std::shared_ptr<BlockingGate>> branchFetchGates;
    std::map<juce::String, std::shared_ptr<BlockingGate>> versionFetchGates;
};

Project makeProject(const juce::String& id, const juce::String& name)
{
    Project project;
    project.id = id;
    project.ownerId = "user-1";
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
        case OperationState::error: return "error";
    }

    return "unknown";
}

juce::String describeProcessorState(const StemhubAudioProcessor& processor)
{
    return "auth=" + juce::String(toString(processor.getAuthState()))
        + ", ui=" + juce::String(toString(processor.getUIState()))
        + ", op=" + juce::String(toString(processor.getOperationState()))
        + ", authError=" + processor.getAuthErrorMessage()
        + ", projectMessage=" + processor.getProjectSelectionStatusMessage()
        + ", activeMessage=" + processor.getActiveProjectStatusMessage()
        + ", selectedProject=" + (processor.getSelectedProject().has_value() ? processor.getSelectedProject()->id : "<none>")
        + ", selectedBranch=" + processor.getSelectedBranchId();
}

bool waitUntil(StemhubAudioProcessor& processor, const std::function<bool()>& predicate, int timeoutMs = 3000)
{
    const auto deadline = juce::Time::getMillisecondCounter() + static_cast<uint32>(timeoutMs);
    while (juce::Time::getMillisecondCounter() < deadline)
    {
        processor.flushPendingBackgroundResultsForTesting();
        if (predicate())
            return true;
        juce::Thread::sleep(5);
    }

    processor.flushPendingBackgroundResultsForTesting();
    return predicate();
}

class ProcessorChangeWatcher : private juce::ChangeListener
{
public:
    explicit ProcessorChangeWatcher(StemhubAudioProcessor& processor)
        : processorRef(processor)
    {
        processorRef.addChangeListener(this);
    }

    ~ProcessorChangeWatcher() override
    {
        processorRef.removeChangeListener(this);
    }

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override
    {
        juce::ignoreUnused(source);
    }

    StemhubAudioProcessor& processorRef;
};

class ProcessorStabilityTests final : public juce::UnitTest
{
public:
    ProcessorStabilityTests()
        : juce::UnitTest("Stemhub plugin processor stability", "plugin")
    {
    }

    void runTest() override
    {
        beginTest("Invalid cached session clears cache and returns to login");
        {
            TestContext context;
            context.api->cachedSessionIsValid = false;
            stemhub::sessioncache::saveAccessToken("expired-token");
            expect(stemhub::sessioncache::loadAccessToken() == "expired-token", "test cache override should persist saved token");

            context.processor.requestRestoreCachedSession();

            const auto didReachAuthError = waitUntil(context.processor, [&context]
            {
                return context.processor.getAuthState() == AuthState::authError;
            });
            expect(didReachAuthError, "cached invalid token should end in authError: " + describeProcessorState(context.processor));
            expect(context.processor.getUIState() == UIState::login, "cached invalid token should leave login UI visible");
            expect(context.processor.getAuthErrorMessage().containsIgnoreCase("sign in again"),
                   "cached invalid token should surface re-auth message: " + describeProcessorState(context.processor));
            expect(stemhub::sessioncache::loadAccessToken().isEmpty(), "cached invalid token should clear saved token");
        }

        beginTest("Missing cached project falls back to project selection and clears stale context");
        {
            TestContext context;
            context.api->projects = { makeProject("project-2", "Project Two") };
            stemhub::sessioncache::saveAccessToken("valid-token");
            stemhub::sessioncache::saveProjectId("missing-project");
            stemhub::sessioncache::saveLastOpenedProjectFilePath("/tmp/missing.flp");
            expect(stemhub::sessioncache::loadAccessToken() == "valid-token", "test cache override should persist saved token");

            context.processor.requestRestoreCachedSession();

            const auto didReturnToProjectSelection = waitUntil(context.processor, [&context]
            {
                return context.processor.getAuthState() == AuthState::signedIn
                    && context.processor.getUIState() == UIState::projectSelection
                    && context.processor.getProjectSelectionStatusMessage().containsIgnoreCase("no longer available");
            });
            expect(didReturnToProjectSelection,
                   "missing cached project should fall back to project selection: " + describeProcessorState(context.processor));
            expect(!context.processor.getSelectedProject().has_value(), "missing cached project should not select a project");
            expect(stemhub::sessioncache::loadProjectId().isEmpty(), "missing cached project should clear cached project id");
            expect(stemhub::sessioncache::loadLastOpenedProjectFilePath().isEmpty(), "missing cached project should clear cached file path");
        }

        beginTest("Stale project activation results are ignored");
        {
            TestContext context;
            auto projectA = makeProject("project-a", "Project A");
            auto projectB = makeProject("project-b", "Project B");
            auto branchA = makeBranch("branch-a", projectA.id, "main");
            auto branchB = makeBranch("branch-b", projectB.id, "main");
            context.api->projects = { projectA, projectB };
            context.api->projectBranches[projectA.id] = { branchA };
            context.api->projectBranches[projectB.id] = { branchB };
            context.api->branchVersions[branchA.id] = { makeVersion("aaaaaaaa-0000", branchA.id, "A") };
            context.api->branchVersions[branchB.id] = { makeVersion("bbbbbbbb-0000", branchB.id, "B") };
            stemhub::sessioncache::saveAccessToken("valid-token");
            expect(stemhub::sessioncache::loadAccessToken() == "valid-token", "test cache override should persist saved token");
            context.processor.requestRestoreCachedSession();
            const auto didRestoreSession = waitUntil(context.processor, [&context] { return context.processor.getAuthState() == AuthState::signedIn; });
            expect(didRestoreSession,
                   "processor should restore valid cached session before open-project race test: " + describeProcessorState(context.processor));

            auto gate = std::make_shared<BlockingGate>();
            context.api->branchFetchGates[projectA.id] = gate;

            context.processor.requestOpenProject(projectA.id, {}, false);
            gate->waitUntilEntered();

            context.processor.requestOpenProject(projectB.id, {}, false);
            expect(waitUntil(context.processor, [&context, &projectB, &branchB]
            {
                return context.processor.getSelectedProject().has_value()
                    && context.processor.getSelectedProject()->id == projectB.id
                    && context.processor.getSelectedBranchId() == branchB.id;
            }), "latest open-project request should win");

            gate->release();
            expect(waitUntil(context.processor, [&context, &projectB, &branchB]
            {
                return context.processor.getSelectedProject().has_value()
                    && context.processor.getSelectedProject()->id == projectB.id
                    && context.processor.getSelectedBranchId() == branchB.id
                    && context.processor.getVersionHistory().size() == 1
                    && context.processor.getVersionHistory().front().branchId == branchB.id;
            }), "stale open-project result should not overwrite winning selection");
            gate->waitUntilFinished();
        }

        beginTest("Stale branch history results are ignored");
        {
            TestContext context;
            auto project = makeProject("project-1", "Project");
            auto branchMain = makeBranch("branch-main", project.id, "main");
            auto branchAlt = makeBranch("branch-alt", project.id, "alt");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branchMain, branchAlt };
            context.api->branchVersions[branchMain.id] = { makeVersion("11111111-0000", branchMain.id, "main") };
            context.api->branchVersions[branchAlt.id] = { makeVersion("22222222-0000", branchAlt.id, "alt") };
            stemhub::sessioncache::saveAccessToken("valid-token");
            expect(stemhub::sessioncache::loadAccessToken() == "valid-token", "test cache override should persist saved token");
            context.processor.requestRestoreCachedSession();
            const auto didRestoreSession = waitUntil(context.processor, [&context] { return context.processor.getAuthState() == AuthState::signedIn; });
            expect(didRestoreSession,
                   "processor should restore valid cached session before branch race test: " + describeProcessorState(context.processor));
            context.processor.requestOpenProject(project.id, {}, false);
            expect(waitUntil(context.processor, [&context, &branchMain]
            {
                return context.processor.getSelectedProject().has_value()
                    && context.processor.getSelectedBranchId() == branchMain.id;
            }), "project open should select main branch before branch race test");

            auto gate = std::make_shared<BlockingGate>();
            context.api->versionFetchGates[branchMain.id] = gate;

            context.processor.requestSelectBranch(branchMain.id);
            gate->waitUntilEntered();

            context.processor.requestSelectBranch(branchAlt.id);
            expect(waitUntil(context.processor, [&context, &branchAlt]
            {
                return context.processor.getSelectedBranchId() == branchAlt.id
                    && context.processor.getVersionHistory().size() == 1
                    && context.processor.getVersionHistory().front().branchId == branchAlt.id;
            }), "latest branch selection should win");

            gate->release();
            expect(waitUntil(context.processor, [&context, &branchAlt]
            {
                return context.processor.getSelectedBranchId() == branchAlt.id
                    && context.processor.getVersionHistory().front().branchId == branchAlt.id;
            }), "stale branch history should not overwrite winning branch");
            gate->waitUntilFinished();
        }

        beginTest("Cached project context failure clears stale selection and stays recoverable");
        {
            TestContext context;
            auto project = makeProject("project-1", "Project");
            context.api->projects = { project };
            context.api->branchErrors[project.id] = "Failed to load workspaces.";
            stemhub::sessioncache::saveAccessToken("valid-token");
            stemhub::sessioncache::saveProjectId(project.id);
            expect(stemhub::sessioncache::loadAccessToken() == "valid-token", "test cache override should persist saved token");

            context.processor.requestRestoreCachedSession();

            const auto didStayRecoverable = waitUntil(context.processor, [&context]
            {
                return context.processor.getAuthState() == AuthState::signedIn
                    && context.processor.getUIState() == UIState::projectSelection
                    && context.processor.getOperationState() == OperationState::error;
            });
            expect(didStayRecoverable,
                   "cached project restore failure should remain signed in and recoverable: " + describeProcessorState(context.processor));
            expect(!context.processor.getSelectedProject().has_value(), "cached project restore failure should not leave a selected project");
            expect(stemhub::sessioncache::loadProjectId().isEmpty(), "cached project restore failure should clear stale cached project id");
            expect(context.processor.getProjectSelectionStatusMessage().containsIgnoreCase("Failed to load workspaces"),
                   "cached project restore failure should surface workspace-load error: " + describeProcessorState(context.processor));
        }
    }

private:
    struct TestContext
    {
        TestContext()
            : tempRoot(juce::File::getSpecialLocation(juce::File::tempDirectory)
                           .getChildFile("stemhub-plugin-tests")
                           .getChildFile(juce::Uuid().toString())),
              cacheFile(tempRoot.getChildFile("session.json")),
              apiOwned(std::make_unique<FakeProjectApi>()),
              api(apiOwned.get()),
              processor(std::move(apiOwned)),
              watcher(processor)
        {
            tempRoot.getParentDirectory().createDirectory();
            tempRoot.createDirectory();
            stemhub::sessioncache::setCacheFileOverrideForTesting(cacheFile);
            stemhub::sessioncache::clear();
        }

        ~TestContext()
        {
            stemhub::sessioncache::clear();
            stemhub::sessioncache::clearCacheFileOverrideForTesting();
            tempRoot.deleteRecursively();
        }

        juce::File tempRoot;
        juce::File cacheFile;
        std::unique_ptr<FakeProjectApi> apiOwned;
        FakeProjectApi* api;
        StemhubAudioProcessor processor;
        ProcessorChangeWatcher watcher;
    };
};

ProcessorStabilityTests processorStabilityTests;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI scopedJuce;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.runAllTests();
    int failureCount = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (auto* result = runner.getResult(i); result != nullptr)
        {
            failureCount += result->failures;
            for (const auto& message : result->messages)
                juce::Logger::writeToLog(message);
        }
    }

    return failureCount == 0 ? 0 : 1;
}
