#include "ui/PluginEditor.hpp"
#include <algorithm>
#include <array>

namespace
{
constexpr auto kDefaultCommitMessage = "Save from plugin";
constexpr auto kDawName = "FL Studio";
constexpr std::array<const char*, 9> kBundledAssetExtensions = {
    "wav", "mp3", "flac", "ogg", "aiff", "aif", "m4a", "mid", "midi"
};

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
        return "Choose a DAW project folder to create your first project.";

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

std::vector<juce::File> collectProjectFileCandidates(const juce::File& folder, bool recursive)
{
    std::vector<juce::File> candidates;
    if (!folder.isDirectory())
        return candidates;

    juce::Array<juce::File> flpFiles;
    juce::Array<juce::File> alsFiles;
    folder.findChildFiles(flpFiles, juce::File::findFiles, recursive, "*.flp");
    folder.findChildFiles(alsFiles, juce::File::findFiles, recursive, "*.als");

    candidates.reserve(static_cast<size_t>(flpFiles.size() + alsFiles.size()));

    for (const auto& file : flpFiles)
    {
        if (file.existsAsFile() && (!recursive || !isBackupPath(file, folder)))
            candidates.push_back(file);
    }

    for (const auto& file : alsFiles)
    {
        if (file.existsAsFile() && (!recursive || !isBackupPath(file, folder)))
            candidates.push_back(file);
    }

    return candidates;
}

int extensionPriority(const juce::File& file)
{
    return file.hasFileExtension("flp") ? 0 : 1;
}

juce::File chooseBestCandidate(const juce::File& folder, std::vector<juce::File> candidates)
{
    if (candidates.empty())
        return {};

    const auto folderName = folder.getFileName();
    for (const auto& candidate : candidates)
    {
        if (candidate.getFileNameWithoutExtension().equalsIgnoreCase(folderName))
            return candidate;
    }

    std::sort(candidates.begin(), candidates.end(), [](const juce::File& lhs, const juce::File& rhs)
    {
        const auto lhsPriority = extensionPriority(lhs);
        const auto rhsPriority = extensionPriority(rhs);
        if (lhsPriority != rhsPriority)
            return lhsPriority < rhsPriority;

        const auto lhsTime = lhs.getLastModificationTime().toMilliseconds();
        const auto rhsTime = rhs.getLastModificationTime().toMilliseconds();
        if (lhsTime != rhsTime)
            return lhsTime > rhsTime;

        return lhs.getFileName().compareNatural(rhs.getFileName()) < 0;
    });

    return candidates.front();
}

juce::File resolvePrimaryProjectFileFromFolder(const juce::File& folder)
{
    if (!folder.isDirectory())
        return {};

    auto topLevelCandidates = collectProjectFileCandidates(folder, false);
    auto selectedCandidate = chooseBestCandidate(folder, std::move(topLevelCandidates));
    if (selectedCandidate.existsAsFile())
        return selectedCandidate;

    auto recursiveCandidates = collectProjectFileCandidates(folder, true);
    return chooseBestCandidate(folder, std::move(recursiveCandidates));
}

// Mirror the same root folder choice used by snapshot bundling so the UI preview matches saved artifacts.
juce::File resolveBundleRootDirectory(const StemhubAudioProcessor& processor, const juce::File& effectiveProjectFile)
{
    const auto selectedFolder = processor.getSelectedProjectFolder();
    if (selectedFolder.isDirectory())
        return selectedFolder;

    if (effectiveProjectFile.existsAsFile())
        return effectiveProjectFile.getParentDirectory();

    const auto pendingFolder = processor.getPendingProjectFolder();
    if (pendingFolder.isDirectory())
        return pendingFolder;

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
    juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName("Syne");
    setWantsKeyboardFocus(true);
    addKeyListener(this);

    setSize(780, 430);
    audioProcessor.addChangeListener(this);

    addAndMakeVisible(loginView);
    addAndMakeVisible(projectSelectionView);
    addAndMakeVisible(dashboardView);

    loginView.onSignIn = [this] { handleSignInClick(); };
    projectSelectionView.onChooseProjectFolder = [this] { handleChooseProjectFolderClick(); };
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

    refreshSessionUi();
}

StemhubAudioProcessorEditor::~StemhubAudioProcessorEditor()
{
    removeKeyListener(this);
    audioProcessor.removeChangeListener(this);
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
        loginView.setMessage(getLoginMessage(audioProcessor));

    resized();
    repaint();
}

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

