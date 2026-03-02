#pragma once

enum class AuthState
{
    signedOut,
    signingIn,
    signedIn,
    authError
};

enum class SyncState
{
    idle,
    syncing,
    syncSuccess,
    syncError
};
