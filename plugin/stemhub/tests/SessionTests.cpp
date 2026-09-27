#include <algorithm>
#include <atomic>
#include <memory>

#include "support/TestSupport.hpp"

namespace
{
using namespace stemhub::test;

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
            context.session.requestOpenProject(projectA.id);
            expectEntered(*openGate, "opening project A");
            context.session.requestOpenProject(projectB.id);
            expect(context.state().operationState == OperationState::loadingProjects, describe(context.session));

            openGate->release();
            expect(waitUntil(context.session, [&context] { return context.isIdle(); }), "the first open should finish");
            expect(openGate->waitUntilFinished(), "opening project A should go through");
            expect(context.state().selectedProject.has_value() && context.state().selectedProject->id == projectA.id,
                   "the open requested meanwhile was ignored: " + describe(context.session));

            auto historyGate = std::make_shared<BlockingGate>();
            context.api->versionFetchGates[branchAlt.id] = historyGate;
            context.session.requestSelectBranch(branchAlt.id);
            expectEntered(*historyGate, "the branch switch");
            context.session.requestSelectBranch(branchMain.id);
            context.session.requestRefreshVersionHistory();
            context.session.requestOpenProject(projectB.id);
            expect(context.state().operationState == OperationState::pulling, describe(context.session));

            historyGate->release();
            expect(waitUntil(context.session, [&context] { return context.isIdle(); }), "the branch switch should finish");
            expect(historyGate->waitUntilFinished(), "the branch switch should go through");
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
            expectEntered(*gate, "the save");

            context.session.signOut();
            signIn(context.session);
            openProject(context.session, projectB.id, {});

            // Signing out asked the save to stop; whatever it ends with belongs to the old session.
            gate->release();
            expect(waitForResults(context.session, 1), "the save should finish");
            expect(context.api->getCreatedVersions().size() == 1, "the old save created nothing");
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
            expectEntered(*gate, "the save");
            context.session.signOut();
            gate->release();

