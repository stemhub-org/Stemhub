#include <JuceHeader.h>

#if ! JUCE_WINDOWS
 #include <sys/stat.h>
#endif

#include "application/CredentialStore.hpp"
#include "application/PluginState.hpp"
#include "application/RestoreHandoff.hpp"
#include "support/TestSupport.hpp"

namespace
{
using namespace stemhub::test;

class PersistenceTests final : public StemhubTest
{
public:
    PersistenceTests() : StemhubTest("Stemhub persistence") {}

    void runTest() override
    {
        beginTest("The link saved in the DAW project reads back, and bad data is ignored");
        {
            namespace pluginstate = stemhub::pluginstate;

            const ProjectLink link { "project-1", "branch-1", juce::File::getSpecialLocation(juce::File::tempDirectory)
                                                                  .getChildFile("Song & Co").getChildFile("song \"1\".flp") };
            const auto saved = pluginstate::encode(link);
            expect(pluginstate::decode(saved.getData(), saved.getSize()) == link, saved.toString());

            expect(!pluginstate::decode(nullptr, 0).isSet(), "no state is no link");
            const juce::String garbage = "not xml at all";
            expect(!pluginstate::decode(garbage.toRawUTF8(), garbage.getNumBytesAsUTF8()).isSet(), "unreadable state is no link");
            const juce::String otherPlugin = R"(<PluginState projectId="project-1"/>)";
            expect(!pluginstate::decode(otherPlugin.toRawUTF8(), otherPlugin.getNumBytesAsUTF8()).isSet(), "another tag is no link");

            const juce::String partial = R"(<StemhubState schema="1" projectId="project-1" workingFile="relative/song.flp"/>)";
            const auto partialLink = pluginstate::decode(partial.toRawUTF8(), partial.getNumBytesAsUTF8());
            expect(partialLink.projectId == "project-1" && partialLink.branchId.isEmpty(), "missing values stay empty");
            expect(partialLink.workingFile == juce::File(), "a path that isn't absolute here is dropped");
        }

        beginTest("A restore hand-off goes to one instance of its project, and only for a while");
        {
            namespace handoff = stemhub::handoff;

            TestEnvironment environment;
            const auto location = environment.root.getChildFile("pending-restore.json");
            const auto copy = environment.root.getChildFile("song-12345678").getChildFile("song.flp");
            expect(copy.create().wasOk() && copy.replaceWithText("flp"));

            const auto now = juce::Time::getCurrentTime();
            const RestoreHandoff written { "project-1", "branch-1", WorkingCopyBaseline::recordedNow(copy, "version-1"), now };
            handoff::write(location, written);

            expect(!handoff::take(location, "project-2", now).has_value() && location.existsAsFile(),
                   "another project's instance leaves it");
            const auto taken = handoff::take(location, "project-1", now + juce::RelativeTime::minutes(9));
            expect(taken.has_value() && taken->branchId == "branch-1" && taken->copy.versionId == "version-1"
                       && taken->copy.describes(copy) && taken->copy.isUnchanged(),
                   "its instance gets the copy and what it holds");
            expect(!location.exists() && !handoff::take(location, "project-1", now).has_value(), "it is taken once");

            handoff::write(location, written);
            expect(handoff::take(location, {}, now).has_value(), "an instance without a link takes any project's");

            handoff::write(location, written);
            expect(!handoff::take(location, "project-1", now + juce::RelativeTime::minutes(11)).has_value()
                       && !location.exists(),
                   "one nobody took in time is dropped");

            handoff::write(location, written);
            expect(copy.deleteFile());
            expect(!handoff::take(location, "project-1", now).has_value() && !location.exists(),
                   "one whose copy is gone is dropped");
        }

        beginTest("The saved token is shared by instances and readable only by its user");
        {
            TestEnvironment environment;
            const auto file = environment.root.getChildFile("Stemhub").getChildFile("credentials.json");
            FileCredentialStore store(file);
            FileCredentialStore otherInstance(file);

            expect(store.loadToken().isEmpty(), "nothing saved yet");
            store.saveToken("secret-token");
            expect(otherInstance.loadToken() == "secret-token", "every instance reads the same token");

           #if ! JUCE_WINDOWS
            struct stat fileInfo {};
            struct stat folderInfo {};
            expect(::stat(file.getFullPathName().toRawUTF8(), &fileInfo) == 0
                       && (fileInfo.st_mode & 0777) == 0600,
                   "the file is private: " + juce::String::formatted("%o", static_cast<unsigned int>(fileInfo.st_mode & 0777)));
            expect(::stat(file.getParentDirectory().getFullPathName().toRawUTF8(), &folderInfo) == 0
                       && (folderInfo.st_mode & 0777) == 0700,
                   "its folder is private: " + juce::String::formatted("%o", static_cast<unsigned int>(folderInfo.st_mode & 0777)));
           #endif

            otherInstance.clear();
            expect(store.loadToken().isEmpty() && !file.exists(), "signing out forgets it everywhere");
        }
    }
};

PersistenceTests persistenceTests;
}
