#include <map>

#include "../include/PluginEditor.hpp"

namespace
{
juce::String getLoginMessage(const StemhubAudioProcessor& processor)
{
    if (processor.getAuthState() == AuthState::signingIn)
        return "Signing in...";

    if (processor.getAuthState() == AuthState::authError && processor.getAuthErrorMessage().isNotEmpty())
        return processor.getAuthErrorMessage();

    return "Please sign in to your Stemhub account to access your projects.";
}

juce::String getProjectSelectionMessage(const StemhubAudioProcessor& processor)
{
    if (processor.getOperationState() == OperationState::loadingProjects)
        return "Loading projects...";

    if (processor.getProjectSelectionStatusMessage().isNotEmpty())
        return processor.getProjectSelectionStatusMessage();

    if (processor.getProjects().empty())
        return "Choose a DAW file to create your first project.";

    return "Choose an existing project or create a new one.";
}

juce::String getDashboardMessage(const StemhubAudioProcessor& processor)
{
    if (processor.getOperationState() == OperationState::committing)
        return "Committing...";

    if (processor.getOperationState() == OperationState::pulling)
        return "Refreshing version history...";

    if (processor.getActiveProjectStatusMessage().isNotEmpty())
        return processor.getActiveProjectStatusMessage();

    return "Project ready.";
}

juce::String formatVersionLabel(const VersionSummary& version)
{
    const auto shortId = version.id.substring(0, 8);
    const auto message = version.commitMessage.isNotEmpty() ? version.commitMessage : "No commit message";
    return shortId + " - " + message;
}
}

StemhubAudioProcessorEditor::StemhubAudioProcessorEditor(StemhubAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), audioProcessor(processorToEdit)
{
    setSize(600, 400);
    audioProcessor.addChangeListener(this);

    addAndMakeVisible(loginView);
    addAndMakeVisible(projectSelectionView);
    addAndMakeVisible(dashboardView);

    loginView.onSignIn = [this] { handleSignInClick(); };
    projectSelectionView.onChooseProjectFile = [this] { handleChooseProjectFileClick(); };
    projectSelectionView.onOpenProject = [this] { handleOpenProjectClick(); };
    projectSelectionView.onCreateProject = [this] { handleCreateProjectClick(); };
    projectSelectionView.onSignOut = [this] { handleSignOutClick(); };
    dashboardView.onSave = [this] { handleSaveChangesClick(); };
    dashboardView.onSync = [this] { handleSyncClick(); };
    dashboardView.onBranchChange = [this] { handleChangeBranchClick(); };
    dashboardView.onVersionSelectionChange = [this] { handleVersionSelectionChanged(); };
    dashboardView.onSignOut = [this] { handleSignOutClick(); };

    refreshSessionUi();
}

StemhubAudioProcessorEditor::~StemhubAudioProcessorEditor()
{
    audioProcessor.removeChangeListener(this);
}

void StemhubAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &audioProcessor)
        refreshSessionUi();
}

