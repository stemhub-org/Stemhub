#include "application/SnapshotBundler.hpp"
#include "application/SnapshotFiles.hpp"
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
                expect(SnapshotBundler::isSafeManifestPath(path), juce::String("should accept ") + path);

            for (const auto* path : { "", "/etc/passwd", "../evil.wav", "Drums/../../evil.wav", "./song.flp",
                                      "a//b.wav", "a/b/", "C:\\evil.wav", "C:evil.wav", "..\\evil.wav",
                                      "Drums\\kick.wav", "trailing.", "trailing ", "NUL.wav", "Samples/con",
                                      "bad\nname.wav" })
                expect(!SnapshotBundler::isSafeManifestPath(path), juce::String("should reject ") + juce::String(path).quoted());

            expect(SnapshotBundler::isSafeManifestPath(juce::String::repeatedString("a", 255)), "255 characters fit the backend limit");
            expect(!SnapshotBundler::isSafeManifestPath(juce::String::repeatedString("a", 256)), "256 characters exceed the backend limit");
        }

        beginTest("Manifests with unsafe or conflicting entries are rejected");
        {
            const auto shaA = juce::String::repeatedString("a", 64);
            const auto shaB = juce::String::repeatedString("b", 64);

            ParsedManifest parsed;
            auto result = SnapshotBundler::parseManifest(makeManifest("../../evil.flp", shaA, {}), parsed);
            expect(result.failed() && result.getErrorMessage().contains("unsafe"), "traversal in the project file is rejected");

            result = SnapshotBundler::parseManifest(
                makeManifest("song.flp", shaA, { { "Drums/kick.wav", shaA }, { "drums/KICK.wav", shaB } }), parsed);
            expect(result.failed() && result.getErrorMessage().contains("two different files"),
                   "two different files at the same path are rejected");

            result = SnapshotBundler::parseManifest(
                makeManifest("song.flp", shaA, { { "Drums/kick.wav", shaB }, { "Drums/kick.wav", shaB } }), parsed);
            expect(result.wasOk() && parsed.entries.size() == 2, "an exact duplicate is kept once");

            result = SnapshotBundler::parseManifest(
                makeManifest("song.flp", "../" + shaA.substring(3), {}), parsed);
            expect(result.failed(), "a hash that is not hex is rejected");
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
    }
};

SnapshotTests snapshotTests;
}
