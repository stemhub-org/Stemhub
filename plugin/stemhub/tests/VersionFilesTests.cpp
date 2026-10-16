#include "application/RestoreFolders.hpp"
#include "application/VersionFiles.hpp"
#include "domain/Manifest.hpp"
#include "support/TestSupport.hpp"

namespace
{
using namespace stemhub::test;

class VersionFilesTests final : public StemhubTest
{
public:
    VersionFilesTests() : StemhubTest("Stemhub version files") {}

    void runTest() override
    {
        beginTest("Manifest paths must stay inside the restore folder");
        {
            for (const auto* path : { "song.flp", "Drums/kick.wav", "Samples/Imported/Kick 01.wav", "a.b/c.wav" })
                expect(stemhub::manifest::isSafePath(path), juce::String("should accept ") + path);

            for (const auto* path : { "", "/etc/passwd", "../evil.wav", "Drums/../../evil.wav", "./song.flp",
                                      "a//b.wav", "a/b/", "C:\\evil.wav", "C:evil.wav", "..\\evil.wav",
                                      "Drums\\kick.wav", "trailing.", "trailing ", "NUL.wav", "Samples/con",
                                      "bad\nname.wav" })
                expect(!stemhub::manifest::isSafePath(path), juce::String("should reject ") + juce::String(path).quoted());

            expect(stemhub::manifest::isSafePath(juce::String::repeatedString("a", 255)), "255 characters fit the backend limit");
            expect(!stemhub::manifest::isSafePath(juce::String::repeatedString("a", 256)), "256 characters exceed the backend limit");
        }

        beginTest("Manifests with unsafe or conflicting entries are rejected, in either format");
        {
            const auto shaA = juce::String::repeatedString("a", 64);
            const auto shaB = juce::String::repeatedString("b", 64);

            namespace manifest = stemhub::manifest;

            for (const auto format : { 1, 2 })
            {
                const auto inFormat = " (manifest v" + juce::String(format) + ")";
                manifest::Manifest parsed;
                auto result = manifest::fromJson(makeManifest("../../evil.flp", shaA, {}, format), parsed);
                expect(result.failed() && result.getErrorMessage().contains("unsafe"), "traversal in the project file is rejected" + inFormat);

                result = manifest::fromJson(
                    makeManifest("song.flp", shaA, { { "Drums/../../evil.wav", shaB } }, format), parsed);
                expect(result.failed() && result.getErrorMessage().contains("unsafe"), "traversal in an asset is rejected" + inFormat);

                result = manifest::fromJson(
                    makeManifest("song.flp", shaA, { { "Drums/kick.wav", shaA }, { "drums/KICK.wav", shaB } }, format), parsed);
                expect(result.failed() && result.getErrorMessage().contains("two different files"),
                       "two different files at the same path are rejected" + inFormat);

                result = manifest::fromJson(
                    makeManifest("song.flp", shaA, { { "Drums/kick.wav", shaB }, { "Drums/kick.wav", shaB } }, format), parsed);
                expect(result.wasOk() && parsed.assets.size() == 1, "an exact duplicate is kept once" + inFormat);

                result = manifest::fromJson(
                    makeManifest("song.flp", "../" + shaA.substring(3), {}, format), parsed);
                expect(result.failed(), "a hash that is not hex is rejected" + inFormat);

                auto negativeSize = makeManifest("song.flp", shaA, {}, format);
                negativeSize.getProperty("project_file", {}).getDynamicObject()->setProperty("size_bytes", -1);
                expect(manifest::fromJson(negativeSize, parsed).failed(), "a negative size is rejected" + inFormat);
            }
        }

        beginTest("A manifest is written in format 2, and reads back what was written");
        {
            namespace manifest = stemhub::manifest;

            manifest::Manifest written;
            written.sourceDaw = "Ableton Live";
            written.projectFile = { juce::String::repeatedString("a", 64), 10, "song.als" };
            written.assets = { { juce::String::repeatedString("b", 64), 5, "Samples/Imported/kick 01.wav" },
                               { juce::String::repeatedString("c", 64), 7, "loop.aif" } };

            const auto json = manifest::toJson(written);
            expect(static_cast<int>(json.getProperty("manifest_version", 0)) == 2, "the plugin writes manifest v2");
            expect(json.getProperty("source_daw", {}).toString() == "Ableton Live");
            expect(json.getProperty("source_project_filename", {}).toString() == "song.als");

            const auto projectFile = json.getProperty("project_file", {});
            expect(projectFile.getProperty("path", {}).toString() == "song.als"
                       && static_cast<int>(projectFile.getProperty("size_bytes", 0)) == 10
                       && projectFile.getProperty("sha256", {}).toString() == written.projectFile.sha256,
                   "the project file is listed by path, size and hash");

            const auto* assets = json.getProperty("assets", {}).getArray();
            expect(assets != nullptr && assets->size() == 2, "the assets are listed under \"assets\"");
            if (assets != nullptr && assets->size() == 2)
                expect(assets->getReference(0).getProperty("path", {}).toString() == "Samples/Imported/kick 01.wav"
                           && static_cast<int>(assets->getReference(1).getProperty("size_bytes", 0)) == 7,
                       "each asset by path, size and hash");

            for (const auto* legacyKey : { "tracks", "filename", "name" })
            {
                const auto hasKey = json.hasProperty(legacyKey) || projectFile.hasProperty(legacyKey)
                                 || (assets != nullptr && !assets->isEmpty() && assets->getReference(0).hasProperty(legacyKey));
                expect(!hasKey, juce::String("format 2 has no \"") + legacyKey + "\"");
            }

            expect(manifest::totalSize(json) == 22, "the history shows the files' total size");

            manifest::Manifest read;
            expect(manifest::fromJson(json, read).wasOk());
            expect(read.sourceDaw == written.sourceDaw && read.projectFile.path == "song.als" && read.projectFile.sizeBytes == 10);
            expect(read.assets.size() == 2 && read.assets[0].path == "Samples/Imported/kick 01.wav"
                       && read.assets[1].sha256 == written.assets[1].sha256);
        }

        beginTest("A manifest earlier plugins wrote, in format 1, still reads");
        {
            namespace manifest = stemhub::manifest;

            const auto shaA = juce::String::repeatedString("a", 64);
            const auto shaB = juce::String::repeatedString("b", 64);
            const auto json = juce::JSON::parse(R"({
                "manifest_version": 1, "source_daw": "FL Studio", "source_project_filename": "song.flp",
                "project_file": { "sha256": ")" + shaA + R"(", "size_bytes": 10, "filename": "song.flp" },
                "tracks": [ { "sha256": ")" + shaB + R"(", "size_bytes": 5, "filename": "Drums/kick.wav", "name": "kick",
                              "bpm": null, "key": null, "duration_seconds": null } ],
                "mixer_state": null
            })");

            manifest::Manifest read;
            const auto result = manifest::fromJson(json, read);
            expect(result.wasOk(), result.getErrorMessage());
            expect(read.sourceDaw == "FL Studio" && read.projectFile.path == "song.flp" && read.projectFile.sha256 == shaA
                       && read.projectFile.sizeBytes == 10,
                   "the project file of a v1 manifest");
            expect(read.assets.size() == 1 && read.assets[0].path == "Drums/kick.wav" && read.assets[0].sha256 == shaB
                       && read.assets[0].sizeBytes == 5,
                   "its tracks are the assets");
            expect(manifest::totalSize(json) == 15, "the history sizes a v1 manifest too");
        }

