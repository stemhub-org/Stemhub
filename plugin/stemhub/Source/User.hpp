#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "States.hpp"

class User {
    public:
        User(std::string name, std::string password)
            : name(std::move(name)), password(hashPassword(password)) {}

        std::string getName() const { return name; }
        std::string getHashedPassword() const { return password; }
        States getUserState() const { return userState; }

        void setName(std::string newName) { name = newName; }
        void setPassword(std::string newPassword) { password = hashPassword(newPassword); }

        void setAuthState(States::AuthState state) { userState.setAuthState(state); }
        void setSyncState(States::SyncState state) { userState.setSyncState(state); }
        void setViewState(States::ViewState state) { userState.setViewState(state); }
    private:
        static std::string hashPassword(const std::string& plainTextPassword) {
            return std::to_string(std::hash<std::string>{}(plainTextPassword));
        }
        std::string name;
        std::string password;
        States userState;
};