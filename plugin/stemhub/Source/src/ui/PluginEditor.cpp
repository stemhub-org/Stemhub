#include "ui/PluginEditor.hpp"
#include "application/ProjectFileService.hpp"
#include <algorithm>
#include <array>

namespace
{
constexpr auto kDefaultCommitMessage = "Save from plugin";
constexpr auto kDawName = "FL Studio";
constexpr auto kProjectFilePattern = "*.flp;*.als";
constexpr std::array<const char*, 9> kBundledAssetExtensions = {
    "wav", "mp3", "flac", "ogg", "aiff", "aif", "m4a", "mid", "midi"
};

juce::String getLoginMessage(const StemhubAudioProcessor& processor)
{
    if (processor.getAuthState() == AuthState::signingIn)
        return "Signing in to your StemHub account...";

    if (processor.getAuthState() == AuthState::authError && processor.getAuthErrorMessage().isNotEmpty())
        return processor.getAuthErrorMessage();

    return {};
}

stemhub::plugin::theme::MessageStatus getLoginStatus(const StemhubAudioProcessor& processor)
{
    if (processor.getAuthState() == AuthState::signingIn)
        return stemhub::plugin::theme::MessageStatus::loading;

    if (processor.getAuthState() == AuthState::authError)
        return stemhub::plugin::theme::MessageStatus::error;

    if (processor.getAuthErrorMessage().isNotEmpty())
        return stemhub::plugin::theme::MessageStatus::warning;

    return stemhub::plugin::theme::MessageStatus::neutral;
}

juce::String getProjectSelectionMessage(const StemhubAudioProcessor& processor)
{
    if (processor.getOperationState() == OperationState::loadingProjects)
        return "Loading projects...";

    if (processor.getProjectSelectionStatusMessage().isNotEmpty())
        return processor.getProjectSelectionStatusMessage();

    if (processor.getProjects().empty())
        return "No StemHub projects available for this account.";

    return "Open an existing project, or choose a local file to create one.";
}

stemhub::plugin::theme::MessageStatus getProjectSelectionStatus(const StemhubAudioProcessor& processor)
{
    if (processor.getOperationState() == OperationState::loadingProjects)
        return stemhub::plugin::theme::MessageStatus::loading;

    if (processor.getOperationState() == OperationState::error)
        return stemhub::plugin::theme::MessageStatus::error;

    if (processor.getProjectSelectionStatusMessage().isNotEmpty())
        return processor.getProjectSelectionStatusMessage().contains("No projects found")
            ? stemhub::plugin::theme::MessageStatus::warning
            : stemhub::plugin::theme::MessageStatus::neutral;

    if (processor.getProjects().empty())
        return stemhub::plugin::theme::MessageStatus::warning;

    return stemhub::plugin::theme::MessageStatus::success;
}

juce::String getDashboardMessage(const StemhubAudioProcessor& processor)
{
    if (processor.getOperationState() == OperationState::committing)
        return "Saving version...";

    if (processor.getOperationState() == OperationState::pulling)
        return "Syncing version history...";

    if (processor.getOperationState() == OperationState::restoring)
        return "Restoring version...";

    if (processor.getActiveProjectStatusMessage().isNotEmpty())
        return processor.getActiveProjectStatusMessage();

    return "Project ready.";
}

stemhub::plugin::theme::MessageStatus getDashboardStatus(const StemhubAudioProcessor& processor)
{
    if (processor.getOperationState() == OperationState::committing
        || processor.getOperationState() == OperationState::pulling
        || processor.getOperationState() == OperationState::restoring)
        return stemhub::plugin::theme::MessageStatus::loading;

    if (processor.getOperationState() == OperationState::error)
        return stemhub::plugin::theme::MessageStatus::error;

    if (processor.getActiveProjectStatusMessage().contains("failed")
        || processor.getActiveProjectStatusMessage().contains("could not"))
        return stemhub::plugin::theme::MessageStatus::error;

    if (processor.getActiveProjectStatusMessage().isNotEmpty())
        return stemhub::plugin::theme::MessageStatus::success;

    return stemhub::plugin::theme::MessageStatus::neutral;
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

bool isBackupPath(const juce::File& candidateFile, const juce::File& rootFolder)
{
    const auto relativePath = candidateFile.getRelativePathFrom(rootFolder).replaceCharacter('\\', '/');
    if (relativePath.isEmpty())
        return false;

    juce::StringArray parts;
    parts.addTokens(relativePath, "/", "");
    for (int i = 0; i < parts.size() - 1; ++i)
    {
        if (parts[i].equalsIgnoreCase("backup"))
            return true;
    }

    return false;
}

// Mirror the same root folder choice used by snapshot bundling so the UI preview matches what a save uploads.
juce::File resolveBundleRootDirectory(const juce::File& effectiveProjectFile)
{
    if (effectiveProjectFile.existsAsFile())
        return effectiveProjectFile.getParentDirectory();

    return {};
}

std::vector<juce::String> collectPackagedRelativeFilePaths(const juce::File& bundleRootDirectory,
                                                           const juce::File& sourceProjectFile)
{
    std::vector<juce::String> relativePaths;
    if (!bundleRootDirectory.isDirectory())
        return relativePaths;

    juce::Array<juce::File> discoveredFiles;
    bundleRootDirectory.findChildFiles(discoveredFiles, juce::File::findFiles, true);
    relativePaths.reserve(static_cast<size_t>(discoveredFiles.size()));

    for (const auto& file : discoveredFiles)
    {
        if (!file.existsAsFile())
            continue;

        if (file != sourceProjectFile)
        {
            bool hasAllowedExtension = false;
            for (const auto* ext : kBundledAssetExtensions)
            {
                if (file.hasFileExtension(ext))
                {
                    hasAllowedExtension = true;
                    break;
                }
            }

            if (!hasAllowedExtension || isBackupPath(file, bundleRootDirectory))
                continue;
        }

        const auto relativePath = file.getRelativePathFrom(bundleRootDirectory).replaceCharacter('\\', '/');
        if (relativePath.isNotEmpty())
            relativePaths.push_back(relativePath);
    }

    std::sort(relativePaths.begin(), relativePaths.end(), [](const juce::String& lhs, const juce::String& rhs)
    {
        return lhs.compareNatural(rhs) < 0;
    });
    return relativePaths;
}
}

StemhubAudioProcessorEditor::StemhubAudioProcessorEditor(StemhubAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), audioProcessor(processorToEdit)
{
    // Scoped to this editor: the process-wide default is shared by every plugin instance.
    setLookAndFeel(&pluginLookAndFeel);
    setSize(720, 560);
    setWantsKeyboardFocus(true);
    setOpaque(true);
    addKeyListener(this);

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
    dashboardView.onBackToProjects = [this] { handleBackToProjectsClick(); };
    dashboardView.onSignOut = [this] { handleSignOutClick(); };
    dashboardView.onRestore = [this] { handleRestoreClick(); };

    audioProcessor.requestRestoreCachedSession();
    refreshSessionUi();
}

StemhubAudioProcessorEditor::~StemhubAudioProcessorEditor()
{
    removeKeyListener(this);
    audioProcessor.removeChangeListener(this);
    commitPopup.reset();
    setLookAndFeel(nullptr);
}

void StemhubAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &audioProcessor)
        refreshSessionUi();
}

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