    const auto pendingFolder = audioProcessor.getPendingProjectFolder();
    const auto effectiveProjectFile = getEffectiveProjectFile();
    const auto hasSelectedProjectFile = effectiveProjectFile.existsAsFile();
    projectSelectionView.setHasExistingProjects(!projects.empty());
    projectSelectionView.setCanCreateProject(hasSelectedProjectFile);
    projectSelectionView.setMessage(getProjectSelectionMessage(audioProcessor));
    if (pendingFolder.isDirectory())
    {
        const auto fileSuffix = hasSelectedProjectFile ? (" -> " + effectiveProjectFile.getFileName())
                                                       : " (no .flp/.als found)";
        projectSelectionView.setSelectedProjectFileMessage("Folder: " + pendingFolder.getFullPathName() + fileSuffix);
    }
    else
    {
        projectSelectionView.setSelectedProjectFileMessage(hasSelectedProjectFile
            ? effectiveProjectFile.getFullPathName()
            : "No project folder selected.");
    }
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

    const auto bundleRootDirectory = resolveBundleRootDirectory(audioProcessor, fileToDisplay);
    dashboardView.setPackagedFiles(bundleRootDirectory.getFullPathName(),
                                   collectPackagedRelativeFilePaths(bundleRootDirectory, fileToDisplay));
}

void StemhubAudioProcessorEditor::handleChooseProjectFolderClick()
{
    launchProjectFolderChooser("Select a DAW project folder", [this](const juce::File& folder)
    {
        audioProcessor.setPendingProjectFolder(folder);

        const auto projectFile = resolvePrimaryProjectFileFromFolder(folder);
        if (projectFile.existsAsFile())
            audioProcessor.setPendingProjectFile(projectFile);
    });
}

void StemhubAudioProcessorEditor::launchProjectFolderChooser(const juce::String& title,
                                                           std::function<void(const juce::File&)> onFolderChosen)
{
    projectFileChooser = std::make_unique<juce::FileChooser>(
        title,
        audioProcessor.getPendingProjectFolder(),
        juce::String());

    constexpr auto flags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectDirectories;

    projectFileChooser->launchAsync(flags, [this, folderChosenCallback = std::move(onFolderChosen)](const juce::FileChooser& chooser)
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
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Open project",
            "Choose an existing project before continuing.");
        return;
    }

    const auto projectFile = getEffectiveProjectFile();
    if (!projectFile.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Open project",
            "Choose a project folder containing a .flp or .als file first.");
        return;
    }

    audioProcessor.setPendingProjectFile(projectFile);
    audioProcessor.requestOpenProject(projectId, projectFile);
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::handleCreateProjectClick()
{
    const auto selectedFile = getEffectiveProjectFile();
    if (!selectedFile.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Create project",
            "Choose a project folder containing a .flp or .als file first.");
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
    requestSaveWithCommitMessage(dashboardView.getCommitMessage());
}

void StemhubAudioProcessorEditor::handleRestoreClick()
{
    const auto selectedVersionId = dashboardView.getSelectedVersionId();
    if (selectedVersionId.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Restore version",
            "Select a version to restore before continuing.");
        return;
    }

    launchProjectFolderChooser("Select where to restore version snapshot", [this, selectedVersionId](const juce::File& folder)
    {
        if (!folder.isDirectory())
            return;

        audioProcessor.requestRestoreVersion(selectedVersionId, folder);
        refreshSessionUi();
    });
}

void StemhubAudioProcessorEditor::requestSaveWithCommitMessage(juce::String commitMessage)
{
    const auto trimmedCommitMessage = commitMessage.trim();
    dashboardView.setCommitMessage(trimmedCommitMessage);

    if (!hasActiveProjectSelection())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Push failed",
            "Choose or create a project before saving.");
        refreshSessionUi();
        return;
    }

    const auto effectiveCommitMessage = trimmedCommitMessage.isNotEmpty()
        ? trimmedCommitMessage
        : juce::String(kDefaultCommitMessage);

    const auto effectiveProjectFile = getEffectiveProjectFile();
    if (!effectiveProjectFile.existsAsFile())
    {
        launchProjectFolderChooser("Select a project folder before saving", [this, effectiveCommitMessage](const juce::File& folder)
        {
            audioProcessor.setPendingProjectFolder(folder);
            const auto projectFile = resolvePrimaryProjectFileFromFolder(folder);
            if (!projectFile.existsAsFile())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Save version",
                    "No .flp or .als file found in this folder.");
                refreshSessionUi();
                return;
            }

            audioProcessor.setPendingProjectFile(projectFile);
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
    const auto selectedProjectFile = audioProcessor.getSelectedProjectFile();
    if (selectedProjectFile.existsAsFile())
        return selectedProjectFile;

    const auto pendingProjectFile = audioProcessor.getPendingProjectFile();
    if (pendingProjectFile.existsAsFile())
        return pendingProjectFile;

    return resolvePrimaryProjectFileFromFolder(audioProcessor.getPendingProjectFolder());
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
    refreshSessionUi();
}

void StemhubAudioProcessorEditor::paint(juce::Graphics& g)
{
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
}

void StemhubAudioProcessorEditor::resized()
{
    const auto area = getLocalBounds();
    loginView.setBounds(area);
    projectSelectionView.setBounds(area);
    dashboardView.setBounds(area);
}
