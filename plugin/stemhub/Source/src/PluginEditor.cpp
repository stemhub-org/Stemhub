<<<<<<< HEAD
#include <map>

=======
>>>>>>> origin/dev
#include "../include/PluginEditor.hpp"

namespace
{
<<<<<<< HEAD
=======
constexpr auto kDefaultCommitMessage = "Save from plugin";
constexpr auto kDawName = "FL Studio";
constexpr auto kProjectFilePattern = "*.flp;*.als";

>>>>>>> origin/dev
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
<<<<<<< HEAD
        return "Syncing...";
=======
        return "Refreshing version history...";
>>>>>>> origin/dev

    if (processor.getActiveProjectStatusMessage().isNotEmpty())
        return processor.getActiveProjectStatusMessage();

    return "Project ready.";
}
<<<<<<< HEAD
=======

juce::String formatVersionLabel(const VersionSummary& version)
{
    const auto shortId = version.id.substring(0, 8);
    const auto message = version.commitMessage.isNotEmpty() ? version.commitMessage : "No commit message";

    juce::String timestamp = "Unknown time";
    if (version.createdAt.isNotEmpty())
    {
        const auto parsed = juce::Time::fromISO8601(version.createdAt);
        if (parsed.toMilliseconds() > 0)
            timestamp = parsed.toString(true, true, true, true);
        else
            timestamp = version.createdAt;
    }

    return shortId + " - " + message + " (" + timestamp + ")";
}

const auto kStemhubDark = juce::Colour::fromRGB(0x1E, 0x1E, 0x1E);
const auto kStemhubPurple = juce::Colour::fromRGB(0x9C, 0x57, 0xDF);
>>>>>>> origin/dev
}

StemhubAudioProcessorEditor::StemhubAudioProcessorEditor(StemhubAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), audioProcessor(processorToEdit)
{
<<<<<<< HEAD
=======
    juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName("Syne");
    setWantsKeyboardFocus(true);
    addKeyListener(this);

>>>>>>> origin/dev
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
<<<<<<< HEAD
=======
    dashboardView.onVersionSelectionChange = [this] { handleVersionSelectionChanged(); };
    dashboardView.onBackToProjects = [this] { handleBackToProjectsClick(); };
>>>>>>> origin/dev
    dashboardView.onSignOut = [this] { handleSignOutClick(); };

    refreshSessionUi();
}

StemhubAudioProcessorEditor::~StemhubAudioProcessorEditor()
{
<<<<<<< HEAD
=======
    removeKeyListener(this);
>>>>>>> origin/dev
    audioProcessor.removeChangeListener(this);
}

void StemhubAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &audioProcessor)
        refreshSessionUi();
}

<<<<<<< HEAD
=======
bool StemhubAudioProcessorEditor::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused(originatingComponent);

    const auto isSaveShortcut = key.getModifiers().isCommandDown()
        && (key.getTextCharacter() == 's' || key.getTextCharacter() == 'S');

    if (!isSaveShortcut || !dashboardView.isVisible())
        return false;

    showCommitMessagePopupForSave();
    return true;
}

>>>>>>> origin/dev
void StemhubAudioProcessorEditor::refreshSessionUi()
{
    const bool isSignedIn = audioProcessor.getAuthState() == AuthState::signedIn;
    const bool showProjectSelection = isSignedIn && audioProcessor.getUIState() == UIState::projectSelection;
    const bool showDashboard = isSignedIn && !showProjectSelection;

    loginView.setVisible(!isSignedIn);
    projectSelectionView.setVisible(showProjectSelection);
    dashboardView.setVisible(showDashboard);

    if (showProjectSelection)
<<<<<<< HEAD
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
        projectSelectionView.setMessage(getProjectSelectionMessage(audioProcessor));
        projectSelectionView.setSelectedProjectFileMessage(audioProcessor.getPendingProjectFile().existsAsFile()
            ? audioProcessor.getPendingProjectFile().getFullPathName()
            : "No project file selected.");
        projectSelectionView.setProjects(projectNames,
                                         projectIds,
                                         audioProcessor.getSelectedProject() ? audioProcessor.getSelectedProject()->id : juce::String());
    }
    else if (showDashboard) {
        dashboardView.setProjectStatusMessage(getDashboardMessage(audioProcessor));
        dashboardView.setCurrentProjectMessage(audioProcessor.getSelectedProject()
            ? "Project: " + audioProcessor.getSelectedProject()->name + " | Branch: " + audioProcessor.getSelectedBranchName()
            : "No project selected.");
        dashboardView.setSelectedProjectFileMessage(audioProcessor.getSelectedProjectFile().existsAsFile()
            ? audioProcessor.getSelectedProjectFile().getFullPathName()
            : "No project file selected.");
    } else {
        loginView.setMessage(getLoginMessage(audioProcessor));
    }
