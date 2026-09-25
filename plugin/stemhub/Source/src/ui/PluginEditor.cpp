#include "ui/PluginEditor.hpp"

namespace
{
constexpr auto kProjectFilePattern = "*.flp;*.als";

namespace theme = stemhub::plugin::theme;

theme::MessageStatus toMessageStatus(Status::Severity severity)
{
    switch (severity)
    {
        case Status::Severity::progress: return theme::MessageStatus::loading;
        case Status::Severity::success:  return theme::MessageStatus::success;
        case Status::Severity::warning:  return theme::MessageStatus::warning;
        case Status::Severity::error:    return theme::MessageStatus::error;
        case Status::Severity::info:     break;
    }

    return theme::MessageStatus::neutral;
}

SessionActivity toSessionActivity(OperationState operation)
{
    switch (operation)
    {
        case OperationState::loadingProjects:
        case OperationState::pulling:    return SessionActivity::loading;
        case OperationState::committing: return SessionActivity::saving;
        case OperationState::restoring:  return SessionActivity::restoring;
        case OperationState::idle:       break;
    }

    return SessionActivity::idle;
}

// The grid's own message, or a hint about what to do next.
Status projectGridStatus(const SessionState& state)
{
    if (!state.projectsStatus.isEmpty())
        return state.projectsStatus;

    if (state.projects.empty())
        return Status::warning("No StemHub projects available for this account.");

    return Status::info("Open an existing project, or choose a local file to create one.");
}

// Alerts are attached to the editor so they use its LookAndFeel and close with it.
void showWarning(juce::Component& owner, const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                                     .withIconType(juce::MessageBoxIconType::WarningIcon)
                                     .withTitle(title)
                                     .withMessage(message)
                                     .withButton("OK")
                                     .withAssociatedComponent(&owner),
                                 nullptr);
}

VersionListItem toVersionListItem(const VersionSummary& version, const juce::String& openedVersionId)
{
    VersionListItem item;
    item.id = version.id;
    item.message = version.commitMessage;
    item.isUntitled = version.commitMessage.trim().isEmpty() || version.commitMessage.trim() == kDefaultSaveNote;
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

}

StemhubAudioProcessorEditor::StemhubAudioProcessorEditor(juce::AudioProcessor& ownerProcessor, StemhubSession& sessionToShow)
    : AudioProcessorEditor(&ownerProcessor), session(sessionToShow)
{
    // Scoped to this editor: the process-wide default is shared by every plugin instance.
    setLookAndFeel(&pluginLookAndFeel);
    setSize(720, 560);
    setWantsKeyboardFocus(true);
    setOpaque(true);
    addKeyListener(this);

    session.addChangeListener(this);

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
    dashboardView.onBackToProjects = [this] { handleBackToProjectsClick(); };
    dashboardView.onSignOut = [this] { handleSignOutClick(); };
    dashboardView.onRestore = [this] { handleRestoreClick(); };
    dashboardView.onCancel = [this] { session.cancelRequest(); };
    dashboardView.setMaxNoteLength(kMaxSaveNoteLength);

    session.requestRestoreSavedSession();
    refreshSessionUi();
}

StemhubAudioProcessorEditor::~StemhubAudioProcessorEditor()
{
    snapshotCounter.shutdown();
    cancelPendingUpdate();
    removeKeyListener(this);
    session.removeChangeListener(this);
    commitPopup.reset();
    setLookAndFeel(nullptr);
}

void StemhubAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &session)
        refreshSessionUi();
}

bool StemhubAudioProcessorEditor::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused(originatingComponent);

    const auto isSaveShortcut = key.getModifiers().isCommandDown()
        && (key.getTextCharacter() == 's' || key.getTextCharacter() == 'S');

    if (!isSaveShortcut || !dashboardView.isVisible())
        return false;

    if (!session.isBusy())
        showCommitMessagePopupForSave();

    return true;
}