            expect(waitForResults(context.session, 1), "the save job should finish");
            expect(context.api->getCreatedVersions().empty(), "signing out stopped the save before it created a version");
            expect(context.state().authState == AuthState::signedOut, describe(context.session));
            expect(!context.state().selectedProject.has_value(), "no project after signing out");
            expect(context.state().versionHistory.empty(), "the save's history is dropped");
        }

        beginTest("Closing the plugin stops a running transfer instead of waiting for it");
        {
            auto gate = std::make_shared<BlockingGate>(true);
            juce::uint32 closingTookMs = 0;
            {
                TestContext context;
                const auto project = makeProject("project-1", "Song");
                const auto branch = makeBranch("branch-1", project.id, "main");
                context.api->projects = { project };
                context.api->projectBranches[project.id] = { branch };
                const auto versionId = context.api->addVersion(branch.id, "first", { { "song.flp", "flp" } });
                signIn(context.session);
                openProject(context.session, project.id, {});

                context.api->setDownloadGate(project.id, gate);
                context.session.requestRestoreVersion(versionId, context.environment.root);
                expectEntered(*gate, "the download");

                const auto closingStarted = juce::Time::getMillisecondCounter();
                context.session.shutdown();
                closingTookMs = juce::Time::getMillisecondCounter() - closingStarted;
            }

            // The gate would hold the download for BlockingGate::kFallbackMs if nothing asked it to stop.
            expect(closingTookMs < 1000, "closing took " + juce::String(closingTookMs) + " ms");
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

                context.session.requestOpenProject(project.id);
                expectEntered(*gate, "opening the project");

                // Released while the session is being destroyed.
                juce::Thread::launch([gate]
                {
                    juce::Thread::sleep(100);
                    gate->release();
                });
            }

            expect(jobReturned->load(), "the session must not be destroyed while its job still runs");
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

        beginTest("Creating a project keeps the project list when a later step fails");
        {
            TestContext context;
            context.api->projects = { makeProject("project-1", "Existing") };
            const auto projectFile = context.environment.root.getChildFile("new song.flp");
            expect(projectFile.replaceWithText("flp"));
            signIn(context.session);

            // The list can't be reloaded once the project is created.
            context.api->failProjectList = true;
            context.session.chooseProjectFile(projectFile);
            context.session.requestCreateProject();
            expect(waitUntil(context.session, [&context] { return context.isIdle(); }), describe(context.session));
            expect(context.state().selectedProject.has_value() && context.state().selectedProject->id == "created-1",
                   "the new project opens: " + describe(context.session));
            expect(context.state().projects.size() == 2,
                   "the grid keeps its projects and shows the new one: " + juce::String(static_cast<int>(context.state().projects.size())));

            // The project is created, but its workspaces can't be loaded.
            context.api->failProjectList = false;
            context.api->branchErrors["created-2"] = "Failed to load workspaces.";
            context.session.showProjectSelection();
            context.session.chooseProjectFile(projectFile);
            context.session.requestCreateProject();
            expect(waitUntil(context.session, [&context] { return context.isIdle(); }), describe(context.session));
            expect(context.state().projectsStatus.isError() && context.state().projectsStatus.text.contains("was created"),
                   "the error says the project exists: " + describe(context.session));
            const auto& projects = context.state().projects;
            expect(std::any_of(projects.begin(), projects.end(), [](const Project& project) { return project.id == "created-2"; }),
                   "so the grid lists it, and it isn't created twice");
        }

        beginTest("Opening another project from the grid doesn't take this project's file");
        {
            TestContext context;
            const auto projectA = makeProject("project-a", "Song A");
            const auto projectB = makeProject("project-b", "Song B");
            context.api->projects = { projectA, projectB };
            context.api->projectBranches[projectA.id] = { makeBranch("branch-a", projectA.id, "main") };
            context.api->projectBranches[projectB.id] = { makeBranch("branch-b", projectB.id, "main") };
            const auto fileA = context.environment.root.getChildFile("a.flp");
            const auto movedFileA = context.environment.root.getChildFile("moved").getChildFile("a.flp");
            expect(fileA.replaceWithText("a") && movedFileA.create().wasOk() && movedFileA.replaceWithText("a"));

            signIn(context.session);
            openProject(context.session, projectA.id, fileA);
            // Saving A, the user picks its file from another folder, as the save dialog lets them.
            context.session.setPendingProjectFile(movedFileA);
            context.session.requestPushVersion("from A");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().workingCopy.isSet(); }),
                   describe(context.session));

            context.session.showProjectSelection();
            openFromGrid(context.session, projectB.id);
            expect(waitUntil(context.session, [&context, &projectB]
            {
                return context.isIdle() && context.state().selectedProject.has_value() && context.state().selectedProject->id == projectB.id;
            }), describe(context.session));
            expect(context.session.getEffectiveProjectFile() == juce::File(),
                   "B has no local file yet: " + context.session.getEffectiveProjectFile().getFullPathName());
            expect(context.state().link.workingFile == juce::File(), "the DAW project isn't linked to A's file");
            expect(!context.state().workingCopy.isSet(), "B doesn't take A's version as its base");
        }

        beginTest("Reopening the linked project from the grid opens its branch");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branchMain = makeBranch("branch-main", project.id, "main");
            const auto branchAlt = makeBranch("branch-alt", project.id, "alt");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branchMain, branchAlt };
            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp"));
            context.storage.credentials->saveToken("valid-token");

            context.session.restoreLink({ project.id, branchAlt.id, projectFile });
            restoreSavedSession(context.session);
            expect(context.state().selectedBranchId == branchAlt.id, describe(context.session));

            context.session.showProjectSelection();
            openFromGrid(context.session, project.id);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().uiState == UIState::dashboard; }),
                   describe(context.session));
            expect(context.state().selectedBranchId == branchAlt.id, "the branch it was on, not main: " + describe(context.session));
            expect(context.session.getEffectiveProjectFile() == projectFile, "with its file");
        }
    }

private:
    // As the project grid does when a tile is clicked.
    static void openFromGrid(StemhubSession& session, const juce::String& projectId)
    {
        session.requestOpenProject(projectId, true);
    }
};

SessionTests sessionTests;
}
