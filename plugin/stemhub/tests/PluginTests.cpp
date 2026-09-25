#include <atomic>
#include <csignal>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <JuceHeader.h>

#include "application/StemhubSession.hpp"
#include "application/SessionCache.hpp"
#include "application/SnapshotBundler.hpp"
#include "network/ApiConfig.hpp"
#include "network/ApiJson.hpp"

namespace
{
template <typename T>
ApiResult<T> fail(int statusCode, const juce::String& message)
{
    return ApiResult<T>::failure(ApiError::fromStatus(statusCode, message));
}

juce::String sha256Of(const juce::MemoryBlock& data)
{
    return juce::SHA256(data.getData(), data.getSize()).toHexString();
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

// In-memory StemHub backend. Worker threads call it while the test thread inspects it,
// so all state is behind one mutex; gates are always waited on outside of it.
class FakeProjectApi final : public IProjectApi
{
public:
    struct CreatedVersion
    {
        juce::String id;
        juce::String branchId;
        juce::String parentVersionId;
        juce::String commitMessage;
    };

    ApiResult<LoginResponse> login(const juce::String& email, const juce::String& password) const override
    {
        juce::ignoreUnused(email, password);
        return ApiResult<LoginResponse>::success({ "token" });
    }

    ApiResult<User> fetchCurrentUser(const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);
        if (offline)
            return ApiResult<User>::failure({ ApiError::Kind::network, 0, "Can't reach StemHub." });
        if (rejectToken || !cachedSessionIsValid)
            return fail<User>(401, "Could not validate credentials");

        return ApiResult<User>::success({ "user-1", "user@example.com", "stemhub" });
    }

    ApiResult<std::vector<Project>> fetchProjects(const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);
        if (rejectToken)
            return fail<std::vector<Project>>(401, "Could not validate credentials");

        const std::lock_guard<std::mutex> lock(mutex);
        return ApiResult<std::vector<Project>>::success(projects);
    }

    ApiResult<Project> createProject(const juce::String& name, const juce::String& accessToken) const override
    {
        juce::ignoreUnused(name, accessToken);
        return fail<Project>(500, "not implemented in tests");
    }

    ApiResult<std::vector<Branch>> fetchBranches(const juce::String& projectId, const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);

        if (auto gate = findGate(branchFetchGates, projectId))
        {
            gate->block();
            if (branchFetchReturned != nullptr)
                *branchFetchReturned = true;
        }

        if (rejectToken)
            return fail<std::vector<Branch>>(401, "Could not validate credentials");

        const std::lock_guard<std::mutex> lock(mutex);
        if (auto it = branchErrors.find(projectId); it != branchErrors.end())
            return fail<std::vector<Branch>>(500, it->second);

        if (auto it = projectBranches.find(projectId); it != projectBranches.end())
            return ApiResult<std::vector<Branch>>::success(it->second);

