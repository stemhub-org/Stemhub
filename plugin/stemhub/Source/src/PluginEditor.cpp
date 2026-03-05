#include <array>

#include "../include/PluginEditor.hpp"

namespace
{
constexpr auto kDefaultCommitMessage = "Save from plugin";
constexpr auto kDefaultCommitTemplate = "Custom";
constexpr auto kDawName = "FL Studio";
constexpr auto kProjectFilePattern = "*.flp;*.als";

struct CommitTemplateDefinition
{
    const char* name;
    const char* pattern;
};

constexpr std::array<CommitTemplateDefinition, 5> kCommitTemplates { {
    { "Custom", "" },
    { "Update snapshots", "Update {project} on {branch} ({date})" },
    { "Fix mix", "Fix mix in {project} ({branch})" },
    { "Arrangement tweak", "Tweak arrangement for {project} ({branch})" },
    { "Sound design", "Sound design pass for {project} ({branch})" },
} };

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

std::vector<juce::String> getCommitTemplateNames()
{
    std::vector<juce::String> names;
    names.reserve(kCommitTemplates.size());

    for (const auto& templateDefinition : kCommitTemplates)
        names.emplace_back(templateDefinition.name);

    return names;
}

const CommitTemplateDefinition* findTemplateByName(const juce::String& templateName)
{
    for (const auto& templateDefinition : kCommitTemplates)
    {
        if (templateName.compareIgnoreCase(templateDefinition.name) == 0)
            return &templateDefinition;
    }

    return nullptr;
}

juce::String renderCommitTemplate(const juce::String& templateName, const StemhubAudioProcessor& processor)
{
    const auto* templateDefinition = findTemplateByName(templateName);
    if (templateDefinition == nullptr)
        return {};

    juce::String pattern(templateDefinition->pattern);
    if (pattern.isEmpty())
        return {};

    const auto projectName = processor.getSelectedProject() ? processor.getSelectedProject()->name : "project";
    const auto branchName = processor.getSelectedBranchName().isNotEmpty() ? processor.getSelectedBranchName() : "main";
    const auto now = juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M");

    return pattern
        .replace("{project}", projectName)
        .replace("{branch}", branchName)
        .replace("{date}", now)
        .replace("{daw}", kDawName);
}

int resolveTemplateSelectionId(const std::vector<juce::String>& templateNames, const juce::String& selectedTemplate)
{
    for (size_t i = 0; i < templateNames.size(); ++i)
    {
        if (templateNames[i] == selectedTemplate)
            return static_cast<int>(i) + 1;
    }

    return 1;
}
}

StemhubAudioProcessorEditor::StemhubAudioProcessorEditor(StemhubAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), audioProcessor(processorToEdit)
{
    juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName("Syne");
    setWantsKeyboardFocus(true);
    addKeyListener(this);

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
    dashboardView.onApplyCommitTemplate = [this] { handleApplyCommitTemplateClick(); };
    dashboardView.onBackToProjects = [this] { handleBackToProjectsClick(); };
    dashboardView.onSignOut = [this] { handleSignOutClick(); };
    dashboardView.setCommitTemplates(getCommitTemplateNames(), kDefaultCommitTemplate);

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

    constexpr auto flags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles;

    projectFileChooser->launchAsync(flags, [this, fileChosenCallback = std::move(onFileChosen)](const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (file.existsAsFile() && fileChosenCallback != nullptr)
            fileChosenCallback(file);

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
    requestSaveWithCommitMessage(dashboardView.getCommitMessage());
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

    if (!getEffectiveProjectFile().existsAsFile())
    {
        launchProjectFileChooser("Select a DAW project file before saving", [this, effectiveCommitMessage](const juce::File& file)
        {
            audioProcessor.setPendingProjectFile(file);
            triggerPushVersion(effectiveCommitMessage);
        });

        refreshSessionUi();
        return;
    }

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

void StemhubAudioProcessorEditor::handleApplyCommitTemplateClick()
{
    const auto templateName = dashboardView.getSelectedCommitTemplate();
    const auto renderedTemplate = renderCommitTemplate(templateName, audioProcessor);
    if (renderedTemplate.isNotEmpty())
        dashboardView.setCommitMessage(renderedTemplate);
}

void StemhubAudioProcessorEditor::showCommitMessagePopupForSave()
{
    auto* commitPopup = new juce::AlertWindow("Save version",
                                              "Enter a commit message before saving.",
                                              juce::AlertWindow::NoIcon);

    const auto templateNames = getCommitTemplateNames();
    juce::StringArray templateNameChoices;
    for (const auto& templateName : templateNames)
        templateNameChoices.add(templateName);

    commitPopup->addComboBox("template", templateNameChoices, "Template");
    commitPopup->addTextEditor("commit_message", dashboardView.getCommitMessage(), "Commit message");
    commitPopup->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    commitPopup->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (auto* templateSelector = commitPopup->getComboBoxComponent("template"))
    {
        const auto selectedTemplate = dashboardView.getSelectedCommitTemplate();
        templateSelector->setSelectedId(resolveTemplateSelectionId(templateNames, selectedTemplate),
                                        juce::dontSendNotification);

        if (auto* messageEditor = commitPopup->getTextEditor("commit_message"))
        {
            templateSelector->onChange = [this, templateSelector, messageEditor]
            {
                const auto renderedTemplate = renderCommitTemplate(templateSelector->getText(), audioProcessor);
                if (renderedTemplate.isNotEmpty())
                    messageEditor->setText(renderedTemplate, juce::dontSendNotification);
            };
        }
    }

    const auto popupRef = juce::Component::SafePointer<juce::AlertWindow>(commitPopup);
    const auto editorRef = juce::Component::SafePointer<StemhubAudioProcessorEditor>(this);
    commitPopup->enterModalState(true, juce::ModalCallbackFunction::create([editorRef, popupRef](int result)
    {
        if (result != 1 || popupRef == nullptr || editorRef == nullptr)
            return;

        const auto commitMessage = popupRef->getTextEditorContents("commit_message").trim();
        const auto selectedTemplate = popupRef->getComboBoxComponent("template") != nullptr
            ? popupRef->getComboBoxComponent("template")->getText()
            : juce::String(kDefaultCommitTemplate);
        editorRef->dashboardView.setCommitTemplates(getCommitTemplateNames(), selectedTemplate);
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