void StemhubAudioProcessorEditor::refreshSessionUi()
{
    // A save that finished cleanly consumes its note; a failed one keeps it for the retry.
    const auto operationState = audioProcessor.getOperationState();
    if (lastObservedOperationState == OperationState::committing && operationState == OperationState::idle)
        dashboardView.clearCommitMessage();
    lastObservedOperationState = operationState;

    const bool isSignedIn = audioProcessor.getAuthState() == AuthState::signedIn;
    const bool showProjectSelection = isSignedIn && audioProcessor.getUIState() == UIState::projectSelection;
    const bool showDashboard = isSignedIn && !showProjectSelection;

    loginView.setVisible(!isSignedIn);
    projectSelectionView.setVisible(showProjectSelection);
    dashboardView.setVisible(showDashboard);

    if (showProjectSelection)
        refreshProjectSelectionUi();
    else if (showDashboard)
        refreshDashboardUi();
    else
        loginView.setMessage(getLoginMessage(audioProcessor), getLoginStatus(audioProcessor));

    resized();
    repaint();
}

void StemhubAudioProcessorEditor::refreshProjectSelectionUi()
{
    std::vector<ProjectListItem> projectItems;
    const auto& projects = audioProcessor.getProjects();
    projectItems.reserve(projects.size());

    for (const auto& project : projects)
        projectItems.push_back({ project.id, project.name, project.description, project.category, project.isPublic });

    const auto effectiveProjectFile = getEffectiveProjectFile();
    const auto hasSelectedProjectFile = effectiveProjectFile.existsAsFile();
    projectSelectionView.setMessage(getProjectSelectionMessage(audioProcessor),
                                   getProjectSelectionStatus(audioProcessor));
    projectSelectionView.setProjectFileSelectionState(hasSelectedProjectFile,
                                                      hasSelectedProjectFile
                                                          ? effectiveProjectFile.getFullPathName()
                                                          : juce::String());
    projectSelectionView.setCanCreateProject(hasSelectedProjectFile);
    projectSelectionView.setAccountName(audioProcessor.getCurrentUser() ? audioProcessor.getCurrentUser()->username
                                                                        : juce::String());
    projectSelectionView.setProjects(projectItems,
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

    std::vector<VersionListItem> versionItems;
    const auto& versions = audioProcessor.getVersionHistory();
    versionItems.reserve(versions.size());

    for (const auto& version : versions)
        versionItems.push_back(toVersionListItem(version, audioProcessor.getCurrentOpenedVersionId()));

    const auto fileToDisplay = getEffectiveProjectFile();

    dashboardView.setProjectStatusMessage(getDashboardMessage(audioProcessor), getDashboardStatus(audioProcessor));
    dashboardView.setBranches(branchNames, branchIds, audioProcessor.getSelectedBranchId());
    dashboardView.setVersions(versionItems, audioProcessor.getSelectedVersionId());
    juce::Logger::writeToLog("[UI] Dashboard refresh -> selectedVersionId=" + audioProcessor.getSelectedVersionId()
                             + ", openedFile=" + (fileToDisplay.existsAsFile() ? fileToDisplay.getFullPathName() : "not available"));
    dashboardView.setProjectNameMessage(audioProcessor.getSelectedProject()
        ? audioProcessor.getSelectedProject()->name
        : "No project selected");
    dashboardView.setBranchNameMessage(audioProcessor.getSelectedBranchName().isNotEmpty()
        ? audioProcessor.getSelectedBranchName()
        : "Workspace not selected");
    dashboardView.setSelectedProjectFilePath(fileToDisplay.existsAsFile()
                                                ? fileToDisplay.getFullPathName()
                                                : juce::String());

    const auto bundleRootDirectory = resolveBundleRootDirectory(fileToDisplay);
    dashboardView.setPackagedFiles({},
                                   collectPackagedRelativeFilePaths(bundleRootDirectory, fileToDisplay));
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
    const auto pendingProjectFile = audioProcessor.getPendingProjectFile();
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

    const auto projectFile = getEffectiveProjectFile();
    if (projectFile.existsAsFile())
        audioProcessor.setPendingProjectFile(projectFile);

    audioProcessor.requestOpenProject(projectId, projectFile, true);
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleCreateProjectClick()
{
    const auto selectedFile = getEffectiveProjectFile();
    if (!selectedFile.existsAsFile())
    {
        showWarning(*this, "Create project", "Choose a project file first.");
        return;
    }

    audioProcessor.setPendingProjectFile(selectedFile);
    audioProcessor.requestCreateProject(selectedFile);
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleSignInClick()
{
    const auto email = loginView.getEmail().trim();
    const auto password = loginView.getPassword();

    if (email.isEmpty() || password.isEmpty())
    {
        loginView.setMessage("Please enter both email and password.", stemhub::plugin::theme::MessageStatus::warning);
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
    requestSaveWithCommitMessage(dashboardView.getCommitMessage());
}

void StemhubAudioProcessorEditor::handleRestoreClick()
{
    const auto selectedVersionId = dashboardView.getSelectedVersionId();

    juce::Logger::writeToLog("[Restore] UI -> request started. processorSelectedVersionId="
                             + audioProcessor.getSelectedVersionId()
                             + ", dropdownVersionId="
                             + selectedVersionId);

    if (selectedVersionId.isEmpty())
    {
        showWarning(*this, "Restore version", "Select a version to restore before continuing.");
        return;
    }

    const auto editorRef = juce::Component::SafePointer<StemhubAudioProcessorEditor>(this);
    const auto confirmAndRestore = [editorRef](const juce::File& folder, const juce::String& versionToRestore)
    {
        if (editorRef == nullptr)
            return;

        auto* editor = editorRef.getComponent();
        if (editor == nullptr)
            return;

        juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon,
            "Restore version",
            "Restoring will replace the currently selected local project file in the plugin context.\n\n"
            "Do you want to continue?",
            "Yes",
            "No",
            editor,
            juce::ModalCallbackFunction::create([editorRef, folder, versionToRestore](const int result)
            {
                if (editorRef == nullptr)
                    return;

                auto* mutableEditor = editorRef.getComponent();
                if (mutableEditor == nullptr)
                    return;

                juce::Logger::writeToLog("[Restore] UI -> confirm result=" + juce::String(result));

                if (result != 1)
                {
                    juce::Logger::writeToLog("[Restore] UI -> user cancelled restore.");
                    return;
                }

                mutableEditor->audioProcessor.setSelectedVersionId(versionToRestore);
                juce::Logger::writeToLog("[Restore] UI -> syncing selectedVersionId before restore: "
                                         + versionToRestore);
                juce::Logger::writeToLog("[Restore] UI -> requesting restore from confirmation callback: "
                                         + folder.getFullPathName() + ", version="
                                         + versionToRestore);
                mutableEditor->audioProcessor.requestRestoreVersion(versionToRestore, folder);
                mutableEditor->refreshSessionUi();
            }));
    };

    auto restoreFolder = getEffectiveProjectFile().getParentDirectory();
    if (!restoreFolder.isDirectory())
        restoreFolder = audioProcessor.getPendingProjectFile().getParentDirectory();

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
    const auto trimmedCommitMessage = commitMessage.trim();
    dashboardView.setCommitMessage(trimmedCommitMessage);

    if (!hasActiveProjectSelection())
    {
        showWarning(*this, "Save failed", "Choose or create a project before saving.");
        refreshSessionUi();
        return;
    }

    const auto effectiveCommitMessage = trimmedCommitMessage.isNotEmpty()
        ? trimmedCommitMessage
        : juce::String(kDefaultCommitMessage);

    const auto effectiveProjectFile = getEffectiveProjectFile();
    if (!effectiveProjectFile.existsAsFile())
    {
        launchProjectFileChooser("Select a DAW project file before saving", [this, effectiveCommitMessage](const juce::File& file)
        {
            audioProcessor.setPendingProjectFile(file);
            triggerPushVersion(effectiveCommitMessage);
        });

        refreshSessionUi();
        return;
    }

    audioProcessor.setPendingProjectFile(effectiveProjectFile);
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
    return stemhub::projectfiles::resolveEffectiveProjectFile(
        audioProcessor.getSelectedProjectFile(),
        audioProcessor.getPendingProjectFile());
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
        return;

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
        stemhub::plugin::theme::stylePrimaryButton(*saveButton);

    if (auto* noteInput = commitPopup->getTextEditor("commit_message"))
        stemhub::plugin::theme::styleTextInput(*noteInput, "Save note");

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
    // A save or restore in flight belongs to this project; leaving would mix its result into another.
    if (audioProcessor.isWriteOperationInProgress())
        return;

    audioProcessor.setOperationState(OperationState::idle);
    audioProcessor.setUIState(UIState::projectSelection);
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(stemhub::plugin::theme::PluginTheme::kBackground);
}

void StemhubAudioProcessorEditor::resized()
{
    const auto area = getLocalBounds();
    loginView.setBounds(area);
    projectSelectionView.setBounds(area);
    dashboardView.setBounds(area);
}