        return ApiResult<std::vector<Branch>>::success({});
    }

    ApiResult<std::vector<VersionSummary>> fetchVersions(const juce::String& branchId,
                                                         const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);

        if (auto gate = findGate(versionFetchGates, branchId))
            gate->block();

        if (rejectToken)
            return fail<std::vector<VersionSummary>>(401, "Could not validate credentials");

        const std::lock_guard<std::mutex> lock(mutex);
        if (auto it = versionErrors.find(branchId); it != versionErrors.end())
            return fail<std::vector<VersionSummary>>(500, it->second);

        if (auto it = branchVersions.find(branchId); it != branchVersions.end())
            return ApiResult<std::vector<VersionSummary>>::success(it->second);

        return ApiResult<std::vector<VersionSummary>>::success({});
    }

    ApiResult<juce::var> fetchVersionManifest(const juce::String& versionId, const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);
        if (rejectToken)
            return fail<juce::var>(401, "Could not validate credentials");

        const std::lock_guard<std::mutex> lock(mutex);
        const auto manifest = manifestsByVersion.find(versionId);
        if (manifest == manifestsByVersion.end())
            return fail<juce::var>(404, "Version not found.");

        return ApiResult<juce::var>::success(juce::JSON::parse(manifest->second));
    }

    ApiResult<std::vector<juce::String>> checkMissingBlobs(const juce::String& projectId,
                                                            const std::vector<juce::String>& sha256s,
                                                            const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);

        if (auto gate = findGate(checkMissingGates, projectId))
            gate->block();

        if (rejectToken)
            return fail<std::vector<juce::String>>(401, "Could not validate credentials");

        const std::lock_guard<std::mutex> lock(mutex);
        std::vector<juce::String> missing;
        for (const auto& sha : sha256s)
            if (blobs.find(sha) == blobs.end())
                missing.push_back(sha);

        return ApiResult<std::vector<juce::String>>::success(missing);
    }

    ApiResult<Unit> uploadBlob(const juce::String& projectId,
                               const juce::String& sha256,
                               const juce::File& file,
                               const juce::String& accessToken) const override
    {
        juce::ignoreUnused(projectId, accessToken);

        juce::MemoryBlock data;
        if (!file.loadFileAsData(data))
            return ApiResult<Unit>::failure({ ApiError::Kind::localFile, 0, "Blob source file does not exist." });

        if (sha256Of(data) != sha256)
            return fail<Unit>(400, "SHA-256 mismatch.");

        const std::lock_guard<std::mutex> lock(mutex);
        blobs[sha256] = data;
        ++uploadsBySha[sha256];
        return ApiResult<Unit>::success({});
    }

    ApiResult<VersionSummary> createVersionFromManifest(const juce::String& branchId,
                                                        const CreateVersionRequest& request,
                                                        const juce::String& accessToken) const override
    {
        juce::ignoreUnused(accessToken);

        const std::lock_guard<std::mutex> lock(mutex);
        const auto number = static_cast<int>(createdVersions.size()) + 1;

        VersionSummary version;
        version.id = juce::String::toHexString(number).paddedLeft('0', 8) + "-0000-4000-8000-000000000000";
        version.branchId = branchId;
        version.parentVersionId = request.parentVersionId;
        version.commitMessage = request.commitMessage;
        version.createdAt = "2026-03-19T10:00:" + juce::String(number).paddedLeft('0', 2) + "Z";
        version.sourceProjectFilename = request.manifest.getProperty("source_project_filename", {}).toString();

        createdVersions.push_back({ version.id, branchId, version.parentVersionId, version.commitMessage });
        manifestsByVersion[version.id] = juce::JSON::toString(request.manifest);
        branchVersions[branchId].push_back(version);
        return ApiResult<VersionSummary>::success(version);
    }

    ApiResult<Unit> downloadBlob(const juce::String& projectId,
                                 const juce::String& sha256,
                                 const juce::File& destinationFile,
                                 const juce::String& accessToken) const override
    {
        juce::ignoreUnused(projectId, accessToken);

        juce::MemoryBlock data;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            const auto blob = blobs.find(sha256);
            if (blob == blobs.end())
                return fail<Unit>(404, "Blob not found");

            data = blob->second;
            if (corruptDownloads)
                data.append("!", 1);
        }

        if (!destinationFile.replaceWithData(data.getData(), data.getSize()))
            return ApiResult<Unit>::failure({ ApiError::Kind::localFile, 0, "Could not write " + destinationFile.getFullPathName() });

        return ApiResult<Unit>::success({});
    }

    // Stores a version made of (relative path, content) files, the first being the project
    // file, as if another collaborator had saved it. Returns its id.
    juce::String addVersion(const juce::String& branchId,
                            const juce::String& commitMessage,
                            const std::vector<std::pair<juce::String, juce::String>>& files)
    {
        juce::Array<juce::var> tracks;
        juce::var projectFileRef;

        for (size_t index = 0; index < files.size(); ++index)
        {
            const auto& [path, content] = files[index];
            const juce::MemoryBlock data(content.toRawUTF8(), content.getNumBytesAsUTF8());
            const auto sha = sha256Of(data);
            {
                const std::lock_guard<std::mutex> lock(mutex);
                blobs[sha] = data;
            }

            auto* ref = new juce::DynamicObject();
            ref->setProperty("sha256", sha);
            ref->setProperty("size_bytes", static_cast<juce::int64>(data.getSize()));
            ref->setProperty("filename", path);

            if (index == 0)
            {
                projectFileRef = juce::var(ref);
            }
            else
            {
                ref->setProperty("name", path);
                tracks.add(juce::var(ref));
            }
        }

        auto* manifest = new juce::DynamicObject();
        manifest->setProperty("manifest_version", 1);
        manifest->setProperty("source_project_filename", files.front().first.fromLastOccurrenceOf("/", false, false));
        manifest->setProperty("project_file", projectFileRef);
        manifest->setProperty("tracks", tracks);

        CreateVersionRequest request;
        request.commitMessage = commitMessage;
        request.manifest = juce::var(manifest);
        return createVersionFromManifest(branchId, request, "token").value->id;
    }

    std::vector<CreatedVersion> getCreatedVersions() const
    {
        const std::lock_guard<std::mutex> lock(mutex);
        return createdVersions;
    }

    int getUploadCount(const juce::String& sha) const
    {
        const std::lock_guard<std::mutex> lock(mutex);
        const auto count = uploadsBySha.find(sha);
        return count != uploadsBySha.end() ? count->second : 0;
    }

    void setCheckMissingGate(const juce::String& projectId, std::shared_ptr<BlockingGate> gate)
    {
        const std::lock_guard<std::mutex> lock(mutex);
        checkMissingGates[projectId] = std::move(gate);
    }

    // Configuration: written by the test thread while no job is running.
    bool cachedSessionIsValid { true };
    std::vector<Project> projects;
    std::map<juce::String, std::vector<Branch>> projectBranches;
    // Also appended to by createVersionFromManifest, under the mutex.
    mutable std::map<juce::String, std::vector<VersionSummary>> branchVersions;
    std::map<juce::String, juce::String> branchErrors;
    std::map<juce::String, juce::String> versionErrors;
    std::map<juce::String, std::shared_ptr<BlockingGate>> branchFetchGates;
    std::map<juce::String, std::shared_ptr<BlockingGate>> versionFetchGates;
    std::shared_ptr<std::atomic<bool>> branchFetchReturned;
    std::atomic<bool> rejectToken { false };     // every call answers 401
    std::atomic<bool> offline { false };         // fetchCurrentUser gets no response
    std::atomic<bool> corruptDownloads { false }; // downloaded blobs don't match their hash

private:
    std::shared_ptr<BlockingGate> findGate(const std::map<juce::String, std::shared_ptr<BlockingGate>>& gates,
                                           const juce::String& key) const
    {
        const std::lock_guard<std::mutex> lock(mutex);
        const auto it = gates.find(key);
        return it != gates.end() ? it->second : nullptr;
    }

    mutable std::mutex mutex;
    mutable std::map<juce::String, juce::MemoryBlock> blobs;
    mutable std::map<juce::String, int> uploadsBySha;
    mutable std::map<juce::String, juce::String> manifestsByVersion;
    mutable std::vector<CreatedVersion> createdVersions;
    std::map<juce::String, std::shared_ptr<BlockingGate>> checkMissingGates;
};

// Serves one canned response per request on 127.0.0.1 and records what it received.
class LocalHttpServer final : private juce::Thread
{
public:
    struct Request
    {
        juce::String requestLine;
        juce::String headers;
    };

    using Handler = std::function<juce::String(const Request&)>;

    explicit LocalHttpServer(Handler handlerToUse)
        : juce::Thread("Test HTTP server"), handler(std::move(handlerToUse))
    {
        if (listener.createListener(0, "127.0.0.1"))
            startThread();
    }

    ~LocalHttpServer() override
    {
        signalThreadShouldExit();
        listener.close();
        stopThread(2000);
    }

    juce::String getBaseUrl() const
    {
        return "http://127.0.0.1:" + juce::String(listener.getBoundPort());
    }