        beginTest("A manifest in an unknown format is refused");
        {
            namespace manifest = stemhub::manifest;

            const auto shaA = juce::String::repeatedString("a", 64);
            for (const auto format : { 0, 3 })
            {
                auto json = makeManifest("song.flp", shaA, {});
                json.getDynamicObject()->setProperty("manifest_version", format);

                manifest::Manifest read;
                const auto result = manifest::fromJson(json, read);
                expect(result.failed() && result.getErrorMessage().contains("unsupported format"),
                       "manifest_version " + juce::String(format) + ": " + result.getErrorMessage());
            }

            auto withoutVersion = makeManifest("song.flp", shaA, {});
            withoutVersion.getDynamicObject()->removeProperty("manifest_version");
            manifest::Manifest read;
            expect(manifest::fromJson(withoutVersion, read).failed(), "a manifest without a version is refused");

            // A v2 manifest can't borrow v1's keys, nor the other way round.
            expect(manifest::fromJson(makeManifest("song.flp", shaA, { { "kick.wav", shaA } }, 1), read).wasOk()
                       && read.assets.size() == 1,
                   "v1 as written by earlier plugins");
            auto mixed = makeManifest("song.flp", shaA, { { "kick.wav", shaA } }, 1);
            mixed.getDynamicObject()->setProperty("manifest_version", 2);
            expect(manifest::fromJson(mixed, read).failed(), "v1 keys under manifest_version 2 are refused");
        }

