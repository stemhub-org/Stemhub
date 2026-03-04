#include <map>

#include "../include/PluginEditor.hpp"

namespace
{
template<typename Map, typename Key>
juce::String findMappedMessage(const Map& messageMap,
                               const Key& key,
                               const juce::String& defaultMessage = {})
{
    const auto it = messageMap.find(key);
    return it != messageMap.end() ? it->second : defaultMessage;
}

const std::map<AuthState, juce::String> authMessages {
    { AuthState::signingIn, "Signing in..." },
    { AuthState::signedOut, "Please sign in to your Stemhub account to access your projects." },
    { AuthState::authError, "An error occurred during authentication. Please try again." },
};

const std::map<UIState, juce::String> signedInMessages {
    { UIState::login, "Please sign in to your Stemhub account to access your projects." },
    { UIState::projectSelection, "Choose an existing project or create a new one." },
    { UIState::commit, "Commit view" },
    { UIState::history, "History and sync view" },
    { UIState::settings, "Branch management view" },
};

const std::map<OperationState, juce::String> operationMessages {
    { OperationState::loadingProjects, "Loading projects..." },
    { OperationState::committing, "Committing..." },
    { OperationState::pulling, "Syncing..." },
    { OperationState::error, "An operation error occurred." },
};
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
    const auto message = buildStatusMessage();

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

        projectSelectionView.setHasExistingProjects(!projects.empty());
        projectSelectionView.setMessage(message);
        projectSelectionView.setSelectedProjectFileMessage(audioProcessor.getPendingProjectFile().existsAsFile()
            ? audioProcessor.getPendingProjectFile().getFullPathName()
            : "No project file selected.");
        projectSelectionView.setProjects(projectNames,
                                         projectIds,
                                         audioProcessor.getSelectedProject() ? audioProcessor.getSelectedProject()->id : juce::String());
    }
    else if (showDashboard) {
        dashboardView.setProjectStatusMessage(audioProcessor.getProjectStatusMessage());
        dashboardView.setCurrentProjectMessage(audioProcessor.getSelectedProject()
            ? "Project: " + audioProcessor.getSelectedProject()->name + " | Branch: " + audioProcessor.getSelectedBranchName()
            : "No project selected.");
        dashboardView.setSelectedProjectFileMessage(audioProcessor.getSelectedProjectFile().existsAsFile()
            ? audioProcessor.getSelectedProjectFile().getFullPathName()
            : "No project file selected.");
    } else {
        loginView.setMessage(message);
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
    const auto pendingFile = audioProcessor.getPendingProjectFile();
    if (!pendingFile.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Open project",
            "Choose the local DAW project file before continuing.");
        return;
    }

    const auto projectId = projectSelectionView.getSelectedProjectId();
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
    audioProcessor.setUIState(UIState::history);
    audioProcessor.setOperationState(OperationState::idle);
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleChangeBranchClick()
{
    audioProcessor.setUIState(UIState::settings);
    audioProcessor.setOperationState(OperationState::idle);
    refreshSessionUi();
}

juce::String StemhubAudioProcessorEditor::buildStatusMessage() const
{
    const auto authState = audioProcessor.getAuthState();
    const auto uiState = audioProcessor.getUIState();
    const auto operationState = audioProcessor.getOperationState();

    juce::String message;

    if (authState == AuthState::signedIn
        && uiState == UIState::projectSelection
        && audioProcessor.getProjects().empty())
        message = "Choose a DAW file to create your first project.";
    else if (authState == AuthState::signedIn && uiState == UIState::dashboard)
    {
        message = "Welcome back " + audioProcessor.getUsername() + "!";

        if (audioProcessor.getProjectStatusMessage().isNotEmpty())
            message << "\n" << audioProcessor.getProjectStatusMessage();
    }
    else if (authState == AuthState::signedIn)
        message = findMappedMessage(signedInMessages, uiState, "Welcome back " + audioProcessor.getUsername() + "!");
    else if (authState == AuthState::authError && audioProcessor.getAuthErrorMessage().isNotEmpty())
        message = audioProcessor.getAuthErrorMessage();
    else
        message = findMappedMessage(authMessages, authState);

    const auto operationSuffix = findMappedMessage(operationMessages, operationState);
    if (!operationSuffix.isEmpty())
        message << "\n" << operationSuffix;

    return message;
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