    std::vector<Request> getRequests() const
    {
        const std::lock_guard<std::mutex> lock(requestsMutex);
        return requests;
    }

private:
    void run() override
    {
        while (!threadShouldExit())
        {
            std::unique_ptr<juce::StreamingSocket> connection(listener.waitForNextConnection());
            // Closing the listener connects to it once to wake this thread up.
            if (connection == nullptr || threadShouldExit())
                return;

            juce::MemoryOutputStream received;
            char buffer[1024];
            while (!received.toString().contains("\r\n\r\n") && connection->waitUntilReady(true, 2000) == 1)
            {
                const auto bytesRead = connection->read(buffer, sizeof(buffer), false);
                if (bytesRead <= 0)
                    break;
                received.write(buffer, static_cast<size_t>(bytesRead));
            }

            const auto text = received.toString();
            if (text.isEmpty())
                continue;

            Request request { text.upToFirstOccurrenceOf("\r\n", false, false),
                              text.fromFirstOccurrenceOf("\r\n", false, false) };
            {
                const std::lock_guard<std::mutex> lock(requestsMutex);
                requests.push_back(request);
            }

            const auto response = handler(request);
            connection->write(response.toRawUTF8(), static_cast<int>(response.getNumBytesAsUTF8()));
        }
    }

    Handler handler;
    juce::StreamingSocket listener;
    mutable std::mutex requestsMutex;
    std::vector<Request> requests;
};

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

bool waitUntil(StemhubSession& session, const std::function<bool()>& predicate, int timeoutMs = 3000)
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

// Waits for the running jobs to hand back this many results, whether applied or dropped.
bool waitForResults(StemhubSession& session, int count, int timeoutMs = 3000)
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