        beginTest("Files are hashed in blocks, and a stopped job stops hashing");
        {
            TestEnvironment environment;
            const auto file = environment.root.getChildFile("loop.wav");
            juce::MemoryBlock content((3 << 20) + 17);
            auto* bytes = static_cast<juce::uint8*>(content.getData());
            for (size_t index = 0; index < content.getSize(); ++index)
                bytes[index] = static_cast<juce::uint8>(index * 31 + 7);
            expect(file.replaceWithData(content.getData(), content.getSize()));

            expect(stemhub::versionfiles::sha256OfFile(file) == sha256Of(content), "the same digest across block boundaries");
            expect(stemhub::versionfiles::sha256OfFile(environment.root.getChildFile("missing.wav")).isEmpty(), "a missing file has none");

            BlockingGate gate;
            BackgroundJobCoordinator<juce::String> jobs { 1, [] {} };
            jobs.enqueue([&gate, file](const auto&)
            {
                gate.block();
                return stemhub::versionfiles::sha256OfFile(file);
            });
            expect(gate.waitUntilEntered(), "the hash should start");
            jobs.stopRunningJobs();
            gate.release();

            std::vector<juce::String> hashes;
            const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32>(kWaitTimeoutMs);
            while (hashes.empty() && juce::Time::getMillisecondCounter() < deadline)
            {
                hashes = jobs.takeResults();
                juce::Thread::sleep(5);
            }

            expect(hashes.size() == 1 && hashes.front().isEmpty(), "a stopped job gets no hash");
        }

        beginTest("A save takes the project's assets, and leaves out backups and restored copies");
        {
            namespace versionfiles = stemhub::versionfiles;

            TestEnvironment environment;
            const auto folder = environment.root.getChildFile("Song");
            const auto projectFile = folder.getChildFile("song.flp");
            const auto write = [&folder](const juce::String& path, const juce::String& content)
            {
                const auto file = folder.getChildFile(path);
                return file.create().wasOk() && file.replaceWithText(content);
            };

            expect(write("song.flp", "flp") && write("Drums/kick.wav", "kick") && write("Loops/loop 01.aif", "loop")
                   && write("mix-deadbeef/vocal.wav", "vocal") && write("notes.txt", "notes")
                   && write("Backup/song overwritten.flp", "old") && write("backup/old kick.wav", "old kick")
                   && write(".hidden.wav", "hidden"));

            // A copy the plugin restored here is marked as such; a folder merely named like one is
            // the user's.
            expect(write("song-0123abcd/" + juce::String(versionfiles::kRestoredCopyMarker), "version-1")
                   && write("song-0123abcd/song.flp", "restored") && write("song-0123abcd/Drums/kick.wav", "kick")
                   && write("Drums-20240101/idea.flp", "idea") && write("Drums-20240101/snare.wav", "snare"));

            const auto files = versionfiles::collect(projectFile);
            juce::StringArray paths;
            for (const auto& file : files)
                paths.add(file.getRelativePathFrom(folder).replaceCharacter('\\', '/'));

            expect(paths.joinIntoString(", ")
                       == "song.flp, Drums-20240101/snare.wav, Drums/kick.wav, Loops/loop 01.aif, mix-deadbeef/vocal.wav",
                   "project file first, then its assets by path: " + paths.joinIntoString(", "));

            const auto summary = versionfiles::summarize(projectFile);
            expect(summary.fileCount == 5 && summary.totalBytes == 3 + 5 + 4 + 4 + 5, juce::String(summary.totalBytes));
            expect(versionfiles::collect(folder.getChildFile("missing.flp")).empty(), "no project file, nothing to save");

            expect(versionfiles::dawNameFor(projectFile) == "FL Studio");
            expect(versionfiles::dawNameFor(folder.getChildFile("song.als")) == "Ableton Live");
            expect(versionfiles::dawNameFor(folder.getChildFile("song.ptx")).isEmpty());
        }

