#include "ui/PluginEditor.hpp"

namespace
{
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
}

StemhubAudioProcessorEditor::StemhubAudioProcessorEditor(juce::AudioProcessor& ownerProcessor, StemhubSession& sessionToShow)
    : AudioProcessorEditor(&ownerProcessor),
      session(sessionToShow),
      presenter(sessionToShow.getState().lastSavedVersionId)
{
    // Scoped to this editor: the process-wide default is shared by every plugin instance.
    setLookAndFeel(&pluginLookAndFeel);
    setSize(720, 560);
    setOpaque(true);

    session.addChangeListener(this);

    addAndMakeVisible(loginView);
    addAndMakeVisible(projectSelectionView);
    addAndMakeVisible(dashboardView);

    loginView.onSignIn = [this] { signIn(); };
    loginView.onCancel = [this] { session.cancelRequest(); };
    projectSelectionView.onChooseProjectFile = [this] { chooseProjectFile(); };
    projectSelectionView.onOpenProject = [this](const juce::String& projectId) { session.requestOpenProject(projectId, true); };
    projectSelectionView.onCreateProject = [this] { createProject(); };
    projectSelectionView.onSignOut = [this] { signOut(); };
    dashboardView.onSave = [this] { save(); };
    dashboardView.onRefresh = [this] { refresh(); };
    dashboardView.onBranchChange = [this](const juce::String& branchId) { session.requestSelectBranch(branchId); };
    dashboardView.onVersionSelected = [this](const juce::String& versionId) { session.setSelectedVersionId(versionId); };
    dashboardView.onBackToProjects = [this] { session.showProjectSelection(); };
    dashboardView.onSignOut = [this] { signOut(); };
    dashboardView.onRestore = [this](const juce::String& versionId) { restore(versionId); };
    dashboardView.onCancel = [this] { session.cancelRequest(); };
    dashboardView.setMaxMessageLength(kMaxMessageLength);

    session.requestResumeSignIn();
    refreshSessionUi();
}

StemhubAudioProcessorEditor::~StemhubAudioProcessorEditor()
{
    workingCopyCounter.shutdown();
    cancelPendingUpdate();
    session.removeChangeListener(this);
    setLookAndFeel(nullptr);
}

void StemhubAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &session)
        refreshSessionUi();
}

void StemhubAudioProcessorEditor::refreshSessionUi()
{
    const auto& state = session.getState();

    // Only what the screen shown needs: each check reads the disk.
    SessionPresenter::FileFacts files;
    if (state.isSignedIn() && state.uiState == UIState::projectSelection)
        files.newProjectFile = session.getProjectFileForGrid();
    else if (state.isSignedIn())
        files.workingFileExists = state.workingFile.existsAsFile();

    const auto model = presenter.present(state, files);

    if (model.messageWasSaved)
    {
        dashboardView.clearMessage();
        countedProjectFile = juce::File();
    }

    loginView.setVisible(model.screen == Screen::login);
    projectSelectionView.setVisible(model.screen == Screen::projects);
    dashboardView.setVisible(model.screen == Screen::dashboard);

    // Each view changes only what differs from what it shows.
    switch (model.screen)
    {
        case Screen::login:
            loginView.show(model.login);
            break;

        case Screen::projects:
            projectSelectionView.show(model.grid);
            break;

        case Screen::dashboard:
            dashboardView.show(model.dashboard);
            countWorkingCopy(model.dashboard.workingFilePath);
            break;
    }
}

void StemhubAudioProcessorEditor::countWorkingCopy(const juce::String& workingFilePath)
{
    const auto projectFile = workingFilePath.isNotEmpty() ? juce::File(workingFilePath) : juce::File();
    if (projectFile == countedProjectFile)
        return;

    countedProjectFile = projectFile;
    dashboardView.setWorkingCopySize(-1, 0);
    if (projectFile != juce::File())
        workingCopyCounter.enqueue([projectFile](const auto&) { return stemhub::versionfiles::summarize(projectFile); });
}