void StemhubAudioProcessorEditor::refreshSessionUi()
{
    const auto& state = session.getState();

    // A save that finished cleanly consumes its note, and changed what the next one takes; a
    // failed one keeps the note for the retry.
    const auto didSave = lastObservedOperationState == OperationState::committing
                      && state.operationState == OperationState::idle
                      && !state.sessionStatus.isError();
    lastObservedOperationState = state.operationState;
    if (didSave)
    {
        dashboardView.clearCommitMessage();
        countedProjectFile = juce::File();
    }

    const bool isSignedIn = state.authState == AuthState::signedIn;
    const bool showProjectSelection = isSignedIn && state.uiState == UIState::projectSelection;
    const bool showDashboard = isSignedIn && !showProjectSelection;

    loginView.setVisible(!isSignedIn);
    projectSelectionView.setVisible(showProjectSelection);
    dashboardView.setVisible(showDashboard);

    if (showProjectSelection)
        refreshProjectSelectionUi();
    else if (showDashboard)
        refreshDashboardUi();
    else
        loginView.setMessage(state.authStatus.text, toMessageStatus(state.authStatus.severity));

    resized();
    repaint();
}

void StemhubAudioProcessorEditor::refreshProjectSelectionUi()
{
    const auto& state = session.getState();

    std::vector<ProjectListItem> projectItems;
    projectItems.reserve(state.projects.size());

    for (const auto& project : state.projects)
        projectItems.push_back({ project.id, project.name, project.description, project.category, project.isPublic });

    const auto effectiveProjectFile = session.getEffectiveProjectFile();
    const auto hasSelectedProjectFile = effectiveProjectFile.existsAsFile();
    const auto status = projectGridStatus(state);
    projectSelectionView.setMessage(status.text, toMessageStatus(status.severity));
    projectSelectionView.setActivity(toSessionActivity(state.operationState));
    projectSelectionView.setProjectFileSelectionState(hasSelectedProjectFile,
                                                      hasSelectedProjectFile
                                                          ? effectiveProjectFile.getFullPathName()
                                                          : juce::String());
    projectSelectionView.setCanCreateProject(hasSelectedProjectFile);
    projectSelectionView.setAccountName(state.currentUser ? state.currentUser->username : juce::String());
    projectSelectionView.setProjects(projectItems, state.selectedProject ? state.selectedProject->id : juce::String());
}

void StemhubAudioProcessorEditor::refreshDashboardUi()
{
    const auto& state = session.getState();

    std::vector<juce::String> branchNames;
    std::vector<juce::String> branchIds;
    branchNames.reserve(state.branches.size());
    branchIds.reserve(state.branches.size());

    for (const auto& branch : state.branches)
    {
        branchNames.push_back(branch.name);
        branchIds.push_back(branch.id);
    }

    std::vector<VersionListItem> versionItems;
    versionItems.reserve(state.versionHistory.size());

    for (const auto& version : state.versionHistory)
        versionItems.push_back(toVersionListItem(version, state.openedVersionId));

    const auto fileToDisplay = session.getEffectiveProjectFile();

    dashboardView.setProjectStatusMessage(state.sessionStatus.text, toMessageStatus(state.sessionStatus.severity));
    dashboardView.setActivity(toSessionActivity(state.operationState));
    dashboardView.setBranches(branchNames, branchIds, state.selectedBranchId);
    dashboardView.setVersions(versionItems, state.selectedVersionId);
    dashboardView.setProjectNameMessage(state.selectedProject ? state.selectedProject->name : "No project selected");
    dashboardView.setBranchNameMessage(state.selectedBranchName.isNotEmpty() ? state.selectedBranchName
                                                                             : "Workspace not selected");
    dashboardView.setSelectedProjectFilePath(fileToDisplay.existsAsFile()
                                                ? fileToDisplay.getFullPathName()
                                                : juce::String());

    refreshSnapshotSize();
}

void StemhubAudioProcessorEditor::refreshSnapshotSize()
{
    const auto projectFile = session.getEffectiveProjectFile();
    if (projectFile == countedProjectFile)
        return;

    countedProjectFile = projectFile;
    dashboardView.setSnapshotSize(-1, 0);
    if (projectFile.existsAsFile())
        snapshotCounter.enqueue([projectFile](const auto&) { return stemhub::snapshotfiles::summarize(projectFile); });
}

void StemhubAudioProcessorEditor::handleAsyncUpdate()
{
    // An older count for the same file is simply replaced by the newer one after it.
    for (const auto& summary : snapshotCounter.takeResults())
        if (summary.projectFile == countedProjectFile)
            dashboardView.setSnapshotSize(summary.fileCount, summary.totalBytes);
}

void StemhubAudioProcessorEditor::handleChooseProjectFileClick()
{
    launchProjectFileChooser("Select a DAW project file", [this](const juce::File& file)
    {
        session.setPendingProjectFile(file);
    });
}