// Appends to a file and moves its modification time forward, like a DAW saving the project.
void simulateDawSave(const juce::File& projectFile, const juce::String& extraContent)
{
    const auto previousModTime = projectFile.getLastModificationTime();
    projectFile.appendText(extraContent);
    projectFile.setLastModificationTime(previousModTime + juce::RelativeTime::seconds(2));
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

// Everything on disk a test touches, removed afterwards.
struct TestEnvironment
{
    TestEnvironment()
        : root(juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("stemhub-plugin-tests")
                   .getChildFile(juce::Uuid().toString()))
    {
        root.createDirectory();
        stemhub::sessioncache::setCacheFileOverrideForTesting(root.getChildFile("session.json"));
        stemhub::sessioncache::clear();
    }

    ~TestEnvironment()
    {
        stemhub::sessioncache::clear();
        stemhub::sessioncache::clearCacheFileOverrideForTesting();
        root.deleteRecursively();
    }

    juce::File root;
};

// A session on the fake backend. Files it would open in the DAW are recorded instead, and the
// latest versions it restores go under the test folder.
struct TestContext
{
    TestContext()
    {
        session.setOpenFileHandler([this](const juce::File& file)
        {
            openedFiles.add(file);
            return canOpenFiles;
        });
        session.setManagedWorkingCopyFolder(environment.root.getChildFile("managed"));
    }

    [[nodiscard]] const SessionState& state() const noexcept { return session.getState(); }
    [[nodiscard]] bool isIdle() const noexcept { return !session.isBusy(); }

    // Declared first, so it is cleaned up after the session and the jobs that use its files.
    TestEnvironment environment;
    std::shared_ptr<FakeProjectApi> api { std::make_shared<FakeProjectApi>() };
    juce::Array<juce::File> openedFiles;
    bool canOpenFiles { true };
    StemhubSession session { api };
};

class StemhubTest : public juce::UnitTest
{
protected:
    explicit StemhubTest(const juce::String& testName)
        : juce::UnitTest(testName, "plugin")
    {
    }

    void signIn(TestContext& context)
    {
        context.session.requestSignIn("user@example.com", "secret");
        expect(waitUntil(context.session, [&context] { return context.state().authState == AuthState::signedIn && context.isIdle(); }),
               "sign-in should succeed: " + describe(context.session));
    }

    void openProject(TestContext& context, const juce::String& projectId, const juce::File& projectFile)
    {
        context.session.requestOpenProject(projectId, projectFile);
        expect(waitUntil(context.session, [&context, projectId]
        {
            return context.state().selectedProject.has_value() && context.state().selectedProject->id == projectId
                && context.isIdle();
        }), "project should open: " + describe(context.session));
    }
};

class SessionTests final : public StemhubTest
{
public:
    SessionTests() : StemhubTest("Stemhub session") {}

    void runTest() override
    {
        beginTest("An invalid saved session is forgotten and the login screen stays");
        {
            TestContext context;
            context.api->cachedSessionIsValid = false;
            stemhub::sessioncache::saveAccessToken("expired-token");

            context.session.requestRestoreCachedSession();
            expect(waitUntil(context.session, [&context] { return context.state().authState == AuthState::authError; }),
                   "an invalid saved token should end in authError: " + describe(context.session));
            expect(context.state().uiState == UIState::login, "the login screen stays visible");
            expect(context.state().authStatus.text.containsIgnoreCase("sign in again"), describe(context.session));
            expect(stemhub::sessioncache::loadAccessToken().isEmpty(), "the invalid token is forgotten");
        }

        beginTest("A saved project that no longer exists falls back to the project grid");
        {
            TestContext context;
            context.api->projects = { makeProject("project-2", "Project Two") };
            stemhub::sessioncache::saveAccessToken("valid-token");
            stemhub::sessioncache::saveProjectId("missing-project");
            stemhub::sessioncache::saveLastOpenedProjectFilePath("/tmp/missing.flp");

            context.session.requestRestoreCachedSession();
            expect(waitUntil(context.session, [&context]
            {
                return context.state().authState == AuthState::signedIn
                    && context.state().uiState == UIState::projectSelection
                    && context.state().projectsStatus.text.containsIgnoreCase("no longer available");
            }), "a missing saved project should fall back to the grid: " + describe(context.session));
            expect(context.state().projectsStatus.severity == Status::Severity::warning, "reported as a warning");
            expect(!context.state().selectedProject.has_value(), "no project is selected");
            expect(stemhub::sessioncache::loadProjectId().isEmpty(), "the saved project id is cleared");
            expect(stemhub::sessioncache::loadLastOpenedProjectFilePath().isEmpty(), "the saved file path is cleared");
        }

        beginTest("A saved project that fails to open leaves the session usable");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Project");
            context.api->projects = { project };
            context.api->branchErrors[project.id] = "Failed to load workspaces.";
            stemhub::sessioncache::saveAccessToken("valid-token");
            stemhub::sessioncache::saveProjectId(project.id);

            context.session.requestRestoreCachedSession();
            expect(waitUntil(context.session, [&context]
            {
                return context.state().authState == AuthState::signedIn
                    && context.state().uiState == UIState::projectSelection
                    && context.isIdle()
                    && context.state().projectsStatus.isError();
            }), "the failure should leave the grid usable: " + describe(context.session));
            expect(!context.state().selectedProject.has_value(), "no project is selected");
            expect(stemhub::sessioncache::loadProjectId().isEmpty(), "the saved project id is cleared");
            expect(context.state().projectsStatus.text.containsIgnoreCase("Failed to load workspaces"), describe(context.session));
        }

        beginTest("Requests made while the session is busy are ignored");
        {
            TestContext context;
            const auto projectA = makeProject("project-a", "Project A");
            const auto projectB = makeProject("project-b", "Project B");
            const auto branchMain = makeBranch("branch-main", projectA.id, "main");
            const auto branchAlt = makeBranch("branch-alt", projectA.id, "alt");
            context.api->projects = { projectA, projectB };
            context.api->projectBranches[projectA.id] = { branchMain, branchAlt };
            context.api->projectBranches[projectB.id] = { makeBranch("branch-b", projectB.id, "main") };
            context.api->branchVersions[branchAlt.id] = { makeVersion("22222222-0000", branchAlt.id, "alt") };
            signIn(context);

            auto openGate = std::make_shared<BlockingGate>();
            context.api->branchFetchGates[projectA.id] = openGate;
            context.session.requestOpenProject(projectA.id, {});
            openGate->waitUntilEntered();
            context.session.requestOpenProject(projectB.id, {});
            expect(context.state().operationState == OperationState::loadingProjects, describe(context.session));

            openGate->release();
            expect(waitUntil(context.session, [&context] { return context.isIdle(); }), "the first open should finish");
            openGate->waitUntilFinished();
            expect(context.state().selectedProject.has_value() && context.state().selectedProject->id == projectA.id,
                   "the open requested meanwhile was ignored: " + describe(context.session));

            auto historyGate = std::make_shared<BlockingGate>();
            context.api->versionFetchGates[branchAlt.id] = historyGate;
            context.session.requestSelectBranch(branchAlt.id);
            historyGate->waitUntilEntered();
            context.session.requestSelectBranch(branchMain.id);
            context.session.requestRefreshVersionHistory();
            context.session.requestOpenProject(projectB.id, {});
            expect(context.state().operationState == OperationState::pulling, describe(context.session));

            historyGate->release();
            expect(waitUntil(context.session, [&context] { return context.isIdle(); }), "the branch switch should finish");
            historyGate->waitUntilFinished();
            expect(context.state().selectedBranchId == branchAlt.id && context.state().versionHistory.size() == 1,
                   "only the first request ran: " + describe(context.session));
        }

        beginTest("Results of jobs started before a sign-out are dropped");
        {
            TestContext context;
            const auto projectA = makeProject("project-a", "Song A");
            const auto projectB = makeProject("project-b", "Song B");
            const auto branchB = makeBranch("branch-b", projectB.id, "main");
            context.api->projects = { projectA, projectB };
            context.api->projectBranches[projectA.id] = { makeBranch("branch-a", projectA.id, "main") };
            context.api->projectBranches[projectB.id] = { branchB };
            const auto versionB = context.api->addVersion(branchB.id, "b", { { "b.flp", "b" } });

            const auto fileA = context.environment.root.getChildFile("a.flp");
            expect(fileA.replaceWithText("a"));
            signIn(context);
            openProject(context, projectA.id, fileA);

            auto gate = std::make_shared<BlockingGate>();
            context.api->setCheckMissingGate(projectA.id, gate);
            context.session.requestPushVersion("from A", "FL Studio");
            gate->waitUntilEntered();

            context.session.signOut();
            signIn(context);
            openProject(context, projectB.id, {});

            // The save still reaches the server, but its result belongs to the old session.
            gate->release();
            expect(waitForResults(context.session, 1), "the save should finish");
            expect(context.api->getCreatedVersions().size() == 2, "the version was created");
            expect(context.state().selectedProject.has_value() && context.state().selectedProject->id == projectB.id,
                   "project B stays open: " + describe(context.session));
            expect(context.state().versionHistory.size() == 1 && context.state().versionHistory.front().id == versionB,
                   "project B's history is untouched");
            expect(context.state().selectedVersionId == versionB, "the selection is untouched");
            expect(!context.state().workingCopy.describes(fileA), "project A's file is not the working copy");
            expect(context.isIdle(), describe(context.session));
        }

        beginTest("Signing out during a save drops its result");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { makeBranch("branch-1", project.id, "main") };

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp"));
            signIn(context);
            openProject(context, project.id, projectFile);

            auto gate = std::make_shared<BlockingGate>();
            context.api->setCheckMissingGate(project.id, gate);
            context.session.requestPushVersion("first", "FL Studio");
            gate->waitUntilEntered();
            context.session.signOut();
            gate->release();

            expect(waitForResults(context.session, 1), "the save job should finish");
            expect(context.api->getCreatedVersions().size() == 1, "the version was created on the server");
            expect(context.state().authState == AuthState::signedOut, describe(context.session));
            expect(!context.state().selectedProject.has_value(), "no project after signing out");
            expect(context.state().versionHistory.empty(), "the save's history is dropped");
        }

        beginTest("Destroying the session waits for running jobs");
        {
            auto jobReturned = std::make_shared<std::atomic<bool>>(false);
            auto gate = std::make_shared<BlockingGate>();
            {
                TestContext context;
                const auto project = makeProject("project-1", "Project");
                context.api->projects = { project };
                context.api->projectBranches[project.id] = { makeBranch("branch-1", project.id, "main") };
                context.api->branchFetchGates[project.id] = gate;
                context.api->branchFetchReturned = jobReturned;
                signIn(context);

                context.session.requestOpenProject(project.id, {});
                gate->waitUntilEntered();

                // Released while the session is being destroyed.
                juce::Thread::launch([gate]
                {
                    juce::Thread::sleep(100);
                    gate->release();
                });
            }

            expect(jobReturned->load(), "the session must not be destroyed while its job still runs");
        }

        beginTest("Saves chain their parent versions and restores never delete earlier ones");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };

            const auto sourceFolder = context.environment.root.getChildFile("source");
            const auto projectFile = sourceFolder.getChildFile("song.flp");
            expect(sourceFolder.getChildFile("Drums").createDirectory().wasOk());
            expect(projectFile.replaceWithText("flp v1"));
            expect(sourceFolder.getChildFile("Drums/kick.wav").replaceWithText("kick"));
            expect(sourceFolder.getChildFile("kick.wav").replaceWithText("a different kick"));

            signIn(context);
            openProject(context, project.id, projectFile);

            context.session.requestPushVersion("first", "FL Studio");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().versionHistory.size() == 1; }),
                   "first save should land in the history: " + describe(context.session));
            const auto firstVersionId = context.api->getCreatedVersions().at(0).id;
            expect(context.state().sessionStatus.severity == Status::Severity::success, describe(context.session));
            expect(context.state().selectedVersionId == firstVersionId, "the saved version should be selected");
            expect(context.state().openedVersionId == firstVersionId, "the saved version is the one in the DAW");

            // Restore into a folder of its own, keeping subfolders.
            const auto restoresFolder = context.environment.root.getChildFile("restores");
            expect(restoresFolder.createDirectory().wasOk());
            context.session.requestRestoreVersion(firstVersionId, restoresFolder);
            expect(waitUntil(context.session, [&context, &restoresFolder]
            {
                return context.isIdle() && context.state().selectedProjectFile.isAChildOf(restoresFolder);
            }), "restore should finish: " + describe(context.session));
            const auto restoredFile = context.state().selectedProjectFile;
            const auto restoredFolder = restoredFile.getParentDirectory();
            expect(restoredFile.existsAsFile(), restoredFile.getFullPathName());
            expect(restoredFolder.getFileName() == "song-" + firstVersionId.substring(0, 8), restoredFolder.getFullPathName());
            expect(restoredFolder.getChildFile("Drums/kick.wav").loadFileAsString() == "kick", "nested files keep their folder");
            expect(restoredFolder.getChildFile("kick.wav").loadFileAsString() == "a different kick", "same-name files don't collide");
            expect(context.openedFiles.contains(restoredFile), "the restored project is opened in the DAW");

            // Saving the restored copy chains from the restored version.
            simulateDawSave(restoredFile, " edit 1");
            context.session.requestPushVersion("second", "FL Studio");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().versionHistory.size() == 2; }),
                   "second save should land in the history: " + describe(context.session));
            auto created = context.api->getCreatedVersions();
            expect(created.size() == 2 && created[1].parentVersionId == firstVersionId,
                   "the second version's parent is the restored one");
            const auto secondVersionId = created[1].id;
            expect(context.state().versionHistory.front().id == secondVersionId, "history is refreshed after a save");
            expect(context.state().selectedVersionId == secondVersionId, "the new version is selected");

            // Nothing changed on disk: no new version, even after a refresh of the same branch.
            context.session.requestRefreshVersionHistory();
            expect(waitUntil(context.session, [&context] { return context.isIdle(); }), "refresh should finish");
            context.session.requestPushVersion("no changes", "FL Studio");
            expect(context.isIdle() && context.state().sessionStatus.severity == Status::Severity::warning
                       && context.state().sessionStatus.text.contains("No changes"),
                   "an unchanged file should not be saved again: " + describe(context.session));
            expect(context.api->getCreatedVersions().size() == 2, "no version is created for an unchanged file");

            simulateDawSave(restoredFile, " edit 2");
            context.session.requestPushVersion("third", "FL Studio");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().versionHistory.size() == 3; }),
                   "third save should land in the history: " + describe(context.session));
            created = context.api->getCreatedVersions();
            expect(created.size() == 3 && created[2].parentVersionId == secondVersionId,
                   "each save's parent is the previous save");

            // Restoring the same version again goes to a new folder; the edited copy stays.
            context.session.requestRestoreVersion(firstVersionId, restoresFolder);
            expect(waitUntil(context.session, [&context, &restoredFile]
            {
                return context.isIdle() && context.state().selectedProjectFile != restoredFile
                    && context.state().selectedProjectFile.existsAsFile();
            }), "second restore should finish: " + describe(context.session));
            expect(restoredFile.loadFileAsString() == "flp v1 edit 1 edit 2", "an earlier restore is never deleted");
            expect(context.state().selectedProjectFile.getParentDirectory().getFileName() == restoredFolder.getFileName() + " (2)",
                   "the new folder is numbered: " + context.state().selectedProjectFile.getFullPathName());
            expect(context.state().selectedVersionId == firstVersionId, "the numbered folder still maps to its version");
        }

        beginTest("Only one save runs at a time, and the project stays open meanwhile");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { makeBranch("branch-1", project.id, "main") };

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp"));
            signIn(context);
            openProject(context, project.id, projectFile);

            auto gate = std::make_shared<BlockingGate>();
            context.api->setCheckMissingGate(project.id, gate);
            context.session.requestPushVersion("first", "FL Studio");
            gate->waitUntilEntered();
            context.session.requestPushVersion("second", "FL Studio");
            context.session.requestRefreshVersionHistory();
            context.session.showProjectSelection();
            expect(context.state().operationState == OperationState::committing
                       && context.state().uiState == UIState::dashboard,
                   "nothing else starts while saving: " + describe(context.session));

            gate->release();
            expect(waitUntil(context.session, [&context] { return context.isIdle(); }), "save should finish");
            const auto created = context.api->getCreatedVersions();
            expect(created.size() == 1 && created.front().commitMessage == "first", "the second save request is ignored");

            context.session.showProjectSelection();
            expect(context.state().uiState == UIState::projectSelection, "back to the grid once the save is done");
        }

        beginTest("Opening a project without a local copy restores its latest version");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };
            const auto versionId = context.api->addVersion(branch.id, "from a collaborator",
                                                           { { "song.flp", "flp" }, { "Samples/kick.wav", "kick" } });

            signIn(context);
            context.session.requestOpenProject(project.id, {}, true);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().selectedProjectFile.existsAsFile(); }),
                   "the latest version should be restored: " + describe(context.session));

            const auto restoredFile = context.state().selectedProjectFile;
            expect(restoredFile.isAChildOf(context.environment.root.getChildFile("managed")), restoredFile.getFullPathName());
            expect(restoredFile.getParentDirectory().getChildFile("Samples/kick.wav").loadFileAsString() == "kick");
            expect(context.openedFiles.contains(restoredFile), "the restored copy is opened in the DAW");
            expect(context.state().openedVersionId == versionId, "the restored version is the one in the DAW");
            expect(context.state().projectsStatus.isEmpty(), "the grid's progress message is cleared");
        }

        beginTest("Opening a project never replaces unsaved local changes");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };
            context.api->addVersion(branch.id, "first", { { "song.flp", "flp v1" } });

            signIn(context);
            context.session.requestOpenProject(project.id, {}, true);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().selectedProjectFile.existsAsFile(); }),
                   "the first version should be restored");
            const auto localCopy = context.state().selectedProjectFile;

            // Someone saves a newer version while this copy has unsaved edits.
            simulateDawSave(localCopy, " my edit");
            context.api->addVersion(branch.id, "second", { { "song.flp", "flp v2" } });

            context.session.requestOpenProject(project.id, localCopy, true);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().versionHistory.size() == 2; }),
                   "the project should reopen: " + describe(context.session));
            expect(context.state().selectedProjectFile == localCopy, "the edited copy stays the working file");
            expect(localCopy.loadFileAsString() == "flp v1 my edit", "the edited copy is untouched");
            expect(context.state().sessionStatus.severity == Status::Severity::warning
                       && context.state().sessionStatus.text.contains("not saved"),
                   describe(context.session));
            expect(context.openedFiles.size() == 1, "nothing else is opened in the DAW");
        }

        beginTest("Opening a project updates an unchanged older copy");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };
            context.api->addVersion(branch.id, "first", { { "song.flp", "flp v1" } });

            signIn(context);
            context.session.requestOpenProject(project.id, {}, true);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().selectedProjectFile.existsAsFile(); }),
                   "the first version should be restored");
            const auto olderCopy = context.state().selectedProjectFile;

            const auto newerVersionId = context.api->addVersion(branch.id, "second", { { "song.flp", "flp v2" } });
            context.session.requestOpenProject(project.id, olderCopy, true);
            expect(waitUntil(context.session, [&context, &olderCopy]
            {
                return context.isIdle() && context.state().selectedProjectFile != olderCopy;
            }), "the newer version should be restored: " + describe(context.session));
            expect(context.state().selectedProjectFile.loadFileAsString() == "flp v2");
            expect(context.state().openedVersionId == newerVersionId);
            expect(olderCopy.loadFileAsString() == "flp v1", "the older copy is kept");
        }

        beginTest("A restored project the DAW can't open is still the working copy");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };
            const auto versionId = context.api->addVersion(branch.id, "first", { { "song.flp", "flp v1" } });
            context.canOpenFiles = false;

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("mine"));
            signIn(context);
            openProject(context, project.id, projectFile);

            context.session.requestRestoreVersion(versionId, context.environment.root);
            expect(waitUntil(context.session, [&context, &projectFile]
            {
                return context.isIdle() && context.state().selectedProjectFile != projectFile;
            }), "the restore should finish: " + describe(context.session));

            const auto restoredFile = context.state().selectedProjectFile;
            expect(restoredFile.loadFileAsString() == "flp v1", restoredFile.getFullPathName());
            expect(context.state().sessionStatus.severity == Status::Severity::warning
                       && context.state().sessionStatus.text.contains(restoredFile.getFullPathName()),
                   "the user is told where the restored project is: " + describe(context.session));
            expect(context.state().workingCopy.versionId == versionId, "the next save builds on the restored version");
            expect(context.state().openedVersionId.isEmpty(), "the DAW is not assumed to hold it");
        }

        beginTest("A restore folder's version is the next parent but never blocks a save");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };
            const auto restoredVersionId = context.api->addVersion(branch.id, "first", { { "song.flp", "flp v1" } });
            context.api->addVersion(branch.id, "second", { { "song.flp", "flp v2" } });

            // A copy restored in an earlier session: only its folder name says which version it is.
            const auto restoredFile = context.environment.root
                                          .getChildFile("song-" + restoredVersionId.substring(0, 8))
                                          .getChildFile("song.flp");
            expect(restoredFile.getParentDirectory().createDirectory().wasOk());
            expect(restoredFile.replaceWithText("flp v1"));

            signIn(context);
            openProject(context, project.id, restoredFile);

            context.session.requestPushVersion("from the restored copy", "FL Studio");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.api->getCreatedVersions().size() == 3; }),
                   "the save must not be refused: " + describe(context.session));
            expect(context.api->getCreatedVersions().back().parentVersionId == restoredVersionId,
                   "the restored version is the parent, not the branch head");
        }

        beginTest("Each distinct file is uploaded once");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { makeBranch("branch-1", project.id, "main") };

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp"));
            expect(context.environment.root.getChildFile("Drums/kick.wav").create().wasOk());
            expect(context.environment.root.getChildFile("Drums/kick.wav").replaceWithText("kick"));
            expect(context.environment.root.getChildFile("Backup/kick copy.wav").create().wasOk());
            expect(context.environment.root.getChildFile("kick copy.wav").replaceWithText("kick"));

            signIn(context);
            openProject(context, project.id, projectFile);

            context.session.requestPushVersion("first", "FL Studio");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.api->getCreatedVersions().size() == 1; }),
                   "first save: " + describe(context.session));
            const auto kickSha = sha256Of(juce::MemoryBlock("kick", 4));
            expect(context.api->getUploadCount(kickSha) == 1, "two identical files are one upload");

            simulateDawSave(projectFile, " edit");
            context.session.requestPushVersion("second", "FL Studio");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.api->getCreatedVersions().size() == 2; }),
                   "second save: " + describe(context.session));
            expect(context.api->getUploadCount(kickSha) == 1, "files the server has are not uploaded again");
        }

        beginTest("Saves beyond the backend limits are refused before uploading");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { makeBranch("branch-1", project.id, "main") };

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp"));
            signIn(context);
            openProject(context, project.id, projectFile);

            context.session.requestPushVersion(juce::String::repeatedString("n", 501), "FL Studio");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().sessionStatus.isError(); }),
                   "a long note should fail: " + describe(context.session));
            expect(context.state().sessionStatus.text.contains("500 characters"), describe(context.session));

            for (int index = 0; index < 501; ++index)
                expect(context.environment.root.getChildFile("stem" + juce::String(index) + ".wav").replaceWithText(juce::String(index)));

            context.session.requestPushVersion("too many files", "FL Studio");
            expect(waitUntil(context.session, [&context]
            {
                return context.isIdle() && context.state().sessionStatus.text.contains("can hold 500");
            }), "501 audio files should fail: " + describe(context.session));
            expect(context.state().sessionStatus.isError(), describe(context.session));
            expect(context.api->getCreatedVersions().empty() && context.api->getUploadCount(sha256Of(juce::MemoryBlock("0", 1))) == 0,
                   "nothing is uploaded");
        }

        beginTest("A download that fails its checksum leaves nothing behind");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };
            const auto versionId = context.api->addVersion(branch.id, "first", { { "song.flp", "flp" }, { "Drums/kick.wav", "kick" } });
            context.api->corruptDownloads = true;

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("mine"));
            signIn(context);
            openProject(context, project.id, projectFile);

            const auto restoresFolder = context.environment.root.getChildFile("restores");
            expect(restoresFolder.createDirectory().wasOk());
            context.session.requestRestoreVersion(versionId, restoresFolder);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().sessionStatus.isError(); }),
                   "the restore should fail: " + describe(context.session));
            expect(context.state().sessionStatus.text.contains("checksum"), describe(context.session));

            juce::Array<juce::File> leftovers;
            restoresFolder.findChildFiles(leftovers, juce::File::findFilesAndDirectories, true);
            expect(leftovers.isEmpty(), "the half-restored folder is removed");
            expect(context.state().selectedProjectFile == projectFile, "the working file doesn't change");
            expect(context.openedFiles.isEmpty(), "nothing is opened in the DAW");
        }

        beginTest("A refused token signs the user out");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { makeBranch("branch-1", project.id, "main") };

            signIn(context);
            openProject(context, project.id, {});

            context.api->rejectToken = true;
            context.session.requestRefreshVersionHistory();
            expect(waitUntil(context.session, [&context] { return context.state().authState == AuthState::authError; }),
                   "a 401 should end the session: " + describe(context.session));
            expect(context.state().uiState == UIState::login, "back on the login screen");
            expect(context.state().authStatus.text.contains("expired"), describe(context.session));
            expect(!context.state().selectedProject.has_value() && context.state().accessToken.isEmpty(), "the session is cleared");
            expect(stemhub::sessioncache::loadAccessToken().isEmpty(), "the refused token is forgotten");
        }

        beginTest("Being offline at startup keeps the saved session");
        {
            TestContext context;
            context.api->projects = { makeProject("project-1", "Song") };
            context.api->offline = true;
            stemhub::sessioncache::saveAccessToken("valid-token");

            context.session.requestRestoreCachedSession();
            expect(waitUntil(context.session, [&context] { return context.state().authState == AuthState::authError; }),
                   "an unreachable server should be reported: " + describe(context.session));
            expect(context.state().authStatus.isError() && context.state().authStatus.text.contains("Can't reach StemHub"),
                   describe(context.session));
            expect(stemhub::sessioncache::loadAccessToken() == "valid-token", "the saved token is kept");

            context.api->offline = false;
            context.session.requestRestoreCachedSession();
            expect(waitUntil(context.session, [&context] { return context.state().authState == AuthState::signedIn; }),
                   "the next attempt signs in: " + describe(context.session));
        }
    }
};