=======
        refreshProjectSelectionUi();
    else if (showDashboard)
        refreshDashboardUi();
    else
        loginView.setMessage(getLoginMessage(audioProcessor));
>>>>>>> origin/dev

    resized();
    repaint();
}

<<<<<<< HEAD
void StemhubAudioProcessorEditor::handleChooseProjectFileClick()
{
    projectFileChooser = std::make_unique<juce::FileChooser>(
        "Select a DAW project file",
        audioProcessor.getPendingProjectFile(),
        "*.flp;*.als");
=======
void StemhubAudioProcessorEditor::refreshProjectSelectionUi()
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

void StemhubAudioProcessorEditor::refreshDashboardUi()
{
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
    dashboardView.setProjectNameMessage(audioProcessor.getSelectedProject()
        ? "Project: " + audioProcessor.getSelectedProject()->name
        : "Project: No project selected");
    dashboardView.setBranchNameMessage(audioProcessor.getSelectedBranchName().isNotEmpty()
        ? "Branch: " + audioProcessor.getSelectedBranchName()
        : "Branch: Not selected");

    const auto fileToDisplay = getEffectiveProjectFile();
    dashboardView.setSelectedProjectFileMessage(fileToDisplay.existsAsFile()
        ? fileToDisplay.getFullPathName()
        : "No project file selected.");
}

void StemhubAudioProcessorEditor::handleChooseProjectFileClick()
{
    launchProjectFileChooser("Select a DAW project file", [this](const juce::File& file)
    {
        audioProcessor.setPendingProjectFile(file);
    });
}

void StemhubAudioProcessorEditor::launchProjectFileChooser(const juce::String& title,
                                                           std::function<void(const juce::File&)> onFileChosen)
{
    projectFileChooser = std::make_unique<juce::FileChooser>(
        title,
        audioProcessor.getPendingProjectFile(),
        kProjectFilePattern);
>>>>>>> origin/dev

    constexpr auto flags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles;

<<<<<<< HEAD
    projectFileChooser->launchAsync(flags, [this](const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();

        if (file.existsAsFile())
            audioProcessor.setPendingProjectFile(file);
=======
    projectFileChooser->launchAsync(flags, [this, fileChosenCallback = std::move(onFileChosen)](const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (file.existsAsFile() && fileChosenCallback != nullptr)
            fileChosenCallback(file);
>>>>>>> origin/dev

        projectFileChooser.reset();
    });
}

void StemhubAudioProcessorEditor::handleOpenProjectClick()
{
<<<<<<< HEAD
    const auto pendingFile = audioProcessor.getPendingProjectFile();
    if (!pendingFile.existsAsFile())
=======
    const auto projectId = projectSelectionView.getSelectedProjectId();
    if (projectId.isEmpty())
>>>>>>> origin/dev
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Open project",
<<<<<<< HEAD
            "Choose the local DAW project file before continuing.");
        return;
    }

    const auto projectId = projectSelectionView.getSelectedProjectId();
=======
            "Choose an existing project before continuing.");
        return;
    }

    const auto pendingFile = audioProcessor.getPendingProjectFile();
>>>>>>> origin/dev
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
<<<<<<< HEAD
    if (!audioProcessor.getSelectedProject() || audioProcessor.getSelectedBranchId().isEmpty())
=======
    requestSaveWithCommitMessage(dashboardView.getCommitMessage());
}

void StemhubAudioProcessorEditor::requestSaveWithCommitMessage(juce::String commitMessage)
{
    const auto trimmedCommitMessage = commitMessage.trim();
    dashboardView.setCommitMessage(trimmedCommitMessage);

    if (!hasActiveProjectSelection())
>>>>>>> origin/dev
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Push failed",
            "Choose or create a project before saving.");
        refreshSessionUi();
        return;
    }