        beginTest("Projects restored on opening go under the project's and branch's names");
        {
            namespace restorefolders = stemhub::restorefolders;

            const auto base = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("StemHub");
            const auto root = restorefolders::projectRoot(base, makeProject("project-1", " AC/DC: Live? "),
                                                      makeBranch("branch-1", "project-1", "main"));
            expect(root == base.getChildFile("ACDC Live").getChildFile("main"), root.getFullPathName());

            const auto fallback = restorefolders::projectRoot(base, makeProject("project-1", ".."),
                                                          makeBranch("branch/1", "project-1", "  "));
            expect(fallback == base.getChildFile("project-1").getChildFile("branch-1"),
                   "a name that can't be a folder gives way to the id: " + fallback.getFullPathName());
        }

        beginTest("A working-copy baseline only vouches for what it recorded");
        {
            TestEnvironment environment;
            const auto file = environment.root.getChildFile("song.flp");
            expect(file.replaceWithText("flp"));

            const WorkingCopyBaseline guessed { file, "version-1" };
            expect(guessed.isSet() && !guessed.hasRecordedState(), "a version without recorded state");
            expect(!guessed.isUnchanged(), "an unknown state counts as changed");

            const auto recorded = WorkingCopyBaseline::recordedNow(file, "version-1");
            expect(recorded.isUnchanged(), "unchanged right after recording");
            simulateDawSave(file, " edit");
            expect(!recorded.isUnchanged(), "a DAW save is a change");

            expect(!WorkingCopyBaseline {}.isSet() && !WorkingCopyBaseline {}.describes(file), "an empty baseline describes nothing");
        }

        beginTest("Collecting a project's files stops when its job is asked to stop");
        {
            TestEnvironment environment;
            const auto projectFile = environment.root.getChildFile("song.flp");
            expect(projectFile.replaceWithText("flp"));
            for (int index = 0; index < 20; ++index)
                expect(environment.root.getChildFile("loop" + juce::String(index) + ".wav").replaceWithText("loop"));

            // Like the plugin window's file count when the window closes.
            BlockingGate gate;
            BackgroundJobCoordinator<size_t> jobs { 1, [] {} };
            jobs.enqueue([&gate, projectFile](const auto&)
            {
                gate.block();
                return stemhub::versionfiles::collect(projectFile).size();
            });
            expect(gate.waitUntilEntered(), "the count should start");
            jobs.stopRunningJobs();
            gate.release();

            std::vector<size_t> counts;
            const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32>(kWaitTimeoutMs);
            while (counts.empty() && juce::Time::getMillisecondCounter() < deadline)
            {
                counts = jobs.takeResults();
                juce::Thread::sleep(5);
            }

            expect(counts.size() == 1 && counts.front() == 0,
                   "a stopped count walks no further: " + juce::String(counts.empty() ? -1 : static_cast<int>(counts.front())));
        }
    }
};

VersionFilesTests versionFilesTests;
}