void StemhubAudioProcessorEditor::launchProjectFileChooser(const juce::String& title,
                                                           std::function<void(const juce::File&)> onFileChosen)
{
    projectFileChooser = std::make_unique<juce::FileChooser>(
        title,
        session.getState().pendingProjectFile,
        kProjectFilePattern);

    constexpr auto chooserFlags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles;

    projectFileChooser->launchAsync(chooserFlags, [this, fileChosenCallback = std::move(onFileChosen)](const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (file.existsAsFile() && fileChosenCallback != nullptr)
            fileChosenCallback(file);

        projectFileChooser.reset();
    });
}

void StemhubAudioProcessorEditor::launchProjectFolderChooser(const juce::String& title,
                                                           std::function<void(const juce::File&)> onFolderChosen)
{
    const auto& pendingProjectFile = session.getState().pendingProjectFile;
    const auto defaultFolder = pendingProjectFile.existsAsFile()
        ? pendingProjectFile.getParentDirectory()
        : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    projectFileChooser = std::make_unique<juce::FileChooser>(
        title,
        defaultFolder,
        juce::String());

    constexpr auto chooserFlags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectDirectories;

    projectFileChooser->launchAsync(chooserFlags, [this, folderChosenCallback = std::move(onFolderChosen)](const juce::FileChooser& chooser)
    {
        const auto folder = chooser.getResult();
        if (folder.isDirectory() && folderChosenCallback != nullptr)
            folderChosenCallback(folder);

        projectFileChooser.reset();
    });
}

void StemhubAudioProcessorEditor::handleOpenProjectClick()
{
    const auto projectId = projectSelectionView.getSelectedProjectId();
    if (projectId.isEmpty())
    {
        showWarning(*this, "Open project", "Choose an existing project before continuing.");
        return;
    }

    const auto projectFile = session.getEffectiveProjectFile();
    if (projectFile.existsAsFile())
        session.setPendingProjectFile(projectFile);

    session.requestOpenProject(projectId, projectFile, true);
}

void StemhubAudioProcessorEditor::handleCreateProjectClick()
{
    const auto selectedFile = session.getEffectiveProjectFile();
    if (!selectedFile.existsAsFile())
    {
        showWarning(*this, "Create project", "Choose a project file first.");
        return;
    }

    session.setPendingProjectFile(selectedFile);
    session.requestCreateProject(selectedFile);
}

void StemhubAudioProcessorEditor::handleSignInClick()
{
    const auto email = loginView.getEmail().trim();
    const auto password = loginView.getPassword();

    if (email.isEmpty() || password.isEmpty())
    {
        loginView.setMessage("Please enter both email and password.", theme::MessageStatus::warning);
        return;
    }
    session.requestSignIn(email, password);
}

void StemhubAudioProcessorEditor::handleSignOutClick()
{
    session.signOut();
    loginView.clearInputs();
}

void StemhubAudioProcessorEditor::handleSaveChangesClick()
{
    requestSaveWithCommitMessage(dashboardView.getCommitMessage());
}

void StemhubAudioProcessorEditor::handleRestoreClick()
{
    const auto selectedVersionId = dashboardView.getSelectedVersionId();
    if (selectedVersionId.isEmpty())
    {
        showWarning(*this, "Restore version", "Select a version to restore before continuing.");
        return;
    }

    const auto editorRef = juce::Component::SafePointer<StemhubAudioProcessorEditor>(this);
    const auto confirmAndRestore = [editorRef](const juce::File& folder, const juce::String& versionToRestore)
    {
        auto* editor = editorRef.getComponent();
        if (editor == nullptr)
            return;

        juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                                         .withIconType(juce::MessageBoxIconType::QuestionIcon)
                                         .withTitle("Restore version")
                                         .withMessage("The version is downloaded into a new folder in\n"
                                                      + folder.getFullPathName()
                                                      + "\n\nIt then opens in your DAW as a project of its own. "
                                                        "The project you have open stays as it is.")
                                         .withButton("Restore")
                                         .withButton("Cancel")
                                         .withAssociatedComponent(editor),
                                     [editorRef, folder, versionToRestore](const int result)
                                     {
                                         auto* confirmedEditor = editorRef.getComponent();
                                         if (confirmedEditor == nullptr || result != 1)
                                             return;

                                         confirmedEditor->session.setSelectedVersionId(versionToRestore);
                                         confirmedEditor->session.requestRestoreVersion(versionToRestore, folder);
                                     });
    };

    auto restoreFolder = session.getEffectiveProjectFile().getParentDirectory();
    if (!restoreFolder.isDirectory())
        restoreFolder = session.getState().pendingProjectFile.getParentDirectory();

    if (!restoreFolder.isDirectory())
    {
        launchProjectFolderChooser("Select where to restore version snapshot", [selectedVersionId, confirmAndRestore](const juce::File& folder)
        {
            if (!folder.isDirectory())
                return;

            confirmAndRestore(folder, selectedVersionId);
        });
        return;
    }

    confirmAndRestore(restoreFolder, selectedVersionId);
}