void StemhubAudioProcessorEditor::handleAsyncUpdate()
{
    // An older count for the same file is simply replaced by the newer one after it.
    for (const auto& summary : workingCopyCounter.takeResults())
        if (summary.projectFile == countedProjectFile)
            dashboardView.setWorkingCopySize(summary.fileCount, summary.totalBytes);
}

void StemhubAudioProcessorEditor::signIn()
{
    const auto email = loginView.getEmail().trim();
    const auto password = loginView.getPassword();

    if (email.isEmpty() || password.isEmpty())
    {
        loginView.showFormMessage("Please enter both email and password.");
        return;
    }

    session.requestSignIn(email, password);
}

void StemhubAudioProcessorEditor::signOut()
{
    session.signOut();
    loginView.clearInputs();
}

void StemhubAudioProcessorEditor::chooseProjectFile()
{
    launchProjectFileChooser("Select a project file", session.getProjectFileForGrid(), [this](const juce::File& file)
    {
        session.chooseProjectFile(file);
    });
}

void StemhubAudioProcessorEditor::createProject()
{
    if (!session.getProjectFileForGrid().existsAsFile())
    {
        showWarning(*this, "Create project", "Choose a project file first.");
        return;
    }

    session.requestCreateProject();
}

void StemhubAudioProcessorEditor::save()
{
    const auto message = dashboardView.getMessage();
    dashboardView.setMessage(message);

    if (!session.getState().hasOpenProject())
    {
        showWarning(*this, "Save failed", "Choose or create a project before saving.");
        return;
    }

    const auto& workingFile = session.getState().workingFile;
    if (!workingFile.existsAsFile())
    {
        launchProjectFileChooser("Select a project file before saving", workingFile, [this, message](const juce::File& file)
        {
            session.setWorkingFile(file);
            session.requestSaveVersion(message);
        });
        return;
    }

    session.requestSaveVersion(message);
}

void StemhubAudioProcessorEditor::refresh()
{
    // Files may have been added in the DAW since the last count.
    countedProjectFile = juce::File();
    session.requestRefreshVersionHistory();
}

void StemhubAudioProcessorEditor::restore(const juce::String& versionId)
{
    if (versionId.isEmpty())
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
                                                      + "\n\nIt then opens in your DAW as a separate copy. "
                                                        "The project file you have open stays as it is.")
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

    // Next to the working copy, when its folder is there.
    const auto restoreFolder = session.getState().workingFile.getParentDirectory();
    if (!restoreFolder.isDirectory())
    {
        launchProjectFolderChooser("Select where to restore this version", [versionId, confirmAndRestore](const juce::File& folder)
        {
            confirmAndRestore(folder, versionId);
        });
        return;
    }

    confirmAndRestore(restoreFolder, versionId);
}

void StemhubAudioProcessorEditor::launchProjectFileChooser(const juce::String& title,
                                                           const juce::File& initialFile,
                                                           std::function<void(const juce::File&)> onFileChosen)
{
    fileChooser = std::make_unique<juce::FileChooser>(title, initialFile, stemhub::versionfiles::projectFilePattern());

    constexpr auto chooserFlags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(chooserFlags, [this, fileChosenCallback = std::move(onFileChosen)](const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (file.existsAsFile() && fileChosenCallback != nullptr)
            fileChosenCallback(file);

        fileChooser.reset();
    });
}

void StemhubAudioProcessorEditor::launchProjectFolderChooser(const juce::String& title,
                                                             std::function<void(const juce::File&)> onFolderChosen)
{
    const auto workingFolder = session.getState().workingFile.getParentDirectory();
    const auto defaultFolder = workingFolder.isDirectory()
        ? workingFolder
        : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    fileChooser = std::make_unique<juce::FileChooser>(title, defaultFolder, juce::String());

    constexpr auto chooserFlags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectDirectories;

    fileChooser->launchAsync(chooserFlags, [this, folderChosenCallback = std::move(onFolderChosen)](const juce::FileChooser& chooser)
    {
        const auto folder = chooser.getResult();
        if (folder.isDirectory() && folderChosenCallback != nullptr)
            folderChosenCallback(folder);

        fileChooser.reset();
    });
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
