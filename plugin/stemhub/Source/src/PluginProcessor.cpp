#include "../include/PluginProcessor.hpp"
#include "../include/PluginEditor.hpp"

StemhubAudioProcessor::StemhubAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor(BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

StemhubAudioProcessor::~StemhubAudioProcessor()
{
    cancelPendingUpdate();
    backgroundJobs.removeAllJobs(true, 2000);
}

void StemhubAudioProcessor::signIn(User newUser) noexcept
{
    currentUser = std::move(newUser);
    sessionState.authState = AuthState::signedIn;
    sessionState.uiState = UIState::dashboard;
    sessionState.operationState = OperationState::idle;
}

void StemhubAudioProcessor::signOut() noexcept
{
    ++authRequestGeneration;
    currentUser.reset();
    access_tkn.clear();
    authErrorMessage.clear();
    projectStatusMessage.clear();
    projects.clear();
    sessionState = {};
    sendChangeMessage();
}

void StemhubAudioProcessor::setAuthState(AuthState newAuthState) noexcept
{
    sessionState.authState = newAuthState;

    if (newAuthState != AuthState::signedIn)
    {
        sessionState.uiState = UIState::login;
        sessionState.operationState = OperationState::idle;
    }
}

void StemhubAudioProcessor::setUIState(UIState newUIState) noexcept
{
    sessionState.uiState = sessionState.authState == AuthState::signedIn ? newUIState : UIState::login;
}

void StemhubAudioProcessor::setOperationState(OperationState newOperationState) noexcept
{
    sessionState.operationState = sessionState.authState == AuthState::signedIn ? newOperationState
                                                                                : OperationState::idle;
}


void StemhubAudioProcessor::requestSignIn(const juce::String& email, const juce::String& password)
{
    if (sessionState.authState == AuthState::signingIn)
        return;

    const auto requestId = ++authRequestGeneration;
    setAuthState(AuthState::signingIn);
    authErrorMessage.clear();
    projectStatusMessage.clear();
    projects.clear();
    sendChangeMessage();

    backgroundJobs.addJob([this, email, password, requestId]
    {
        AuthRequestResult result;
        result.requestId = requestId;

        auto loginResult = apiClient.login(email, password);
        if (!loginResult.ok())
        {
            result.authErrorMessage = loginResult.error ? loginResult.error->message
                                                        : "Failed to sign in.";
        }
        else
        {
            const auto token = loginResult.value->accessToken;
            auto userResult = apiClient.fetchCurrentUser(token);

            if (!userResult.ok() || !userResult.value->isValid())
            {
                result.authErrorMessage = userResult.error ? userResult.error->message
                                                           : "Failed to load your user profile.";
            }
            else
            {
                result.token = token;
                result.user = std::move(userResult.value);

                auto projectsResult = apiClient.fetchProjects(token);
                if (projectsResult.ok())
                {
                    result.projects = std::move(*projectsResult.value);
                    result.projectStatusMessage = result.projects.empty() ? "No projects found."
                                                                          : "Loaded " + juce::String(static_cast<int>(result.projects.size())) + " project(s).";
                }
                else
                {
                    result.projectStatusMessage = projectsResult.error ? projectsResult.error->message
                                                                       : "Failed to load projects.";
                }
            }
        }

        {
            const std::lock_guard<std::mutex> lock(authResultMutex);
            pendingAuthResult = std::move(result);
        }

        triggerAsyncUpdate();
    });
}

void StemhubAudioProcessor::handleAsyncUpdate()
{
    std::optional<AuthRequestResult> result;

    {
        const std::lock_guard<std::mutex> lock(authResultMutex);
        result = std::move(pendingAuthResult);
        pendingAuthResult.reset();
    }

    if (!result.has_value())
        return;

    if (result->requestId != authRequestGeneration.load())
        return;

    if (result->authErrorMessage.isNotEmpty())
    {
        authErrorMessage = result->authErrorMessage;
        setAuthState(AuthState::authError);
        sendChangeMessage();
        return;
    }

    authErrorMessage.clear();
    projectStatusMessage = result->projectStatusMessage;
    projects = std::move(result->projects);
    access_tkn = std::move(result->token);
    signIn(std::move(*result->user));
    sendChangeMessage();
}

const juce::String StemhubAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool StemhubAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool StemhubAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool StemhubAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double StemhubAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int StemhubAudioProcessor::getNumPrograms()
{
    return 1;
}

int StemhubAudioProcessor::getCurrentProgram()
{
    return 0;
}

void StemhubAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String StemhubAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void StemhubAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void StemhubAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void StemhubAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool StemhubAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

template <typename SampleType>
static void clearExtraOutputChannels(juce::AudioProcessor& processor, juce::AudioBuffer<SampleType>& buffer)
{
    const auto totalNumInputChannels = processor.getTotalNumInputChannels();
    const auto totalNumOutputChannels = processor.getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
}

void StemhubAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);
    clearExtraOutputChannels(*this, buffer);
}

void StemhubAudioProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);
    clearExtraOutputChannels(*this, buffer);
}

bool StemhubAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* StemhubAudioProcessor::createEditor()
{
    return new StemhubAudioProcessorEditor(*this);
}

void StemhubAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ignoreUnused(destData);
}

void StemhubAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StemhubAudioProcessor();
}
