#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <JuceHeader.h>

#include "application/CredentialStore.hpp"
#include "application/StemhubSession.hpp"
#include "support/BlockingGate.hpp"
#include "support/FakeProjectApi.hpp"

namespace stemhub::test
{
Project makeProject(const juce::String& id, const juce::String& name);
Branch makeBranch(const juce::String& id, const juce::String& projectId, const juce::String& name);
VersionSummary makeVersion(const juce::String& id, const juce::String& branchId, const juce::String& commit);

// The session's state on one line, for failure messages.
juce::String describe(const StemhubSession& session);

// Applies finished jobs until the predicate holds. False if it still doesn't after timeoutMs.
bool waitUntil(StemhubSession& session, const std::function<bool()>& predicate, int timeoutMs = kWaitTimeoutMs);

// Waits for the running jobs to hand back this many results, whether applied or dropped.
bool waitForResults(StemhubSession& session, int count, int timeoutMs = kWaitTimeoutMs);

// Appends to a file and moves its modification time forward, like a DAW saving the project.
void simulateDawSave(const juce::File& projectFile, const juce::String& extraContent);

// A version manifest with a project file and tracks, each a (relative path, SHA-256) pair.
juce::var makeManifest(const juce::String& projectPath,
                       const juce::String& projectSha,
                       const std::vector<std::pair<juce::String, juce::String>>& tracks);

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
    [[nodiscard]] const juce::File& restoredProjectsFolder() const noexcept { return storage.restoredProjectsFolder; }

    // Declared first, so it is cleaned up after the sessions and the jobs that use its files.
    TestEnvironment environment;
    std::shared_ptr<FakeProjectApi> api { std::make_shared<FakeProjectApi>() };
    SessionStorage storage { std::make_shared<InMemoryCredentialStore>(),
                             environment.root.getChildFile("pending-restore.json"),
                             environment.root.getChildFile("Documents").getChildFile("StemHub"),
                             WorkingCopyIndex(environment.root.getChildFile("working-copies.json")) };
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

    // With projectFile as its working file, as when the file is chosen on the grid first.
    void openProject(StemhubSession& session, const juce::String& projectId, const juce::File& projectFile)
    {
        if (projectFile != juce::File())
            session.chooseProjectFile(projectFile);

        session.requestOpenProject(projectId);
        expect(waitUntil(session, [&session, projectId]
        {
            const auto& state = session.getState();
            return state.selectedProject.has_value() && state.selectedProject->id == projectId && !session.isBusy();
        }), "project should open: " + describe(session));
    }

    // Fails the test when a gated call never reached its gate.
    void expectEntered(BlockingGate& gate, const juce::String& call)
    {
        expect(gate.waitUntilEntered(), call + " should reach its gate");
    }
};
}