class SnapshotTests final : public StemhubTest
{
public:
    SnapshotTests() : StemhubTest("Stemhub snapshots") {}

    void runTest() override
    {
        beginTest("Manifest paths must stay inside the restore folder");
        {
            for (const auto* path : { "song.flp", "Drums/kick.wav", "Samples/Imported/Kick 01.wav", "a.b/c.wav" })
                expect(SnapshotBundler::isSafeManifestPath(path), juce::String("should accept ") + path);

            for (const auto* path : { "", "/etc/passwd", "../evil.wav", "Drums/../../evil.wav", "./song.flp",
                                      "a//b.wav", "a/b/", "C:\\evil.wav", "C:evil.wav", "..\\evil.wav",
                                      "Drums\\kick.wav", "trailing.", "trailing ", "NUL.wav", "Samples/con",
                                      "bad\nname.wav" })
                expect(!SnapshotBundler::isSafeManifestPath(path), juce::String("should reject ") + juce::String(path).quoted());

            expect(SnapshotBundler::isSafeManifestPath(juce::String::repeatedString("a", 255)), "255 characters fit the backend limit");
            expect(!SnapshotBundler::isSafeManifestPath(juce::String::repeatedString("a", 256)), "256 characters exceed the backend limit");
        }

        beginTest("Manifests with unsafe or conflicting entries are rejected");
        {
            const auto shaA = juce::String::repeatedString("a", 64);
            const auto shaB = juce::String::repeatedString("b", 64);

            ParsedManifest parsed;
            auto result = SnapshotBundler::parseManifest(makeManifest("../../evil.flp", shaA, {}), parsed);
            expect(result.failed() && result.getErrorMessage().contains("unsafe"), "traversal in the project file is rejected");

            result = SnapshotBundler::parseManifest(
                makeManifest("song.flp", shaA, { { "Drums/kick.wav", shaA }, { "drums/KICK.wav", shaB } }), parsed);
            expect(result.failed() && result.getErrorMessage().contains("two different files"),
                   "two different files at the same path are rejected");

            result = SnapshotBundler::parseManifest(
                makeManifest("song.flp", shaA, { { "Drums/kick.wav", shaB }, { "Drums/kick.wav", shaB } }), parsed);
            expect(result.wasOk() && parsed.entries.size() == 2, "an exact duplicate is kept once");

            result = SnapshotBundler::parseManifest(
                makeManifest("song.flp", "../" + shaA.substring(3), {}), parsed);
            expect(result.failed(), "a hash that is not hex is rejected");
        }

        beginTest("A working-copy baseline only vouches for what it recorded");
        {
            TestEnvironment environment;
            const auto file = environment.root.getChildFile("song.flp");
            expect(file.replaceWithText("flp"));

            const WorkingCopyBaseline guessed { file, "version-1" };
            expect(guessed.isSet() && !guessed.hasRecordedState(), "a version without recorded state");
            expect(!guessed.isUnchanged(), "an unknown state counts as changed");

            const auto recorded = WorkingCopyBaseline::recordedNow(file, "version-1");
            expect(recorded.isUnchanged(), "unchanged right after recording");
            simulateDawSave(file, " edit");
            expect(!recorded.isUnchanged(), "a DAW save is a change");

            expect(!WorkingCopyBaseline {}.isSet() && !WorkingCopyBaseline {}.describes(file), "an empty baseline describes nothing");
        }
    }
};