<<<<<<< HEAD
    if (!audioProcessor.getSelectedProjectFile().existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Push failed",
            "Choose a project file before saving.");
=======
    const auto effectiveCommitMessage = trimmedCommitMessage.isNotEmpty()
        ? trimmedCommitMessage
        : juce::String(kDefaultCommitMessage);

    if (!getEffectiveProjectFile().existsAsFile())
    {
        launchProjectFileChooser("Select a DAW project file before saving", [this, effectiveCommitMessage](const juce::File& file)
        {
            audioProcessor.setPendingProjectFile(file);
            triggerPushVersion(effectiveCommitMessage);
        });

>>>>>>> origin/dev
        refreshSessionUi();
        return;
    }

<<<<<<< HEAD
    audioProcessor.requestPushVersion("Save from plugin", "FL Studio");
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleSyncClick()
{
    audioProcessor.setOperationState(OperationState::idle);
    audioProcessor.setActiveProjectStatusMessage("Sync is not implemented yet.");
=======
    triggerPushVersion(effectiveCommitMessage);
}

void StemhubAudioProcessorEditor::triggerPushVersion(const juce::String& commitMessage)
{
    audioProcessor.requestPushVersion(commitMessage, kDawName);
    refreshSessionUi();
}

bool StemhubAudioProcessorEditor::hasActiveProjectSelection() const
{
    return audioProcessor.getSelectedProject().has_value() && audioProcessor.getSelectedBranchId().isNotEmpty();
}

juce::File StemhubAudioProcessorEditor::getEffectiveProjectFile() const
{
    const auto selectedProjectFile = audioProcessor.getSelectedProjectFile();
    if (selectedProjectFile.existsAsFile())
        return selectedProjectFile;

    return audioProcessor.getPendingProjectFile();
}

void StemhubAudioProcessorEditor::handleSyncClick()
{
    audioProcessor.requestRefreshVersionHistory();
>>>>>>> origin/dev
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleChangeBranchClick()
{
<<<<<<< HEAD
    audioProcessor.setOperationState(OperationState::idle);
    audioProcessor.setActiveProjectStatusMessage("Branch management is not implemented yet.");
=======
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

void StemhubAudioProcessorEditor::showCommitMessagePopupForSave()
{
    auto* commitPopup = new juce::AlertWindow("Save version",
                                              "Enter a commit message before saving.",
                                              juce::AlertWindow::NoIcon);

    commitPopup->addTextEditor("commit_message", dashboardView.getCommitMessage(), "Commit message");
    commitPopup->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    commitPopup->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    const auto popupRef = juce::Component::SafePointer<juce::AlertWindow>(commitPopup);
    const auto editorRef = juce::Component::SafePointer<StemhubAudioProcessorEditor>(this);
    commitPopup->enterModalState(true, juce::ModalCallbackFunction::create([editorRef, popupRef](int result)
    {
        if (result != 1 || popupRef == nullptr || editorRef == nullptr)
            return;

        const auto commitMessage = popupRef->getTextEditorContents("commit_message").trim();
        editorRef->dashboardView.setCommitMessage(commitMessage);
        editorRef->requestSaveWithCommitMessage(commitMessage);
    }), true);
}

void StemhubAudioProcessorEditor::handleBackToProjectsClick()
{
    audioProcessor.setOperationState(OperationState::idle);
    audioProcessor.setUIState(UIState::projectSelection);
>>>>>>> origin/dev
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::paint(juce::Graphics& g)
{
<<<<<<< HEAD
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
=======
    g.fillAll(kStemhubDark);

    juce::ColourGradient topGlow(kStemhubPurple.withAlpha(0.22f),
                                 static_cast<float>(getWidth()) * 0.52f,
                                 static_cast<float>(getHeight()) * 0.08f,
                                 kStemhubDark,
                                 static_cast<float>(getWidth()) * 0.5f,
                                 static_cast<float>(getHeight()) * 0.7f,
                                 true);
    g.setGradientFill(topGlow);
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(6.0f), 10.0f);
>>>>>>> origin/dev
}

void StemhubAudioProcessorEditor::resized()
{
    const auto area = getLocalBounds();
    loginView.setBounds(area);
    projectSelectionView.setBounds(area);
    dashboardView.setBounds(area);
}
