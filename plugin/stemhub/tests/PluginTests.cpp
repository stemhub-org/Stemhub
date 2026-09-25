#include <atomic>
#include <csignal>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <JuceHeader.h>

#if ! JUCE_WINDOWS
 #include <sys/stat.h>
#endif

#include "application/CredentialStore.hpp"
#include "application/PluginState.hpp"
#include "application/RestoreHandoff.hpp"
#include "application/SnapshotBundler.hpp"
#include "application/SnapshotFiles.hpp"
#include "application/StemhubSession.hpp"
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
    }

    ~TestEnvironment()
    {
        root.deleteRecursively();
    }

    juce::File root;
};

// A plugin instance's session on the fake backend, with the stores every instance on the machine
// shares under a temporary folder. Files a session would open in the DAW are recorded instead.
struct TestContext
{
    TestContext()
    {
        recordOpenedFiles(session);
    }

    // Another plugin instance, as the DAW creates one for each project it opens.
    std::unique_ptr<StemhubSession> makeInstance()
    {
        auto instance = std::make_unique<StemhubSession>(api, storage);
        recordOpenedFiles(*instance);
        return instance;
    }

    [[nodiscard]] const SessionState& state() const noexcept { return session.getState(); }
    [[nodiscard]] bool isIdle() const noexcept { return !session.isBusy(); }
    [[nodiscard]] const juce::File& handoffFile() const noexcept { return storage.restoreHandoffFile; }

    // Declared first, so it is cleaned up after the sessions and the jobs that use its files.
    TestEnvironment environment;
    std::shared_ptr<FakeProjectApi> api { std::make_shared<FakeProjectApi>() };
    SessionStorage storage { std::make_shared<InMemoryCredentialStore>(),
                             environment.root.getChildFile("pending-restore.json"),
                             environment.root.getChildFile("managed") };
    juce::Array<juce::File> openedFiles;
    bool canOpenFiles { true };
    StemhubSession session { api, storage };

private:
    void recordOpenedFiles(StemhubSession& target)
    {
        target.setOpenFileHandler([this](const juce::File& file)
        {
            openedFiles.add(file);
            return canOpenFiles;
        });
    }
};

class StemhubTest : public juce::UnitTest
{
protected:
    explicit StemhubTest(const juce::String& testName)
        : juce::UnitTest(testName, "plugin")
    {
    }

    // Waits for the linked project too, when there is one.
    void signIn(StemhubSession& session)
    {
        session.requestSignIn("user@example.com", "secret");
        expect(waitUntil(session, [&session] { return session.getState().authState == AuthState::signedIn && !session.isBusy(); }),
               "sign-in should succeed: " + describe(session));
    }

    // With the saved token, as when the plugin window opens.
    void restoreSavedSession(StemhubSession& session)
    {
        session.requestRestoreSavedSession();
        expect(waitUntil(session, [&session] { return session.getState().authState == AuthState::signedIn && !session.isBusy(); }),
               "the saved session should restore: " + describe(session));
    }

    void openProject(StemhubSession& session, const juce::String& projectId, const juce::File& projectFile)
    {
        session.requestOpenProject(projectId, projectFile);
        expect(waitUntil(session, [&session, projectId]
        {
            const auto& state = session.getState();
            return state.selectedProject.has_value() && state.selectedProject->id == projectId && !session.isBusy();
        }), "project should open: " + describe(session));
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
            context.storage.credentials->saveToken("expired-token");

            context.session.requestRestoreSavedSession();
            expect(waitUntil(context.session, [&context] { return context.state().authState == AuthState::authError; }),
                   "an invalid saved token should end in authError: " + describe(context.session));
            expect(context.state().uiState == UIState::login, "the login screen stays visible");
            expect(context.state().authStatus.text.containsIgnoreCase("sign in again"), describe(context.session));
            expect(context.storage.credentials->loadToken().isEmpty(), "the invalid token is forgotten");
        }