class ApiTests final : public StemhubTest
{
public:
    ApiTests() : StemhubTest("Stemhub API") {}

    void runTest() override
    {
        beginTest("API responses are parsed into domain values");
        {
            namespace json = stemhub::api::json;

            const auto versions = json::parseVersions(juce::JSON::parse(R"([{
                "id": "v1", "branch_id": "b1", "created_at": "2026-03-18T10:00:00Z", "commit_message": "first",
                "manifest_json": { "project_file": { "size_bytes": 10 }, "tracks": [ { "size_bytes": 5 }, { "size_bytes": 7 } ] }
            }])"));
            expect(versions.ok() && versions.value->size() == 1, "a version list parses");
            if (versions.ok())
                expect(versions.value->front().totalSizeBytes == 22, "the size is summed from the manifest");

            expect(!json::parseVersions(juce::JSON::parse(R"({"id": "v1"})")).ok(), "an object is not a list");
            const auto missingFields = json::parseProject(juce::JSON::parse(R"({"id": "p1"})"));
            expect(!missingFields.ok() && missingFields.error->kind == ApiError::Kind::invalidResponse,
                   "a project without a name is invalid");
            expect(json::parseMissingBlobs(juce::JSON::parse(R"({"missing": ["a", "b"]})")).value->size() == 2);
            expect(!json::parseLogin(juce::JSON::parse(R"({"token_type": "bearer"})")).ok(), "a login without a token fails");

            expect(json::extractErrorMessage(juce::JSON::parse(R"({"detail": "Incorrect email or password"})"), {}, "x")
                       == "Incorrect email or password");
            expect(json::extractErrorMessage(juce::JSON::parse(R"({"detail": [{"msg": "field required"}]})"), {}, "x")
                       == "field required");
            expect(json::extractErrorMessage({}, "<html>Bad gateway</html>", "Request failed.") == "Request failed.",
                   "HTML error pages are not shown");

            expect(ApiError::kindForStatus(401) == ApiError::Kind::unauthorized);
            expect(ApiError::kindForStatus(422) == ApiError::Kind::invalidRequest);
            expect(ApiError::kindForStatus(503) == ApiError::Kind::server);
            expect(ApiError::kindForStatus(0) == ApiError::Kind::network);
        }

