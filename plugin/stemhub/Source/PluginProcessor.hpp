#pragma once

#include <JuceHeader.h>
#include <optional>
#include "User.hpp"
#include "States.hpp"

class StemhubAudioProcessor : public juce::AudioProcessor
{
    public:
        StemhubAudioProcessor();
        ~StemhubAudioProcessor() override = default;

        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override;

    #ifndef JucePlugin_PreferredChannelConfigurations
        bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    #endif

        using AudioProcessor::processBlock;
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
        void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override;

        const juce::String getName() const override;

        bool acceptsMidi() const override;
        bool producesMidi() const override;
        bool isMidiEffect() const override;
        double getTailLengthSeconds() const override;

        int getNumPrograms() override;
        int getCurrentProgram() override;
        void setCurrentProgram (int index) override;
        const juce::String getProgramName (int index) override;
        void changeProgramName (int index, const juce::String& newName) override;

        void getStateInformation (juce::MemoryBlock& destData) override;
        void setStateInformation (const void* data, int sizeInBytes) override;

        [[nodiscard]] AuthState getAuthState() const noexcept { return authState; }
        [[nodiscard]] SyncState getSyncState() const noexcept { return syncState; }
        [[nodiscard]] const std::optional<User>& getCurrentUser() const noexcept { return currentUser; }

        void setAuthState (AuthState newState) noexcept { authState = newState; }
        void setSyncState (SyncState newState) noexcept { syncState = newState; }
        void setCurrentUser (std::optional<User> newUser) noexcept { currentUser = std::move (newUser); }
        void clearSession() noexcept {
            currentUser.reset();
            authState = AuthState::signedOut;
            syncState = SyncState::idle;
        }

        [[nodiscard]] juce::String getUsername() const noexcept {
            return currentUser ? currentUser->username : juce::String();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemhubAudioProcessor)

        std::optional<User> currentUser;
        AuthState authState { AuthState::signedOut };
        SyncState syncState { SyncState::idle };
};
