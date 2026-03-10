#pragma once

enum class AuthState
{
    signedOut,
    signingIn,
    signedIn,
    authError
};

enum class UIState
{
    login,
    projectSelection,
    dashboard
};

enum class OperationState
{
    idle,
    loadingProjects,
    committing,
    pulling,
    error
};

struct SessionState
{
    AuthState authState { AuthState::signedOut };
    UIState uiState { UIState::login };
    OperationState operationState { OperationState::idle };
};