void StemhubAudioProcessorEditor::refreshSessionUi()
{
    const bool isSignedIn = audioProcessor.getAuthState() == AuthState::signedIn;
    const bool showProjectSelection = isSignedIn && audioProcessor.getUIState() == UIState::projectSelection;
    const bool showDashboard = isSignedIn && !showProjectSelection;

    loginView.setVisible(!isSignedIn);
    projectSelectionView.setVisible(showProjectSelection);
    dashboardView.setVisible(showDashboard);

    if (showProjectSelection)
    {
        std::vector<juce::String> projectNames;
        std::vector<juce::String> projectIds;
        const auto& projects = audioProcessor.getProjects();
        projectNames.reserve(projects.size());
        projectIds.reserve(projects.size());

        for (const auto& project : projects)
        {
            projectNames.push_back(project.name);
            projectIds.push_back(project.id);
        }

        const auto hasSelectedProjectFile = audioProcessor.getPendingProjectFile().existsAsFile();
        projectSelectionView.setHasExistingProjects(!projects.empty());
        projectSelectionView.setCanCreateProject(hasSelectedProjectFile);
        projectSelectionView.setMessage(getProjectSelectionMessage(audioProcessor));
        projectSelectionView.setSelectedProjectFileMessage(hasSelectedProjectFile
            ? audioProcessor.getPendingProjectFile().getFullPathName()
            : "No project file selected.");
        projectSelectionView.setProjects(projectNames,
                                         projectIds,
                                         audioProcessor.getSelectedProject() ? audioProcessor.getSelectedProject()->id : juce::String());
    }
    else if (showDashboard) {
        std::vector<juce::String> branchNames;
        std::vector<juce::String> branchIds;
        const auto& branches = audioProcessor.getBranches();
        branchNames.reserve(branches.size());
        branchIds.reserve(branches.size());

        for (const auto& branch : branches)
        {
            branchNames.push_back(branch.name);
            branchIds.push_back(branch.id);
        }

        std::vector<juce::String> versionLabels;
        std::vector<juce::String> versionIds;
        const auto& versions = audioProcessor.getVersionHistory();
        versionLabels.reserve(versions.size());
        versionIds.reserve(versions.size());

        for (const auto& version : versions)
        {
            versionLabels.push_back(formatVersionLabel(version));
            versionIds.push_back(version.id);
        }

        dashboardView.setProjectStatusMessage(getDashboardMessage(audioProcessor));
        dashboardView.setBranches(branchNames, branchIds, audioProcessor.getSelectedBranchId());
        dashboardView.setVersions(versionLabels, versionIds, audioProcessor.getSelectedVersionId());
        dashboardView.setCurrentProjectMessage(audioProcessor.getSelectedProject()
            ? "Project: " + audioProcessor.getSelectedProject()->name + " | Branch: " + audioProcessor.getSelectedBranchName()
            : "No project selected.");
        dashboardView.setSelectedProjectFileMessage(audioProcessor.getSelectedProjectFile().existsAsFile()
            ? audioProcessor.getSelectedProjectFile().getFullPathName()
            : "No project file selected.");
    } else {
        loginView.setMessage(getLoginMessage(audioProcessor));
    }

    resized();
    repaint();
}

void StemhubAudioProcessorEditor::handleChooseProjectFileClick()
{
    projectFileChooser = std::make_unique<juce::FileChooser>(
        "Select a DAW project file",
        audioProcessor.getPendingProjectFile(),
        "*.flp;*.als");

    constexpr auto flags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles;

    projectFileChooser->launchAsync(flags, [this](const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();

        if (file.existsAsFile())
            audioProcessor.setPendingProjectFile(file);

        projectFileChooser.reset();
    });
}

void StemhubAudioProcessorEditor::handleOpenProjectClick()
{
    const auto projectId = projectSelectionView.getSelectedProjectId();
    if (projectId.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Open project",
            "Choose an existing project before continuing.");
        return;
    }

    const auto pendingFile = audioProcessor.getPendingProjectFile();
    audioProcessor.requestOpenProject(projectId, pendingFile);
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleCreateProjectClick()
{
    const auto selectedFile = audioProcessor.getPendingProjectFile();
    if (!selectedFile.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Create project",
            "Choose a project file first.");
        return;
    }

    audioProcessor.requestCreateProject(selectedFile);
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleSignInClick()
{
    const auto email = loginView.getEmail().trim();
    const auto password = loginView.getPassword();

    if (email.isEmpty() || password.isEmpty())
    {
        loginView.setMessage("Please enter both email and password.");
        return;
    }
    audioProcessor.requestSignIn(email, password);
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleSignOutClick()
{
    audioProcessor.signOut();
    loginView.clearInputs();
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleSaveChangesClick()
{
    if (!audioProcessor.getSelectedProject() || audioProcessor.getSelectedBranchId().isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Push failed",
            "Choose or create a project before saving.");
        refreshSessionUi();
        return;
    }

    if (!audioProcessor.getSelectedProjectFile().existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Push failed",
            "Choose a project file before saving.");
        refreshSessionUi();
        return;
    }

    audioProcessor.requestPushVersion("Save from plugin", "FL Studio");
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleSyncClick()
{
    audioProcessor.requestRefreshVersionHistory();
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleChangeBranchClick()
{
    const auto selectedBranchId = dashboardView.getSelectedBranchId();
    if (selectedBranchId.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Branch selection",
            "Select a branch before loading history.");
        return;
    }

    audioProcessor.requestSelectBranch(selectedBranchId);
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleVersionSelectionChanged()
{
    const auto selectedVersionId = dashboardView.getSelectedVersionId();
    audioProcessor.setSelectedVersionId(selectedVersionId);
}

void StemhubAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void StemhubAudioProcessorEditor::resized()
{
    const auto area = getLocalBounds();
    loginView.setBounds(area);
    projectSelectionView.setBounds(area);
    dashboardView.setBounds(area);
}
