#pragma once

enum class AuthState
{
    signedOut,
    signedIn,
    authError
};

enum class UIState
{
    login,
    dashboard,
    commit,
    history,
    settings
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
