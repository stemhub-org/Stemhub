#pragma once

#include <JuceHeader.h>

// Where the signed-in user's token is kept between plugin sessions. Every plugin instance on the
// machine shares it; each keeps its own session once signed in.
class CredentialStore
{
public:
    virtual ~CredentialStore() = default;

    [[nodiscard]] virtual juce::String loadToken() const = 0;
    virtual void saveToken(const juce::String& token) = 0;
    virtual void clear() = 0;
};

// A JSON file only the current user can read: on macOS and Linux its folder is made 0700 and
// the file 0600. Windows already keeps app data per user.
class FileCredentialStore final : public CredentialStore
{
public:
    explicit FileCredentialStore(juce::File fileToUse);

    [[nodiscard]] juce::String loadToken() const override;
    void saveToken(const juce::String& token) override;
    void clear() override;

private:
    juce::File file;
};

// Keeps the token for the lifetime of the object only.
class InMemoryCredentialStore final : public CredentialStore
{
public:
    [[nodiscard]] juce::String loadToken() const override { return token; }
    void saveToken(const juce::String& newToken) override { token = newToken; }
    void clear() override { token.clear(); }

private:
    juce::String token;
};
