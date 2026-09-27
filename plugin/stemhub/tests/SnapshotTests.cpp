#include "application/SnapshotFiles.hpp"
#include "domain/Manifest.hpp"
#include "support/TestSupport.hpp"

namespace
{
using namespace stemhub::test;

class SnapshotTests final : public StemhubTest
{
public:
    SnapshotTests() : StemhubTest("Stemhub snapshots") {}

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

        beginTest("Manifests with unsafe or conflicting entries are rejected");
        {
            const auto shaA = juce::String::repeatedString("a", 64);
            const auto shaB = juce::String::repeatedString("b", 64);

            namespace manifest = stemhub::manifest;

            manifest::Manifest parsed;
            auto result = manifest::fromJson(makeManifest("../../evil.flp", shaA, {}), parsed);
            expect(result.failed() && result.getErrorMessage().contains("unsafe"), "traversal in the project file is rejected");

            result = manifest::fromJson(
                makeManifest("song.flp", shaA, { { "Drums/kick.wav", shaA }, { "drums/KICK.wav", shaB } }), parsed);
            expect(result.failed() && result.getErrorMessage().contains("two different files"),
                   "two different files at the same path are rejected");

            result = manifest::fromJson(
                makeManifest("song.flp", shaA, { { "Drums/kick.wav", shaB }, { "Drums/kick.wav", shaB } }), parsed);
            expect(result.wasOk() && parsed.tracks.size() == 1, "an exact duplicate is kept once");

            result = manifest::fromJson(
                makeManifest("song.flp", "../" + shaA.substring(3), {}), parsed);
            expect(result.failed(), "a hash that is not hex is rejected");

            auto negativeSize = makeManifest("song.flp", shaA, {});
            negativeSize.getProperty("project_file", {}).getDynamicObject()->setProperty("size_bytes", -1);
            expect(manifest::fromJson(negativeSize, parsed).failed(), "a negative size is rejected");
        }

        beginTest("A manifest reads back what was written");
        {
            namespace manifest = stemhub::manifest;

            manifest::Manifest written;
            written.sourceDaw = "Ableton Live";
            written.projectFile = { juce::String::repeatedString("a", 64), 10, "song.als" };
            written.tracks = { { juce::String::repeatedString("b", 64), 5, "Samples/Imported/kick 01.wav" },
                               { juce::String::repeatedString("c", 64), 7, "loop.aif" } };

            const auto json = manifest::toJson(written);
            expect(json.getProperty("source_project_filename", {}).toString() == "song.als");
            expect(json.getProperty("tracks", {})[0].getProperty("name", {}).toString() == "kick 01", "tracks are named after their file");
            expect(manifest::totalSize(json) == 22, "the history shows the files' total size");

            manifest::Manifest read;
            expect(manifest::fromJson(json, read).wasOk());
            expect(read.sourceDaw == written.sourceDaw && read.projectFile.path == "song.als" && read.projectFile.sizeBytes == 10);
            expect(read.tracks.size() == 2 && read.tracks[0].path == "Samples/Imported/kick 01.wav"
                       && read.tracks[1].sha256 == written.tracks[1].sha256);
        }

        beginTest("Files are hashed in blocks, and a stopped job stops hashing");
        {
            TestEnvironment environment;
            const auto file = environment.root.getChildFile("stem.wav");
            juce::MemoryBlock content((3 << 20) + 17);
            auto* bytes = static_cast<juce::uint8*>(content.getData());
            for (size_t index = 0; index < content.getSize(); ++index)
                bytes[index] = static_cast<juce::uint8>(index * 31 + 7);
            expect(file.replaceWithData(content.getData(), content.getSize()));

            expect(stemhub::snapshotfiles::sha256OfFile(file) == sha256Of(content), "the same digest across block boundaries");
            expect(stemhub::snapshotfiles::sha256OfFile(environment.root.getChildFile("missing.wav")).isEmpty(), "a missing file has none");

            BlockingGate gate;
            BackgroundJobCoordinator<juce::String> jobs { 1, [] {} };
            jobs.enqueue([&gate, file](const auto&)
            {
                gate.block();
                return stemhub::snapshotfiles::sha256OfFile(file);
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

        beginTest("A save takes the project's audio, and leaves out backups and restored copies");
        {
            namespace snapshotfiles = stemhub::snapshotfiles;

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
                   && write(".hidden.wav", "hidden") && write("song-0123abcd/song.flp", "restored")
                   && write("song-0123abcd/Drums/kick.wav", "kick") && write("song-0123abcd (2)/song.flp", "restored 2"));

            const auto files = snapshotfiles::collect(projectFile);
            juce::StringArray paths;
            for (const auto& file : files)
                paths.add(file.getRelativePathFrom(folder).replaceCharacter('\\', '/'));

            expect(paths.joinIntoString(", ") == "song.flp, Drums/kick.wav, Loops/loop 01.aif, mix-deadbeef/vocal.wav",
                   "project file first, then its audio by path: " + paths.joinIntoString(", "));

            const auto summary = snapshotfiles::summarize(projectFile);
            expect(summary.fileCount == 4 && summary.totalBytes == 3 + 4 + 4 + 5, juce::String(summary.totalBytes));
            expect(snapshotfiles::collect(folder.getChildFile("missing.flp")).empty(), "no project file, nothing to save");

            expect(snapshotfiles::dawNameFor(projectFile) == "FL Studio");
            expect(snapshotfiles::dawNameFor(folder.getChildFile("song.als")) == "Ableton Live");
            expect(snapshotfiles::dawNameFor(folder.getChildFile("song.ptx")).isEmpty());
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
                expect(environment.root.getChildFile("stem" + juce::String(index) + ".wav").replaceWithText("stem"));

            // Like the plugin window's file count when the window closes.
            BlockingGate gate;
            BackgroundJobCoordinator<size_t> jobs { 1, [] {} };
            jobs.enqueue([&gate, projectFile](const auto&)
            {
                gate.block();
                return stemhub::snapshotfiles::collect(projectFile).size();
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

SnapshotTests snapshotTests;
}
