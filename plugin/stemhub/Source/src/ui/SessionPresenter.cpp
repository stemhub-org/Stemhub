#include "ui/SessionPresenter.hpp"

namespace
{
VersionListItem toVersionListItem(const VersionSummary& version, const juce::String& openedVersionId)
{
    VersionListItem item;
    item.id = version.id;
    item.message = version.message;
    item.isUntitled = version.message.trim().isEmpty() || version.message.trim() == kLegacyUntitledMessage;
    if (version.createdAt.isNotEmpty())
    {
        const auto parsed = juce::Time::fromISO8601(version.createdAt);
        if (parsed.toMilliseconds() > 0)
            item.createdAt = parsed;
    }
    item.sourceDaw = version.sourceDaw;
    item.sourceFilename = version.sourceProjectFilename;
    item.sizeBytes = version.totalSizeBytes;
    item.isOpenInDaw = openedVersionId.isNotEmpty() && version.id == openedVersionId;
    return item;
}

ProjectGridModel presentGrid(const SessionState& state, const SessionPresenter::FileFacts& files)
{
    ProjectGridModel grid;
    grid.projects.reserve(state.projects.size());
    for (const auto& project : state.projects)
        grid.projects.push_back({ project.id, project.name, project.description, project.category, project.isPublic });

    grid.selectedProjectId = state.selectedProject.has_value() ? state.selectedProject->id : juce::String();
    grid.status = !state.projectsStatus.isEmpty() ? state.projectsStatus
                : state.projects.empty()          ? Status::warning("No StemHub projects available for this account.")
                                                  : Status::info("Open an existing project, or choose a project file to create one.");
    grid.accountName = state.currentUser.has_value() ? state.currentUser->username : juce::String();
    grid.newProjectFilePath = files.newProjectFile != juce::File() ? files.newProjectFile.getFullPathName() : juce::String();
    grid.activity = SessionPresenter::activityFor(state.operationState);
    return grid;
}

DashboardModel presentDashboard(const SessionState& state, const SessionPresenter::FileFacts& files)
{
    DashboardModel dashboard;
    dashboard.projectName = state.selectedProject.has_value() ? state.selectedProject->name : juce::String("No project selected");

    dashboard.branches.reserve(state.branches.size());
    for (const auto& branch : state.branches)
        dashboard.branches.push_back({ branch.id, branch.name });
    dashboard.selectedBranchId = state.selectedBranchId;

    dashboard.versions.reserve(state.versionHistory.size());
    for (const auto& version : state.versionHistory)
        dashboard.versions.push_back(toVersionListItem(version, state.openedVersionId));
    dashboard.selectedVersionId = state.selectedVersionId;

    dashboard.status = state.dashboardStatus;
    dashboard.activity = SessionPresenter::activityFor(state.operationState);
    dashboard.workingFilePath = files.workingFileExists ? state.workingFile.getFullPathName() : juce::String();
    return dashboard;
}
}

SessionPresenter::SessionPresenter(juce::String savedVersionSeen)
    : lastSeenSavedVersionId(std::move(savedVersionSeen))
{
}

SessionModel SessionPresenter::present(const SessionState& state, const FileFacts& files)
{
    SessionModel model;

    // Only a save that created a version spends the message: a failed or cancelled one keeps it
    // for the retry.
    model.messageWasSaved = state.lastSavedVersionId.isNotEmpty() && state.lastSavedVersionId != lastSeenSavedVersionId;
    lastSeenSavedVersionId = state.lastSavedVersionId;

    if (!state.isSignedIn())
    {
        model.screen = Screen::login;
        model.login.status = state.authStatus;
        model.login.isSigningIn = state.operationState == OperationState::signingIn;
        return model;
    }

    if (state.uiState == UIState::projectSelection)
    {
        model.screen = Screen::projects;
        model.grid = presentGrid(state, files);
        return model;
    }

    model.screen = Screen::dashboard;
    model.dashboard = presentDashboard(state, files);
    return model;
}

SessionActivity SessionPresenter::activityFor(const OperationState operation) noexcept
{
    switch (operation)
    {
        case OperationState::signingIn:
        case OperationState::loadingProjects:
        case OperationState::loadingHistory: return SessionActivity::loading;
        case OperationState::saving:         return SessionActivity::saving;
        case OperationState::restoring:      return SessionActivity::restoring;
        case OperationState::idle:           break;
    }

    return SessionActivity::idle;
}