        beginTest("The API base URL is https, or http to this machine");
        {
            using stemhub::api::chooseBaseUrl;
            using stemhub::api::normaliseBaseUrl;

            expect(normaliseBaseUrl("https://api.stemhub.app/") == "https://api.stemhub.app");
            expect(normaliseBaseUrl(" http://localhost:8000 ") == "http://localhost:8000");
            expect(normaliseBaseUrl("http://127.0.0.1:8000/") == "http://127.0.0.1:8000");
            expect(normaliseBaseUrl("http://[::1]:8000") == "http://[::1]:8000");
            expect(normaliseBaseUrl("http://api.stemhub.app").isEmpty(), "plain http to another host would leak the token");
            expect(normaliseBaseUrl("http://localhost.evil.example").isEmpty());
            expect(normaliseBaseUrl("http://localhost@evil.example").isEmpty());
            expect(normaliseBaseUrl("ftp://api.stemhub.app").isEmpty());
            expect(normaliseBaseUrl("https://").isEmpty());

            const juce::String builtIn = "http://localhost:8000/";
            const juce::String configFile = R"({ "api_base_url": "https://staging.stemhub.app" })";
            expect(chooseBaseUrl({}, {}, builtIn) == "http://localhost:8000", "the built-in URL is the fallback");
            expect(chooseBaseUrl({}, configFile, builtIn) == "https://staging.stemhub.app", "the config file overrides it");
            expect(chooseBaseUrl("https://dev.stemhub.app", configFile, builtIn) == "https://dev.stemhub.app",
                   "the environment wins");
            expect(chooseBaseUrl("http://evil.example", "not json", builtIn) == "http://localhost:8000",
                   "unusable overrides are ignored");
        }

