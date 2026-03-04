#pragma once

#include <atomic>
#include <JuceHeader.h>
#include <mutex>
#include <optional>
#include <vector>
#include "User.hpp"
#include "Project.hpp"
#include "States.hpp"
#include "ApiUtils.hpp"
#include "ApiClient.hpp"
#include "VersionControlService.hpp"

class StemhubAudioProcessor : public juce::AudioProcessor,
                              public juce::ChangeBroadcaster,
                              private juce::AsyncUpdater
{
public:
    StemhubAudioProcessor();
    ~StemhubAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    using AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // CUSTOM METHODS
    [[nodiscard]] const SessionState& getSessionState() const noexcept { return sessionState; }
    [[nodiscard]] AuthState getAuthState() const noexcept { return sessionState.authState; }
    [[nodiscard]] UIState getUIState() const noexcept { return sessionState.uiState; }
    [[nodiscard]] OperationState getOperationState() const noexcept { return sessionState.operationState; }
    [[nodiscard]] const std::optional<User>& getCurrentUser() const noexcept { return currentUser; }
    [[nodiscard]] const std::vector<Project>& getProjects() const noexcept { return projects; }
    [[nodiscard]] const juce::String& getAuthErrorMessage() const noexcept { return authErrorMessage; }
    [[nodiscard]] const juce::String& getProjectStatusMessage() const noexcept { return projectStatusMessage; }
    
    void setCurrentUser(std::optional<User> newUser) noexcept { currentUser = std::move(newUser); }
    void signIn(User newUser) noexcept;
    void signOut() noexcept;
    
    [[nodiscard]] juce::String getUsername() const noexcept { return currentUser ? currentUser->username : juce::String();}
    
    void setAuthState(AuthState newAuthState) noexcept;
    void setUIState(UIState newUIState) noexcept;
    void setOperationState(OperationState newOperationState) noexcept;
    
    void requestSignIn(const juce::String& email, const juce::String& password);
    VersionControlService& getVersionControlService() noexcept { return versionControlService; }

private:
    void handleAsyncUpdate() override;

    struct AuthRequestResult
    {
        uint64_t requestId {};
        std::optional<User> user;
        std::vector<Project> projects;
        juce::String token;
        juce::String authErrorMessage;
        juce::String projectStatusMessage;
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemhubAudioProcessor)

    ApiClient apiClient;
    juce::String access_tkn;
    juce::String authErrorMessage;
    juce::String projectStatusMessage;
    std::optional<User> currentUser;
    std::vector<Project> projects;
    SessionState sessionState;
    std::mutex authResultMutex;
    std::optional<AuthRequestResult> pendingAuthResult;
    std::atomic<uint64_t> authRequestGeneration { 0 };
    juce::ThreadPool backgroundJobs { 1 };
    VersionControlService versionControlService { apiClient };
};