void StemhubAudioProcessorEditor::requestSaveWithCommitMessage(juce::String commitMessage)
{
    const auto note = commitMessage.trim();
    dashboardView.setCommitMessage(note);

    if (!hasActiveProjectSelection())
    {
        showWarning(*this, "Save failed", "Choose or create a project before saving.");
        return;
    }

    const auto effectiveProjectFile = session.getEffectiveProjectFile();
    if (!effectiveProjectFile.existsAsFile())
    {
        launchProjectFileChooser("Select a DAW project file before saving", [this, note](const juce::File& file)
        {
            session.setPendingProjectFile(file);
            session.requestPushVersion(note);
        });
        return;
    }

    session.setPendingProjectFile(effectiveProjectFile);
    session.requestPushVersion(note);
}

bool StemhubAudioProcessorEditor::hasActiveProjectSelection() const
{
    const auto& state = session.getState();
    return state.selectedProject.has_value() && state.selectedBranchId.isNotEmpty();
}

void StemhubAudioProcessorEditor::handleSyncClick()
{
    // Files may have been added in the DAW since the last count.
    countedProjectFile = juce::File();
    session.requestRefreshVersionHistory();
}

void StemhubAudioProcessorEditor::handleChangeBranchClick()
{
    const auto selectedBranchId = dashboardView.getSelectedBranchId();
    if (selectedBranchId.isEmpty())
        return;

    session.requestSelectBranch(selectedBranchId);
}

void StemhubAudioProcessorEditor::handleVersionSelectionChanged()
{
    session.setSelectedVersionId(dashboardView.getSelectedVersionId());
}

void StemhubAudioProcessorEditor::showCommitMessagePopupForSave()
{
    if (commitPopup != nullptr)
        return;

    // Owned by the editor (and declared after its LookAndFeel) so it can never outlive either.
    commitPopup = std::make_unique<juce::AlertWindow>("Save version",
                                                      "Enter a save note before saving.",
                                                      juce::MessageBoxIconType::NoIcon,
                                                      this);
    commitPopup->setLookAndFeel(&pluginLookAndFeel);
    commitPopup->addTextEditor("commit_message", dashboardView.getCommitMessage(), "Save note");
    commitPopup->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    commitPopup->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (auto* saveButton = dynamic_cast<juce::TextButton*>(commitPopup->getButton("Save")))
        theme::stylePrimaryButton(*saveButton);

    if (auto* noteInput = commitPopup->getTextEditor("commit_message"))
    {
        theme::styleTextInput(*noteInput, "Save note");
        noteInput->setInputRestrictions(kMaxSaveNoteLength);
    }

    const auto editorRef = juce::Component::SafePointer<StemhubAudioProcessorEditor>(this);
    commitPopup->enterModalState(true, juce::ModalCallbackFunction::create([editorRef](int result)
    {
        if (editorRef == nullptr || editorRef->commitPopup == nullptr)
            return;

        const auto commitMessage = editorRef->commitPopup->getTextEditorContents("commit_message").trim();

        // Destroyed once the modal manager is done with it.
        juce::MessageManager::callAsync([editorRef]
        {
            if (editorRef != nullptr)
                editorRef->commitPopup.reset();
        });

        if (result != 1)
            return;

        editorRef->dashboardView.setCommitMessage(commitMessage);
        editorRef->requestSaveWithCommitMessage(commitMessage);
    }), false);
}

void StemhubAudioProcessorEditor::handleBackToProjectsClick()
{
    session.showProjectSelection();
}

void StemhubAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(theme::PluginTheme::kBackground);
}

void StemhubAudioProcessorEditor::resized()
{
    const auto area = getLocalBounds();
    loginView.setBounds(area);
    projectSelectionView.setBounds(area);
    dashboardView.setBounds(area);
}