        beginTest("Blob downloads follow the storage redirect without the bearer token");
        {
            const juce::String storagePath = "/storage/blob?X-Goog-Signature=ab%2Fcd&X-Goog-Credential=a%40b";
            std::unique_ptr<LocalHttpServer> server;
            server = std::make_unique<LocalHttpServer>([&server, storagePath](const LocalHttpServer::Request& request)
            {
                if (request.requestLine.startsWith("GET /projects/"))
                    return "HTTP/1.1 307 Temporary Redirect\r\nLocation: " + server->getBaseUrl() + storagePath
                         + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";

                return juce::String("HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: close\r\n\r\nbytes");
            });

            TestEnvironment environment;
            const auto destination = environment.root.getChildFile("blob.bin");
            ApiClient client(server->getBaseUrl());
            const auto result = client.downloadBlob("project-1", juce::String::repeatedString("c", 64), destination, "secret-token");

            expect(result.ok(), result.errorMessage("download failed"));
            expect(destination.loadFileAsString() == "bytes", "the storage response is written to disk");

            const auto requests = server->getRequests();
            expect(requests.size() == 2, "one request to the API, one to storage");
            if (requests.size() == 2)
            {
                expect(requests[0].headers.contains("Bearer secret-token"), "the API request is authorized");
                expect(requests[1].requestLine == "GET " + storagePath + " HTTP/1.1",
                       "the presigned URL is requested as-is: " + requests[1].requestLine);
                expect(!requests[1].headers.containsIgnoreCase("authorization"), "the token never reaches storage");
            }
        }
    }
};

SessionTests sessionTests;
SnapshotTests snapshotTests;
ApiTests apiTests;
}

int main()
{
   #if ! JUCE_WINDOWS
    // The local test server writes with send(); a client hanging up must not kill the run.
    std::signal(SIGPIPE, SIG_IGN);
   #endif

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
