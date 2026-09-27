#include <memory>

#include "support/TestSupport.hpp"

namespace
{
using namespace stemhub::test;

class SaveRestoreTests final : public StemhubTest
{
public:
    SaveRestoreTests() : StemhubTest("Stemhub saves and restores") {}

    void runTest() override
    {
        beginTest("A save shows its progress and can be cancelled before its version exists");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { makeBranch("branch-1", project.id, "main") };

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp"));
            expect(context.environment.root.getChildFile("kick.wav").replaceWithText("kick"));
            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            auto gate = std::make_shared<BlockingGate>();
            context.api->setCheckMissingGate(project.id, gate);
            context.session.requestPushVersion("first");
            expectEntered(*gate, "the save");
            expect(waitUntil(context.session, [&context] { return context.state().sessionStatus.text == "Preparing 2 of 2 files..."; }),
                   "the save says how far it got: " + describe(context.session));

            context.session.cancelRequest();
            expect(context.state().sessionStatus.text == "Cancelling..." && context.session.isBusy(),
                   "it stops at its next step: " + describe(context.session));

            gate->release();
            expect(waitUntil(context.session, [&context] { return context.isIdle(); }), describe(context.session));
            expect(context.state().sessionStatus.severity == Status::Severity::warning
                       && context.state().sessionStatus.text == "Save cancelled.",
                   describe(context.session));
            expect(context.api->getCreatedVersions().empty(), "no version is created");
            expect(context.state().versionHistory.empty() && !context.state().workingCopy.isSet(), "nothing changed");
            expect(context.state().lastSavedVersionId.isEmpty(), "no saved version, so the note is kept for the retry");

            context.session.requestPushVersion("again");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().versionHistory.size() == 1; }),
                   "the next save works: " + describe(context.session));
            expect(context.state().lastSavedVersionId == context.api->getCreatedVersions().front().id,
                   "the saved version is announced, which clears the note");
        }

        beginTest("A cancelled restore leaves nothing behind");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };
            const auto versionId = context.api->addVersion(branch.id, "first", { { "song.flp", "flp" }, { "Drums/kick.wav", "kick" } });

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("mine"));
            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            auto gate = std::make_shared<BlockingGate>();
            context.api->setDownloadGate(project.id, gate);
            const auto restoresFolder = context.environment.root.getChildFile("restores");
            expect(restoresFolder.createDirectory().wasOk());
            context.session.requestRestoreVersion(versionId, restoresFolder);
            expectEntered(*gate, "the download");
            expect(waitUntil(context.session, [&context] { return context.state().sessionStatus.text == "Downloading 1 of 2 files..."; }),
                   describe(context.session));

            context.session.cancelRequest();
            gate->release();
            expect(waitUntil(context.session, [&context] { return context.isIdle(); }), describe(context.session));
            expect(context.state().sessionStatus.text == "Restore cancelled.", describe(context.session));

            juce::Array<juce::File> leftovers;
            restoresFolder.findChildFiles(leftovers, juce::File::findFilesAndDirectories, true);
            expect(leftovers.isEmpty(), "the folder the restore created is removed");
            expect(context.openedFiles.isEmpty() && !context.handoffFile().exists(), "nothing is handed to the DAW");
        }

        beginTest("Saves chain their parent versions and restores never delete earlier ones");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };

            const auto sourceFolder = context.environment.root.getChildFile("source");
            const auto projectFile = sourceFolder.getChildFile("song.flp");
            expect(sourceFolder.getChildFile("Drums").createDirectory().wasOk());
            expect(projectFile.replaceWithText("flp v1"));
            expect(sourceFolder.getChildFile("Drums/kick.wav").replaceWithText("kick"));
            expect(sourceFolder.getChildFile("kick.wav").replaceWithText("a different kick"));

            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            context.session.requestPushVersion("first");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().versionHistory.size() == 1; }),
                   "first save should land in the history: " + describe(context.session));
            const auto firstVersionId = context.api->getCreatedVersions().at(0).id;
            expect(context.state().sessionStatus.severity == Status::Severity::success, describe(context.session));
            expect(context.state().selectedVersionId == firstVersionId, "the saved version should be selected");
            expect(context.state().openedVersionId == firstVersionId, "the saved version is the one in the DAW");

            // Restore into a folder of its own, keeping subfolders.
            const auto restoresFolder = context.environment.root.getChildFile("restores");
            expect(restoresFolder.createDirectory().wasOk());
            context.session.requestRestoreVersion(firstVersionId, restoresFolder);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.openedFiles.size() == 1; }),
                   "restore should finish: " + describe(context.session));
            const auto restoredFile = context.openedFiles.getFirst();
            const auto restoredFolder = restoredFile.getParentDirectory();
            expect(restoredFile.loadFileAsString() == "flp v1", restoredFile.getFullPathName());
            expect(restoredFolder.getParentDirectory() == restoresFolder
                       && restoredFolder.getFileName() == "song-" + firstVersionId.substring(0, 8),
                   restoredFolder.getFullPathName());
            expect(restoredFolder.getChildFile("Drums/kick.wav").loadFileAsString() == "kick", "nested files keep their folder");
            expect(restoredFolder.getChildFile("kick.wav").loadFileAsString() == "a different kick", "same-name files don't collide");
            expect(context.state().selectedProjectFile == projectFile, "this instance stays with its own project file");
            expect(context.handoffFile().existsAsFile(), "the restored copy waits for the instance the DAW opens");

            // The DAW opens the copy with a plugin instance of its own. Its saved state is from
            // before the first save, so it names the original file.
            auto restoredInstance = context.makeInstance();
            restoredInstance->restoreLink({ project.id, branch.id, projectFile });
            expect(!context.handoffFile().exists(), "the hand-off is taken once");
            restoreSavedSession(*restoredInstance);
            const auto& restoredState = restoredInstance->getState();
            expect(restoredState.selectedProjectFile == restoredFile, "the copy is that instance's working file: " + describe(*restoredInstance));
            expect(restoredState.link.workingFile == restoredFile, "and what its DAW project will be saved with");
            expect(restoredState.openedVersionId == firstVersionId, "the restored version is the one in the DAW");

            restoredInstance->requestPushVersion("nothing new");
            expect(restoredState.sessionStatus.severity == Status::Severity::warning
                       && restoredState.sessionStatus.text.contains("No changes"),
                   "an unchanged copy is not saved again: " + describe(*restoredInstance));

            // Saving the copy chains from the restored version.
            simulateDawSave(restoredFile, " edit 1");
            restoredInstance->requestPushVersion("second");
            expect(waitUntil(*restoredInstance, [&restoredInstance] { return !restoredInstance->isBusy()
                                                                            && restoredInstance->getState().versionHistory.size() == 2; }),
                   "second save should land in the history: " + describe(*restoredInstance));
            auto created = context.api->getCreatedVersions();
            expect(created.size() == 2 && created[1].parentVersionId == firstVersionId,
                   "the second version's parent is the restored one");
            const auto secondVersionId = created[1].id;
            expect(restoredState.versionHistory.front().id == secondVersionId, "history is refreshed after a save");
            expect(restoredState.selectedVersionId == secondVersionId, "the new version is selected");

            // Nothing changed on disk: no new version, even after a refresh of the same branch.
            restoredInstance->requestRefreshVersionHistory();
            expect(waitUntil(*restoredInstance, [&restoredInstance] { return !restoredInstance->isBusy(); }), "refresh should finish");
            restoredInstance->requestPushVersion("no changes");
            expect(restoredState.sessionStatus.text.contains("No changes"), describe(*restoredInstance));
            expect(context.api->getCreatedVersions().size() == 2, "no version is created for an unchanged file");

            simulateDawSave(restoredFile, " edit 2");
            restoredInstance->requestPushVersion("third");
            expect(waitUntil(*restoredInstance, [&restoredInstance] { return !restoredInstance->isBusy()
                                                                            && restoredInstance->getState().versionHistory.size() == 3; }),
                   "third save should land in the history: " + describe(*restoredInstance));
            created = context.api->getCreatedVersions();
            expect(created.size() == 3 && created[2].parentVersionId == secondVersionId,
                   "each save's parent is the previous save");

            // Restoring the same version again goes to a new folder; the edited copy stays.
            restoredInstance->requestRestoreVersion(firstVersionId, restoresFolder);
            expect(waitUntil(*restoredInstance, [&context, &restoredInstance] { return !restoredInstance->isBusy()
                                                                                      && context.openedFiles.size() == 2; }),
                   "second restore should finish: " + describe(*restoredInstance));
            expect(restoredFile.loadFileAsString() == "flp v1 edit 1 edit 2", "an earlier restore is never deleted");
            expect(context.openedFiles.getLast().getParentDirectory().getFileName() == restoredFolder.getFileName() + " (2)",
                   "the new folder is numbered: " + context.openedFiles.getLast().getFullPathName());
            expect(restoredState.selectedProjectFile == restoredFile, "the instance keeps working on its copy");
        }

        beginTest("A save leaves out a copy restored into its own folder");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { makeBranch("branch-1", project.id, "main") };

            const auto folder = context.environment.root.getChildFile("Song");
            const auto projectFile = folder.getChildFile("song.flp");
            expect(folder.getChildFile("kick.wav").create().wasOk() && folder.getChildFile("kick.wav").replaceWithText("kick"));
            expect(projectFile.replaceWithText("flp v1"));
            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            context.session.requestPushVersion("first");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().versionHistory.size() == 1; }),
                   describe(context.session));
            const auto firstVersionId = context.state().workingCopy.versionId;

            // Where the dashboard restores by default: next to the project file.
            context.session.requestRestoreVersion(firstVersionId, folder);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.openedFiles.size() == 1; }),
                   describe(context.session));
            expect(context.openedFiles.getFirst().isAChildOf(folder), context.openedFiles.getFirst().getFullPathName());

            simulateDawSave(projectFile, " edit");
            context.session.requestPushVersion("second");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().versionHistory.size() == 2; }),
                   describe(context.session));

            const auto manifest = context.api->fetchVersionManifest(context.state().workingCopy.versionId, "token");
            expect(manifest.ok(), "the second version has a manifest");
            juce::StringArray trackPaths;
            if (manifest.ok())
                if (const auto* tracks = manifest.value->getProperty("tracks", {}).getArray())
                    for (const auto& track : *tracks)
                        trackPaths.add(track.getProperty("filename", {}).toString());

            expect(trackPaths.joinIntoString(", ") == "kick.wav", "only the project's own audio: " + trackPaths.joinIntoString(", "));
        }

        beginTest("Only one save runs at a time, and the project stays open meanwhile");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { makeBranch("branch-1", project.id, "main") };

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp"));
            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            auto gate = std::make_shared<BlockingGate>();
            context.api->setCheckMissingGate(project.id, gate);
            context.session.requestPushVersion("first");
            expectEntered(*gate, "the first save");
            context.session.requestPushVersion("second");
            context.session.requestRefreshVersionHistory();
            context.session.showProjectSelection();
            expect(context.state().operationState == OperationState::committing
                       && context.state().uiState == UIState::dashboard,
                   "nothing else starts while saving: " + describe(context.session));

            gate->release();
            expect(waitUntil(context.session, [&context] { return context.isIdle(); }), "save should finish");
            const auto created = context.api->getCreatedVersions();
            expect(created.size() == 1 && created.front().commitMessage == "first", "the second save request is ignored");

            context.session.showProjectSelection();
            expect(context.state().uiState == UIState::projectSelection, "back to the grid once the save is done");
        }

        beginTest("Opening a project without a local copy restores its latest version");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };
            const auto versionId = context.api->addVersion(branch.id, "from a collaborator",
                                                           { { "song.flp", "flp" }, { "Samples/kick.wav", "kick" } });

            signIn(context.session);
            context.session.requestOpenProject(project.id, true);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.openedFiles.size() == 1; }),
                   "the latest version should be restored: " + describe(context.session));

            const auto restoredFile = context.openedFiles.getFirst();
            expect(restoredFile.isAChildOf(context.environment.root.getChildFile("managed")), restoredFile.getFullPathName());
            expect(restoredFile.getParentDirectory().getChildFile("Samples/kick.wav").loadFileAsString() == "kick");
            expect(context.state().selectedProject.has_value() && context.state().selectedProjectFile == juce::File(),
                   "this instance had no copy of its own and still has none");
            expect(context.state().projectsStatus.isEmpty(), "the grid's progress message is cleared");

            // The DAW opens the copy. Saved by an earlier version, its plugin state has no link;
            // some hosts hand that empty state over twice.
            auto restoredInstance = context.makeInstance();
            restoredInstance->restoreLink({});
            restoredInstance->restoreLink({});
            expect(restoredInstance->getState().link.workingFile == restoredFile, "the hand-off is taken and kept");
            restoreSavedSession(*restoredInstance);
            const auto& restoredState = restoredInstance->getState();
            expect(restoredState.selectedProject.has_value() && restoredState.selectedProject->id == project.id,
                   "the instance takes the hand-off: " + describe(*restoredInstance));
            expect(restoredState.selectedProjectFile == restoredFile, "and works on the restored copy");
            expect(restoredState.openedVersionId == versionId, "the restored version is the one in the DAW");
        }

        beginTest("Opening a project never replaces unsaved local changes");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp v1"));
            signIn(context.session);
            openProject(context.session, project.id, projectFile);
            context.session.requestPushVersion("first");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().versionHistory.size() == 1; }),
                   "the first version should be saved: " + describe(context.session));

            // Someone saves a newer version while this copy has unsaved edits.
            simulateDawSave(projectFile, " my edit");
            context.api->addVersion(branch.id, "second", { { "song.flp", "flp v2" } });

            context.session.showProjectSelection();
            context.session.requestOpenProject(project.id, true);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().versionHistory.size() == 2; }),
                   "the project should reopen: " + describe(context.session));
            expect(context.state().selectedProjectFile == projectFile, "the edited copy stays the working file");
            expect(projectFile.loadFileAsString() == "flp v1 my edit", "the edited copy is untouched");
            expect(context.state().sessionStatus.severity == Status::Severity::warning
                       && context.state().sessionStatus.text.contains("not saved"),
                   describe(context.session));
            expect(context.openedFiles.isEmpty() && !context.handoffFile().exists(), "nothing is restored or opened");
        }

        beginTest("Opening a project updates an unchanged older copy");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp v1"));
            signIn(context.session);
            openProject(context.session, project.id, projectFile);
            context.session.requestPushVersion("first");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().versionHistory.size() == 1; }),
                   "the first version should be saved: " + describe(context.session));
            const auto firstVersionId = context.state().workingCopy.versionId;

            const auto newerVersionId = context.api->addVersion(branch.id, "second", { { "song.flp", "flp v2" } });
            context.session.showProjectSelection();
            context.session.requestOpenProject(project.id, true);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.openedFiles.size() == 1; }),
                   "the newer version should be restored: " + describe(context.session));
            const auto newerCopy = context.openedFiles.getFirst();
            expect(newerCopy.loadFileAsString() == "flp v2" && newerCopy.isAChildOf(context.environment.root.getChildFile("managed")),
                   newerCopy.getFullPathName());
            expect(context.state().selectedProjectFile == projectFile && projectFile.loadFileAsString() == "flp v1",
                   "this instance keeps its older copy");
            expect(context.state().openedVersionId == firstVersionId, describe(context.session));

            auto newerInstance = context.makeInstance();
            newerInstance->restoreLink({ project.id, branch.id, projectFile });
            restoreSavedSession(*newerInstance);
            expect(newerInstance->getState().selectedProjectFile == newerCopy, describe(*newerInstance));
            expect(newerInstance->getState().openedVersionId == newerVersionId);
        }

        beginTest("A restore the DAW can't open says where the copy is");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };
            const auto versionId = context.api->addVersion(branch.id, "first", { { "song.flp", "flp v1" } });
            context.canOpenFiles = false;

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("mine"));
            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            context.session.requestRestoreVersion(versionId, context.environment.root);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.openedFiles.size() == 1; }),
                   "the restore should finish: " + describe(context.session));

            const auto restoredFile = context.openedFiles.getFirst();
            expect(restoredFile.loadFileAsString() == "flp v1", restoredFile.getFullPathName());
            expect(context.state().sessionStatus.severity == Status::Severity::warning
                       && context.state().sessionStatus.text.contains(restoredFile.getFullPathName()),
                   "the user is told where the restored project is: " + describe(context.session));
            expect(context.state().selectedProjectFile == projectFile, "this instance keeps its own file");
            expect(context.handoffFile().existsAsFile(), "the copy waits for the user to open it");

            // Opened by hand, with no link saved in it: the instance picks the copy up when its
            // window opens.
            auto openedByHand = context.makeInstance();
            restoreSavedSession(*openedByHand);
            expect(openedByHand->getState().selectedProjectFile == restoredFile, describe(*openedByHand));
            expect(openedByHand->getState().workingCopy.versionId == versionId, "it knows which version the copy holds");
        }

        beginTest("A restore folder's version is the next parent but never blocks a save");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };
            const auto restoredVersionId = context.api->addVersion(branch.id, "first", { { "song.flp", "flp v1" } });
            context.api->addVersion(branch.id, "second", { { "song.flp", "flp v2" } });

            // A copy restored in an earlier session: only its folder name says which version it is.
            const auto restoredFile = context.environment.root
                                          .getChildFile("song-" + restoredVersionId.substring(0, 8))
                                          .getChildFile("song.flp");
            expect(restoredFile.getParentDirectory().createDirectory().wasOk());
            expect(restoredFile.replaceWithText("flp v1"));

            signIn(context.session);
            openProject(context.session, project.id, restoredFile);

            context.session.requestPushVersion("from the restored copy");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.api->getCreatedVersions().size() == 3; }),
                   "the save must not be refused: " + describe(context.session));
            expect(context.api->getCreatedVersions().back().parentVersionId == restoredVersionId,
                   "the restored version is the parent, not the branch head");
        }

        beginTest("Each distinct file is uploaded once");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { makeBranch("branch-1", project.id, "main") };

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp"));
            expect(context.environment.root.getChildFile("Drums/kick.wav").create().wasOk());
            expect(context.environment.root.getChildFile("Drums/kick.wav").replaceWithText("kick"));
            expect(context.environment.root.getChildFile("Backup/kick copy.wav").create().wasOk());
            expect(context.environment.root.getChildFile("kick copy.wav").replaceWithText("kick"));

            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            context.session.requestPushVersion("first");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.api->getCreatedVersions().size() == 1; }),
                   "first save: " + describe(context.session));
            const auto kickSha = sha256Of(juce::MemoryBlock("kick", 4));
            expect(context.api->getUploadCount(kickSha) == 1, "two identical files are one upload");

            simulateDawSave(projectFile, " edit");
            context.session.requestPushVersion("second");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.api->getCreatedVersions().size() == 2; }),
                   "second save: " + describe(context.session));
            expect(context.api->getUploadCount(kickSha) == 1, "files the server has are not uploaded again");
        }

        beginTest("Saves beyond the backend limits are refused before uploading");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { makeBranch("branch-1", project.id, "main") };

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp"));
            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            context.session.requestPushVersion(juce::String::repeatedString("n", 501));
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().sessionStatus.isError(); }),
                   "a long note should fail: " + describe(context.session));
            expect(context.state().sessionStatus.text.contains("500 characters"), describe(context.session));

            for (int index = 0; index < 501; ++index)
                expect(context.environment.root.getChildFile("stem" + juce::String(index) + ".wav").replaceWithText(juce::String(index)));

            context.session.requestPushVersion("too many files");
            expect(waitUntil(context.session, [&context]
            {
                return context.isIdle() && context.state().sessionStatus.text.contains("can hold 500");
            }), "501 audio files should fail: " + describe(context.session));
            expect(context.state().sessionStatus.isError(), describe(context.session));
            expect(context.api->getCreatedVersions().empty() && context.api->getUploadCount(sha256Of(juce::MemoryBlock("0", 1))) == 0,
                   "nothing is uploaded");
        }

        beginTest("A download that fails its checksum leaves nothing behind");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };
            const auto versionId = context.api->addVersion(branch.id, "first", { { "song.flp", "flp" }, { "Drums/kick.wav", "kick" } });
            context.api->corruptDownloads = true;

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("mine"));
            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            const auto restoresFolder = context.environment.root.getChildFile("restores");
            expect(restoresFolder.createDirectory().wasOk());
            context.session.requestRestoreVersion(versionId, restoresFolder);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().sessionStatus.isError(); }),
                   "the restore should fail: " + describe(context.session));
            expect(context.state().sessionStatus.text.contains("checksum"), describe(context.session));

            juce::Array<juce::File> leftovers;
            restoresFolder.findChildFiles(leftovers, juce::File::findFilesAndDirectories, true);
            expect(leftovers.isEmpty(), "the half-restored folder is removed");
            expect(context.state().selectedProjectFile == projectFile, "the working file doesn't change");
            expect(context.openedFiles.isEmpty() && !context.handoffFile().exists(), "nothing is handed to the DAW");
        }

        beginTest("A restore hand-off survives the plugin window opening before the host's saved state");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            const auto branch = makeBranch("branch-1", project.id, "main");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { branch };
            const auto versionId = context.api->addVersion(branch.id, "first", { { "song.flp", "flp v1" } });

            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("mine"));
            signIn(context.session);
            openProject(context.session, project.id, projectFile);
            context.session.requestRestoreVersion(versionId, context.environment.root);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.openedFiles.size() == 1; }),
                   describe(context.session));
            const auto restoredFile = context.openedFiles.getFirst();

            // The DAW opens the copy and shows the plugin window, which was open when the project was
            // saved, before it hands the new instance its saved state.
            auto restoredInstance = context.makeInstance();
            restoredInstance->requestRestoreSavedSession();
            restoredInstance->restoreLink({ project.id, branch.id, projectFile });
            expect(waitUntil(*restoredInstance, [&restoredInstance]
            {
                return !restoredInstance->isBusy() && restoredInstance->getState().selectedProject.has_value();
            }), describe(*restoredInstance));
            expect(restoredInstance->getState().selectedProjectFile == restoredFile,
                   "the instance works on the restored copy: " + restoredInstance->getState().selectedProjectFile.getFullPathName());
            expect(restoredInstance->getState().openedVersionId == versionId, "and knows which version it holds");
        }

        beginTest("A failed save or restore says what failed, once");
        {
            TestContext context;
            const auto project = makeProject("project-1", "Song");
            context.api->projects = { project };
            context.api->projectBranches[project.id] = { makeBranch("branch-1", project.id, "main") };
            const auto projectFile = context.environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp"));
            signIn(context.session);
            openProject(context.session, project.id, projectFile);

            // What ApiClient reports when the server gives no detail.
            context.api->createVersionError = "Failed to create the version.";
            context.session.requestPushVersion("first");
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().sessionStatus.isError(); }),
                   describe(context.session));
            expect(context.state().sessionStatus.text == "Failed to create the version.", context.state().sessionStatus.text);

            // A version deleted since the history was loaded.
            context.session.requestRestoreVersion("deleted-version", context.environment.root);
            expect(waitUntil(context.session, [&context] { return context.isIdle() && context.state().sessionStatus.isError(); }),
                   describe(context.session));
            expect(context.state().sessionStatus.text.contains("Version not found")
                       && !context.state().sessionStatus.text.contains("file list"),
                   context.state().sessionStatus.text);
        }
    }
};

SaveRestoreTests saveRestoreTests;
}
