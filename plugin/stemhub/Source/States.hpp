#pragma once

#include <string>
#include <vector>

class States {
    public:
        enum class AuthState {Disconnected,Connecting,Connected,Disconnecting};
        enum class SyncState {NotSynced,Syncing,Synced};
        enum class ViewState {Default,Compact,Expanded};

        static const AuthState getAuthState() { return authState; };
        void setAuthState(AuthState state) { authState = state; };
        
        static const SyncState getSyncState() { return syncState; };
        void setSyncState(SyncState state) { syncState = state; };

        static const ViewState getViewState() { return viewState; };
        void setViewState(ViewState state) { viewState = state; };

    private:
        static AuthState authState;
        static SyncState syncState;
        static ViewState viewState;
};
