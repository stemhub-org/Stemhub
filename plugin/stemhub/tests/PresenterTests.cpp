#include "support/TestSupport.hpp"
#include "ui/SessionPresenter.hpp"
#include "ui/UiFormat.hpp"

namespace
{
using namespace stemhub::test;
namespace uiformat = stemhub::uiformat;

SessionState signedInState()
{
    SessionState state;
    state.currentUser = User { "user-1", "user@example.com", "erwan" };
    state.accessToken = "token";
    return state;
}

class PresenterTests final : public StemhubTest
{
public:
    PresenterTests() : StemhubTest("Stemhub presenter") {}

    void runTest() override
    {
        beginTest("Signed out, the login screen shows how signing in went");
        {
            SessionPresenter presenter;
            SessionState state;
            state.authStatus = Status::error("Incorrect email or password.");

            auto model = presenter.present(state, {});
            expect(model.screen == Screen::login && model.login.status == state.authStatus && !model.login.isSigningIn);

            state.operationState = OperationState::signingIn;
            state.authStatus = Status::progress("Signing in to your StemHub account...");
            model = presenter.present(state, {});
            expect(model.login.isSigningIn && model.login.status == state.authStatus, "a sign-in in progress can be cancelled");
        }

        beginTest("The grid lists the projects and the file a new one would come from");
        {
            SessionPresenter presenter;
            auto state = signedInState();
            state.projects = { makeProject("p1", "Night Bus"), makeProject("p2", "Dust Signal") };
            state.projects[1].isPublic = true;
            state.selectedProject = state.projects[0];

            const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("song.flp");
            auto model = presenter.present(state, { file, false });
            expect(model.screen == Screen::projects);
            expect(model.grid.projects.size() == 2 && model.grid.projects[1].isPublic && model.grid.selectedProjectId == "p1");
            expect(model.grid.newProjectFilePath == file.getFullPathName() && model.grid.accountName == "erwan");
            expect(model.grid.status.severity == Status::Severity::info && model.grid.status.text.startsWith("Open an existing project"),
                   "a hint when the grid has nothing to say: " + model.grid.status.text);

            state.projectsStatus = Status::error("Failed to load projects.");
            expect(presenter.present(state, {}).grid.status == state.projectsStatus, "the grid's own message comes first");

            state.projectsStatus = {};
            state.projects.clear();
            model = presenter.present(state, {});
            expect(model.grid.status.severity == Status::Severity::warning, "an account without projects gets a warning");
            expect(model.grid.newProjectFilePath.isEmpty(), "no file, no path");
        }

        beginTest("The dashboard marks the version in the DAW, and untitled saves");
        {
            SessionPresenter presenter;
            auto state = signedInState();
            state.uiState = UIState::dashboard;
            state.selectedProject = makeProject("p1", "Night Bus");
            state.branches = { makeBranch("b1", "p1", "main"), makeBranch("b2", "p1", "drums") };
            state.selectedBranchId = "b2";
            // Earlier plugins saved "Save from plugin" when the user wrote nothing.
            state.versionHistory = { makeVersion("v3", "b2", "Save from plugin"),
                                     makeVersion("v2", "b2", "First sketch"),
                                     makeVersion("v1", "b2", "   ") };
            state.selectedVersionId = "v2";
            state.openedVersionId = "v2";
            state.workingFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("song.flp");

            const auto model = presenter.present(state, { {}, true });
            expect(model.screen == Screen::dashboard);

            const auto& dashboard = model.dashboard;
            expect(dashboard.projectName == "Night Bus");
            expect(dashboard.branches.size() == 2 && dashboard.branches[1].name == "drums" && dashboard.selectedBranchId == "b2");
            expect(dashboard.versions.size() == 3 && dashboard.selectedVersionId == "v2");
            expect(dashboard.versions[0].isUntitled && dashboard.versions[2].isUntitled,
                   "the message earlier plugins saved and a blank one are no titles");
            expect(!dashboard.versions[1].isUntitled && dashboard.versions[1].isOpenInDaw && !dashboard.versions[0].isOpenInDaw,
                   "only the version in the DAW is marked");
            expect(dashboard.versions[1].createdAt == juce::Time::fromISO8601("2026-03-18T10:00:00Z"), "times are read");
            expect(dashboard.workingFilePath == state.workingFile.getFullPathName());
            expect(presenter.present(state, { {}, false }).dashboard.workingFilePath.isEmpty(),
                   "a working copy that isn't there isn't shown");
        }

        beginTest("Each job shows as the activity the views wait on");
        {
            expect(SessionPresenter::activityFor(OperationState::idle) == SessionActivity::idle);
            expect(SessionPresenter::activityFor(OperationState::signingIn) == SessionActivity::loading);
            expect(SessionPresenter::activityFor(OperationState::loadingProjects) == SessionActivity::loading);
            expect(SessionPresenter::activityFor(OperationState::loadingHistory) == SessionActivity::loading);
            expect(SessionPresenter::activityFor(OperationState::saving) == SessionActivity::saving);
            expect(SessionPresenter::activityFor(OperationState::restoring) == SessionActivity::restoring);
        }

        beginTest("Only a save that created a version spends the message");
        {
            auto state = signedInState();
            state.uiState = UIState::dashboard;
            state.lastSavedVersionId = "v1";

            SessionPresenter presenter("v1");
            expect(!presenter.present(state, {}).messageWasSaved, "a save from before the window opened");

            state.dashboardStatus = Status::warning("Save cancelled.");
            expect(!presenter.present(state, {}).messageWasSaved, "a cancelled save keeps it");

            state.lastSavedVersionId = "v2";
            expect(presenter.present(state, {}).messageWasSaved, "a new version spends it");
            expect(!presenter.present(state, {}).messageWasSaved, "once");

            state = {};
            expect(!presenter.present(state, {}).messageWasSaved, "signing out spends nothing");
        }

        beginTest("Values are written the same way everywhere");
        {
            const auto now = juce::Time::fromISO8601("2026-03-18T12:00:00Z");
            expect(uiformat::relativeTime(now - juce::RelativeTime::seconds(30), now) == "Just now");
            expect(uiformat::relativeTime(now - juce::RelativeTime::minutes(5), now) == "5 min ago");
            expect(uiformat::relativeTime(now - juce::RelativeTime::hours(3), now) == "3 h ago");
            expect(uiformat::relativeTime(now - juce::RelativeTime::hours(30), now) == "Yesterday");
            expect(uiformat::relativeTime(now - juce::RelativeTime::days(4), now) == "4 days ago");
            const auto monthAgo = now - juce::RelativeTime::days(30);
            expect(uiformat::relativeTime(monthAgo, now) == monthAgo.formatted("%d %b %Y"));
            expect(uiformat::relativeTime({}, now).isEmpty() && uiformat::timestamp({}, true) == "Unknown time");

            expect(uiformat::twoDigits(3) == "03" && uiformat::twoDigits(12) == "12" && uiformat::twoDigits(123) == "123");
            expect(uiformat::slug("Night Bus / v2!") == "night-bus-v2", uiformat::slug("Night Bus / v2!"));
            expect(uiformat::slug(" ?! ").isEmpty());

            VersionListItem untitled;
            untitled.isUntitled = true;
            VersionListItem titled;
            titled.message = "  Drums  ";
            expect(uiformat::versionTitle(untitled) == "Untitled version" && uiformat::versionTitle(titled) == "Drums");

            expect(uiformat::workingCopySummary(false, 3, 100) == "No working copy");
            expect(uiformat::workingCopySummary(true, -1, 0) == "Counting files" + uiformat::ellipsis());
            expect(uiformat::workingCopySummary(true, 1, 1024) == "1 file" + uiformat::metaSeparator() + juce::File::descriptionOfSizeInBytes(1024));
            expect(uiformat::workingCopySummary(true, 3, 2048).startsWith("3 files"));

            expect(uiformat::statusChipText(Status::Severity::progress) == "Working" + uiformat::ellipsis());
            expect(uiformat::statusChipText(Status::Severity::success) == "Done");
            expect(uiformat::statusChipText(Status::Severity::warning) == "Attention");
            expect(uiformat::statusChipText(Status::Severity::error) == "Error");
            expect(uiformat::statusChipText(Status::Severity::info) == "Ready");
        }
    }
};

PresenterTests presenterTests;
}