        beginTest("A DAW project reopens its linked project, branch and file");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branchMain = makeBranch("branch-main", project.id, "main");
            const auto branchAlt = makeBranch("branch-alt", project.id, "alt");
            context.api->projects = { makeProject("project-0", "Other"), project };
            context.api->projectBranches[project.id] = { branchMain, branchAlt };
            context.api->branchVersions[branchAlt.id] = { makeVersion("22222222-0000", branchAlt.id, "alt") };

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp"));
            context.storage.credentials->saveToken("valid-token");

            context.session.restoreLink({ project.id, branchAlt.id, projectFile });
            restoreSavedSession(context.session);
            expect(context.state().uiState == UIState::dashboard, describe(context.session));
            expect(context.state().selectedProject.has_value() && context.state().selectedProject->id == project.id,
                   describe(context.session));
            expect(context.state().selectedBranchId == branchAlt.id, "the linked branch is opened, not main");
            expect(context.state().selectedProjectFile == projectFile, "the linked file is the working file");
            expect(context.openedFiles.isEmpty(), "reopening the link opens nothing in the DAW");
        }

        beginTest("A DAW project linked to a project that is gone shows the project grid");
        {
            TestContext context;
            context.api->projects = { makeProject("project-2", "Project Two") };
            context.storage.credentials->saveToken("valid-token");

            context.session.restoreLink({ "missing-project", "branch-1", context.environment.root.getChildFile("song.flp") });
            restoreSavedSession(context.session);
            expect(context.state().uiState == UIState::projectSelection, describe(context.session));
            expect(context.state().projectsStatus.severity == Status::Severity::warning
                       && context.state().projectsStatus.text.contains("no longer open"),
                   describe(context.session));
            expect(!context.state().selectedProject.has_value(), "no project is selected");
            expect(context.state().link.projectId == "missing-project", "the link stays until another project is opened");
        }

        beginTest("A linked file that is missing keeps its place in the link");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { makeBranch("branch-1", project.id, "main") };
            context.storage.credentials->saveToken("valid-token");

            const auto unpluggedFile = context.environment.root.getChildFile("External drive").getChildFile("song.flp");
            context.session.restoreLink({ project.id, "branch-1", unpluggedFile });
            restoreSavedSession(context.session);
            expect(context.state().uiState == UIState::dashboard, describe(context.session));
            expect(context.session.getEffectiveProjectFile() == juce::File(), "there is nothing to save from until it is back");
            expect(context.state().link.workingFile == unpluggedFile, "the DAW project stays linked to it");
        }

        beginTest("A linked project that fails to open leaves the session usable");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Project");
            context.api->projects = { project };
            context.api->branchErrors[project.id] = "Failed to load workspaces.";
            context.storage.credentials->saveToken("valid-token");

            context.session.restoreLink({ project.id, {}, {} });
            restoreSavedSession(context.session);
            expect(context.state().uiState == UIState::projectSelection && context.state().projectsStatus.isError(),
                   "the failure should leave the grid usable: " + describe(context.session));
            expect(!context.state().selectedProject.has_value(), "no project is selected");
            expect(context.state().projectsStatus.text.containsIgnoreCase("Failed to load workspaces"), describe(context.session));
        }

        beginTest("Each plugin instance keeps its own project, and signing out keeps the link");
        {
            TestContext context;
            const auto projectA = makeProject("project-a", "Song A");
            const auto projectB = makeProject("project-b", "Song B");
            context.api->projects = { projectA, projectB };
            context.api->projectBranches[projectA.id] = { makeBranch("branch-a", projectA.id, "main") };
            context.api->projectBranches[projectB.id] = { makeBranch("branch-b", projectB.id, "main") };
            const auto fileA = context.environment.root.getChildFile("a.flp");
            const auto fileB = context.environment.root.getChildFile("b.flp");
            expect(fileA.replaceWithText("a") && fileB.replaceWithText("b"));

            signIn(context.session);
            openProject(context.session, projectA.id, fileA);

            // A second DAW project with the plugin: the saved token signs it in, with no link.
            auto second = context.makeInstance();
            restoreSavedSession(*second);
            expect(second->getState().uiState == UIState::projectSelection, describe(*second));
            openProject(*second, projectB.id, fileB);

            expect(context.state().link == ProjectLink { projectA.id, "branch-a", fileA }, "the first instance is linked to A");
            expect(second->getState().link == ProjectLink { projectB.id, "branch-b", fileB }, "the second one to B");

            context.session.signOut();
            expect(second->getState().authState == AuthState::signedIn, "the other instance stays signed in");
            expect(context.state().link.projectId == projectA.id, "the DAW project keeps its link");

            signIn(context.session);
            expect(context.state().selectedProject.has_value() && context.state().selectedProject->id == projectA.id
                       && context.state().selectedProjectFile == fileA,
                   "signing in again reopens the linked project: " + describe(context.session));
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
            signIn(context.session);

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
            signIn(context.session);
            openProject(context.session, projectA.id, fileA);

            auto gate = std::make_shared<BlockingGate>();
            context.api->setCheckMissingGate(projectA.id, gate);
            context.session.requestPushVersion("from A");
            gate->waitUntilEntered();

            context.session.signOut();
            signIn(context.session);
            openProject(context.session, projectB.id, {});

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
            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            auto gate = std::make_shared<BlockingGate>();
            context.api->setCheckMissingGate(project.id, gate);
            context.session.requestPushVersion("first");
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
                signIn(context.session);

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

            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            context.session.requestPushVersion("first");
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
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.openedFiles.size() == 1; }),
                   "restore should finish: " + describe(context.session));
            const auto restoredFile = context.openedFiles.getFirst();
            const auto restoredFolder = restoredFile.getParentDirectory();
            expect(restoredFile.loadFileAsString() == "flp v1", restoredFile.getFullPathName());
            expect(restoredFolder.getParentDirectory() == restoresFolder
                       && restoredFolder.getFileName() == "song-" + firstVersionId.substring(0, 8),
                   restoredFolder.getFullPathName());
            expect(restoredFolder.getChildFile("Drums/kick.wav").loadFileAsString() == "kick", "nested files keep their folder");
            expect(restoredFolder.getChildFile("kick.wav").loadFileAsString() == "a different kick", "same-name files don't collide");
            expect(context.state().selectedProjectFile == projectFile, "this instance stays with its own project file");
            expect(context.handoffFile().existsAsFile(), "the restored copy waits for the instance the DAW opens");

            // The DAW opens the copy with a plugin instance of its own. Its saved state is from
            // before the first save, so it names the original file.
            auto restoredInstance = context.makeInstance();
            restoredInstance->restoreLink({ project.id, branch.id, projectFile });
            expect(!context.handoffFile().exists(), "the hand-off is taken once");
            restoreSavedSession(*restoredInstance);
            const auto& restoredState = restoredInstance->getState();
            expect(restoredState.selectedProjectFile == restoredFile, "the copy is that instance's working file: " + describe(*restoredInstance));
            expect(restoredState.link.workingFile == restoredFile, "and what its DAW project will be saved with");
            expect(restoredState.openedVersionId == firstVersionId, "the restored version is the one in the DAW");

            restoredInstance->requestPushVersion("nothing new");
            expect(restoredState.sessionStatus.severity == Status::Severity::warning
                       && restoredState.sessionStatus.text.contains("No changes"),
                   "an unchanged copy is not saved again: " + describe(*restoredInstance));

            // Saving the copy chains from the restored version.
            simulateDawSave(restoredFile, " edit 1");
            restoredInstance->requestPushVersion("second");
            expect(waitUntil(*restoredInstance, [&restoredInstance] { return !restoredInstance->isBusy()
                                                                            && restoredInstance->getState().versionHistory.size() == 2; }),
                   "second save should land in the history: " + describe(*restoredInstance));
            auto created = context.api->getCreatedVersions();
            expect(created.size() == 2 && created[1].parentVersionId == firstVersionId,
                   "the second version's parent is the restored one");
            const auto secondVersionId = created[1].id;
            expect(restoredState.versionHistory.front().id == secondVersionId, "history is refreshed after a save");
            expect(restoredState.selectedVersionId == secondVersionId, "the new version is selected");

            // Nothing changed on disk: no new version, even after a refresh of the same branch.
            restoredInstance->requestRefreshVersionHistory();
            expect(waitUntil(*restoredInstance, [&restoredInstance] { return !restoredInstance->isBusy(); }), "refresh should finish");
            restoredInstance->requestPushVersion("no changes");
            expect(restoredState.sessionStatus.text.contains("No changes"), describe(*restoredInstance));
            expect(context.api->getCreatedVersions().size() == 2, "no version is created for an unchanged file");

            simulateDawSave(restoredFile, " edit 2");
            restoredInstance->requestPushVersion("third");
            expect(waitUntil(*restoredInstance, [&restoredInstance] { return !restoredInstance->isBusy()
                                                                            && restoredInstance->getState().versionHistory.size() == 3; }),
                   "third save should land in the history: " + describe(*restoredInstance));
            created = context.api->getCreatedVersions();
            expect(created.size() == 3 && created[2].parentVersionId == secondVersionId,
                   "each save's parent is the previous save");

            // Restoring the same version again goes to a new folder; the edited copy stays.
            restoredInstance->requestRestoreVersion(firstVersionId, restoresFolder);
            expect(waitUntil(*restoredInstance, [&context, &restoredInstance] { return !restoredInstance->isBusy()
                                                                                      && context.openedFiles.size() == 2; }),
                   "second restore should finish: " + describe(*restoredInstance));
            expect(restoredFile.loadFileAsString() == "flp v1 edit 1 edit 2", "an earlier restore is never deleted");
            expect(context.openedFiles.getLast().getParentDirectory().getFileName() == restoredFolder.getFileName() + " (2)",
                   "the new folder is numbered: " + context.openedFiles.getLast().getFullPathName());
            expect(restoredState.selectedProjectFile == restoredFile, "the instance keeps working on its copy");
        }

        beginTest("A save leaves out a copy restored into its own folder");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { makeBranch("branch-1", project.id, "main") };

            const auto folder = context.environment.root.getChildFile("Song");
            const auto projectFile = folder.getChildFile("song.flp");
            expect(folder.getChildFile("kick.wav").create().wasOk() && folder.getChildFile("kick.wav").replaceWithText("kick"));
            expect(projectFile.replaceWithText("flp v1"));
            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            context.session.requestPushVersion("first");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().versionHistory.size() == 1; }),
                   describe(context.session));
            const auto firstVersionId = context.state().workingCopy.versionId;

            // Where the dashboard restores by default: next to the project file.
            context.session.requestRestoreVersion(firstVersionId, folder);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.openedFiles.size() == 1; }),
                   describe(context.session));
            expect(context.openedFiles.getFirst().isAChildOf(folder), context.openedFiles.getFirst().getFullPathName());

            simulateDawSave(projectFile, " edit");
            context.session.requestPushVersion("second");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().versionHistory.size() == 2; }),
                   describe(context.session));

            const auto manifest = context.api->fetchVersionManifest(context.state().workingCopy.versionId, "token");
            expect(manifest.ok(), "the second version has a manifest");
            juce::StringArray trackPaths;
            if (manifest.ok())
                if (const auto* tracks = manifest.value->getProperty("tracks", {}).getArray())
                    for (const auto& track : *tracks)
                        trackPaths.add(track.getProperty("filename", {}).toString());

            expect(trackPaths.joinIntoString(", ") == "kick.wav", "only the project's own audio: " + trackPaths.joinIntoString(", "));
        }

        beginTest("Only one save runs at a time, and the project stays open meanwhile");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { makeBranch("branch-1", project.id, "main") };

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp"));
            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            auto gate = std::make_shared<BlockingGate>();
            context.api->setCheckMissingGate(project.id, gate);
            context.session.requestPushVersion("first");
            gate->waitUntilEntered();
            context.session.requestPushVersion("second");
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

            signIn(context.session);
            context.session.requestOpenProject(project.id, {}, true);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.openedFiles.size() == 1; }),
                   "the latest version should be restored: " + describe(context.session));

            const auto restoredFile = context.openedFiles.getFirst();
            expect(restoredFile.isAChildOf(context.environment.root.getChildFile("managed")), restoredFile.getFullPathName());
            expect(restoredFile.getParentDirectory().getChildFile("Samples/kick.wav").loadFileAsString() == "kick");
            expect(context.state().selectedProject.has_value() && context.state().selectedProjectFile == juce::File(),
                   "this instance had no copy of its own and still has none");
            expect(context.state().projectsStatus.isEmpty(), "the grid's progress message is cleared");

            // The DAW opens the copy. Saved by an earlier version, its plugin state has no link;
            // some hosts hand that empty state over twice.
            auto restoredInstance = context.makeInstance();
            restoredInstance->restoreLink({});
            restoredInstance->restoreLink({});
            expect(restoredInstance->getState().link.workingFile == restoredFile, "the hand-off is taken and kept");
            restoreSavedSession(*restoredInstance);
            const auto& restoredState = restoredInstance->getState();
            expect(restoredState.selectedProject.has_value() && restoredState.selectedProject->id == project.id,
                   "the instance takes the hand-off: " + describe(*restoredInstance));
            expect(restoredState.selectedProjectFile == restoredFile, "and works on the restored copy");
            expect(restoredState.openedVersionId == versionId, "the restored version is the one in the DAW");
        }

        beginTest("Opening a project never replaces unsaved local changes");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp v1"));
            signIn(context.session);
            openProject(context.session, project.id, projectFile);
            context.session.requestPushVersion("first");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().versionHistory.size() == 1; }),
                   "the first version should be saved: " + describe(context.session));

            // Someone saves a newer version while this copy has unsaved edits.
            simulateDawSave(projectFile, " my edit");
            context.api->addVersion(branch.id, "second", { { "song.flp", "flp v2" } });

            context.session.showProjectSelection();
            context.session.requestOpenProject(project.id, projectFile, true);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().versionHistory.size() == 2; }),
                   "the project should reopen: " + describe(context.session));
            expect(context.state().selectedProjectFile == projectFile, "the edited copy stays the working file");
            expect(projectFile.loadFileAsString() == "flp v1 my edit", "the edited copy is untouched");
            expect(context.state().sessionStatus.severity == Status::Severity::warning
                       && context.state().sessionStatus.text.contains("not saved"),
                   describe(context.session));
            expect(context.openedFiles.isEmpty() && !context.handoffFile().exists(), "nothing is restored or opened");
        }

        beginTest("Opening a project updates an unchanged older copy");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp v1"));
            signIn(context.session);
            openProject(context.session, project.id, projectFile);
            context.session.requestPushVersion("first");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().versionHistory.size() == 1; }),
                   "the first version should be saved: " + describe(context.session));
            const auto firstVersionId = context.state().workingCopy.versionId;

            const auto newerVersionId = context.api->addVersion(branch.id, "second", { { "song.flp", "flp v2" } });
            context.session.showProjectSelection();
            context.session.requestOpenProject(project.id, projectFile, true);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.openedFiles.size() == 1; }),
                   "the newer version should be restored: " + describe(context.session));
            const auto newerCopy = context.openedFiles.getFirst();
            expect(newerCopy.loadFileAsString() == "flp v2" && newerCopy.isAChildOf(context.environment.root.getChildFile("managed")),
                   newerCopy.getFullPathName());
            expect(context.state().selectedProjectFile == projectFile && projectFile.loadFileAsString() == "flp v1",
                   "this instance keeps its older copy");
            expect(context.state().openedVersionId == firstVersionId, describe(context.session));

            auto newerInstance = context.makeInstance();
            newerInstance->restoreLink({ project.id, branch.id, projectFile });
            restoreSavedSession(*newerInstance);
            expect(newerInstance->getState().selectedProjectFile == newerCopy, describe(*newerInstance));
            expect(newerInstance->getState().openedVersionId == newerVersionId);
        }

        beginTest("A restore the DAW can't open says where the copy is");
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
            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            context.session.requestRestoreVersion(versionId, context.environment.root);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.openedFiles.size() == 1; }),
                   "the restore should finish: " + describe(context.session));

            const auto restoredFile = context.openedFiles.getFirst();
            expect(restoredFile.loadFileAsString() == "flp v1", restoredFile.getFullPathName());
            expect(context.state().sessionStatus.severity == Status::Severity::warning
                       && context.state().sessionStatus.text.contains(restoredFile.getFullPathName()),
                   "the user is told where the restored project is: " + describe(context.session));
            expect(context.state().selectedProjectFile == projectFile, "this instance keeps its own file");
            expect(context.handoffFile().existsAsFile(), "the copy waits for the user to open it");

            // Opened by hand, with no link saved in it: the instance picks the copy up when its
            // window opens.
            auto openedByHand = context.makeInstance();
            restoreSavedSession(*openedByHand);
            expect(openedByHand->getState().selectedProjectFile == restoredFile, describe(*openedByHand));
            expect(openedByHand->getState().workingCopy.versionId == versionId, "it knows which version the copy holds");
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

            signIn(context.session);
            openProject(context.session, project.id, restoredFile);

            context.session.requestPushVersion("from the restored copy");
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

            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            context.session.requestPushVersion("first");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.api->getCreatedVersions().size() == 1; }),
                   "first save: " + describe(context.session));
            const auto kickSha = sha256Of(juce::MemoryBlock("kick", 4));
            expect(context.api->getUploadCount(kickSha) == 1, "two identical files are one upload");

            simulateDawSave(projectFile, " edit");
            context.session.requestPushVersion("second");
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
            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            context.session.requestPushVersion(juce::String::repeatedString("n", 501));
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().sessionStatus.isError(); }),
                   "a long note should fail: " + describe(context.session));
            expect(context.state().sessionStatus.text.contains("500 characters"), describe(context.session));

            for (int index = 0; index < 501; ++index)
                expect(context.environment.root.getChildFile("stem" + juce::String(index) + ".wav").replaceWithText(juce::String(index)));

            context.session.requestPushVersion("too many files");
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
            signIn(context.session);
            openProject(context.session, project.id, projectFile);

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
            expect(context.openedFiles.isEmpty() && !context.handoffFile().exists(), "nothing is handed to the DAW");
        }

        beginTest("A refused token signs the user out");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { makeBranch("branch-1", project.id, "main") };

            signIn(context.session);
            openProject(context.session, project.id, {});

            context.api->rejectToken = true;
            context.session.requestRefreshVersionHistory();
            expect(waitUntil(context.session, [&context] { return context.state().authState == AuthState::authError; }),
                   "a 401 should end the session: " + describe(context.session));
            expect(context.state().uiState == UIState::login, "back on the login screen");
            expect(context.state().authStatus.text.contains("expired"), describe(context.session));
            expect(!context.state().selectedProject.has_value() && context.state().accessToken.isEmpty(), "the session is cleared");
            expect(context.storage.credentials->loadToken().isEmpty(), "the refused token is forgotten");
        }

        beginTest("Being offline at startup keeps the saved session");
        {
            TestContext context;
            context.api->projects = { makeProject("project-1", "Song") };
            context.api->offline = true;
            context.storage.credentials->saveToken("valid-token");

            context.session.requestRestoreSavedSession();
            expect(waitUntil(context.session, [&context] { return context.state().authState == AuthState::authError; }),
                   "an unreachable server should be reported: " + describe(context.session));
            expect(context.state().authStatus.isError() && context.state().authStatus.text.contains("Can't reach StemHub"),
                   describe(context.session));
            expect(context.storage.credentials->loadToken() == "valid-token", "the saved token is kept");

            context.api->offline = false;
            context.session.requestRestoreSavedSession();
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

        beginTest("A save takes the project's audio, and leaves out backups and restored copies");
        {
            namespace snapshotfiles = stemhub::snapshotfiles;

            TestEnvironment environment;
            const auto folder = environment.root.getChildFile("Song");
            const auto projectFile = folder.getChildFile("song.flp");
            const auto write = [&folder](const juce::String& path, const juce::String& content)
            {
                const auto file = folder.getChildFile(path);
                return file.create().wasOk() && file.replaceWithText(content);
            };

            expect(write("song.flp", "flp") && write("Drums/kick.wav", "kick") && write("Loops/loop 01.aif", "loop")
                   && write("mix-deadbeef/vocal.wav", "vocal") && write("notes.txt", "notes")
                   && write("Backup/song overwritten.flp", "old") && write("backup/old kick.wav", "old kick")
                   && write(".hidden.wav", "hidden") && write("song-0123abcd/song.flp", "restored")
                   && write("song-0123abcd/Drums/kick.wav", "kick") && write("song-0123abcd (2)/song.flp", "restored 2"));

            const auto files = snapshotfiles::collect(projectFile);
            juce::StringArray paths;
            for (const auto& file : files)
                paths.add(file.getRelativePathFrom(folder).replaceCharacter('\\', '/'));

            expect(paths.joinIntoString(", ") == "song.flp, Drums/kick.wav, Loops/loop 01.aif, mix-deadbeef/vocal.wav",
                   "project file first, then its audio by path: " + paths.joinIntoString(", "));

            const auto summary = snapshotfiles::summarize(projectFile);
            expect(summary.fileCount == 4 && summary.totalBytes == 3 + 4 + 4 + 5, juce::String(summary.totalBytes));
            expect(snapshotfiles::collect(folder.getChildFile("missing.flp")).empty(), "no project file, nothing to save");

            expect(snapshotfiles::dawNameFor(projectFile) == "FL Studio");
            expect(snapshotfiles::dawNameFor(folder.getChildFile("song.als")) == "Ableton Live");
            expect(snapshotfiles::dawNameFor(folder.getChildFile("song.ptx")).isEmpty());
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

class PersistenceTests final : public StemhubTest
{
public:
    PersistenceTests() : StemhubTest("Stemhub persistence") {}

    void runTest() override
    {
        beginTest("The link saved in the DAW project reads back, and bad data is ignored");
        {
            namespace pluginstate = stemhub::pluginstate;

            const ProjectLink link { "project-1", "branch-1", juce::File::getSpecialLocation(juce::File::tempDirectory)
                                                                  .getChildFile("Song & Co").getChildFile("song \"1\".flp") };
            const auto saved = pluginstate::encode(link);
            expect(pluginstate::decode(saved.getData(), saved.getSize()) == link, saved.toString());

            expect(!pluginstate::decode(nullptr, 0).isSet(), "no state is no link");
            const juce::String garbage = "not xml at all";
            expect(!pluginstate::decode(garbage.toRawUTF8(), garbage.getNumBytesAsUTF8()).isSet(), "unreadable state is no link");
            const juce::String otherPlugin = R"(<PluginState projectId="project-1"/>)";
            expect(!pluginstate::decode(otherPlugin.toRawUTF8(), otherPlugin.getNumBytesAsUTF8()).isSet(), "another tag is no link");

            const juce::String partial = R"(<StemhubState schema="1" projectId="project-1" workingFile="relative/song.flp"/>)";
            const auto partialLink = pluginstate::decode(partial.toRawUTF8(), partial.getNumBytesAsUTF8());
            expect(partialLink.projectId == "project-1" && partialLink.branchId.isEmpty(), "missing values stay empty");
            expect(partialLink.workingFile == juce::File(), "a path that isn't absolute here is dropped");
        }

        beginTest("A restore hand-off goes to one instance of its project, and only for a while");
        {
            namespace handoff = stemhub::handoff;

            TestEnvironment environment;
            const auto location = environment.root.getChildFile("pending-restore.json");
            const auto copy = environment.root.getChildFile("song-12345678").getChildFile("song.flp");
            expect(copy.create().wasOk() && copy.replaceWithText("flp"));

            const auto now = juce::Time::getCurrentTime();
            const RestoreHandoff written { "project-1", "branch-1", WorkingCopyBaseline::recordedNow(copy, "version-1"), now };
            handoff::write(location, written);

            expect(!handoff::take(location, "project-2", now).has_value() && location.existsAsFile(),
                   "another project's instance leaves it");
            const auto taken = handoff::take(location, "project-1", now + juce::RelativeTime::minutes(9));
            expect(taken.has_value() && taken->branchId == "branch-1" && taken->copy.versionId == "version-1"
                       && taken->copy.describes(copy) && taken->copy.isUnchanged(),
                   "its instance gets the copy and what it holds");
            expect(!location.exists() && !handoff::take(location, "project-1", now).has_value(), "it is taken once");

            handoff::write(location, written);
            expect(handoff::take(location, {}, now).has_value(), "an instance without a link takes any project's");

            handoff::write(location, written);
            expect(!handoff::take(location, "project-1", now + juce::RelativeTime::minutes(11)).has_value()
                       && !location.exists(),
                   "one nobody took in time is dropped");

            handoff::write(location, written);
            expect(copy.deleteFile());
            expect(!handoff::take(location, "project-1", now).has_value() && !location.exists(),
                   "one whose copy is gone is dropped");
        }

        beginTest("The saved token is shared by instances and readable only by its user");
        {
            TestEnvironment environment;
            const auto file = environment.root.getChildFile("Stemhub").getChildFile("credentials.json");
            FileCredentialStore store(file);
            FileCredentialStore otherInstance(file);

            expect(store.loadToken().isEmpty(), "nothing saved yet");
            store.saveToken("secret-token");
            expect(otherInstance.loadToken() == "secret-token", "every instance reads the same token");

           #if ! JUCE_WINDOWS
            struct stat fileInfo {};
            struct stat folderInfo {};
            expect(::stat(file.getFullPathName().toRawUTF8(), &fileInfo) == 0
                       && (fileInfo.st_mode & 0777) == 0600,
                   "the file is private: " + juce::String::formatted("%o", static_cast<unsigned int>(fileInfo.st_mode & 0777)));
            expect(::stat(file.getParentDirectory().getFullPathName().toRawUTF8(), &folderInfo) == 0
                       && (folderInfo.st_mode & 0777) == 0700,
                   "its folder is private: " + juce::String::formatted("%o", static_cast<unsigned int>(folderInfo.st_mode & 0777)));
           #endif

            otherInstance.clear();
            expect(store.loadToken().isEmpty() && !file.exists(), "signing out forgets it everywhere");
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
PersistenceTests persistenceTests;
SnapshotTests snapshotTests;
ApiTests apiTests;
}

// JUCE's default logger writes to the debugger on Windows; CI reads the console.
class ConsoleLogger final : public juce::Logger
{
    void logMessage(const juce::String& message) override
    {
        std::cout << message << std::endl;
    }
};

int main()
{
   #if ! JUCE_WINDOWS
    // The local test server writes with send(); a client hanging up must not kill the run.
    std::signal(SIGPIPE, SIG_IGN);
   #endif

    ConsoleLogger consoleLogger;
    juce::Logger::setCurrentLogger(&consoleLogger);
    const juce::ScopeGuard restoreLogger { [] { juce::Logger::setCurrentLogger(nullptr); } };

    juce::ScopedJuceInitialiser_GUI scopedJuce;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.runAllTests();
    int passCount = 0;
    int failureCount = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (auto* result = runner.getResult(i); result != nullptr)
        {
            passCount += result->passes;
            failureCount += result->failures;
            for (const auto& message : result->messages)
                juce::Logger::writeToLog(message);
        }
    }

    juce::Logger::writeToLog(juce::String(passCount) + " checks passed, " + juce::String(failureCount) + " failed.");
    return failureCount == 0 ? 0 : 1;
}
