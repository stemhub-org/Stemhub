#pragma once

#include <vector>

#include <JuceHeader.h>

#include "domain/Status.hpp"

// What the views show, worked out from the session's state by SessionPresenter. Plain values
// without GUI code, so the tests can check them without a window.

// What the session is doing, as far as the views care. While it works, the controls that would
// start more work are disabled, and the one that started it shows it is busy.
enum class SessionActivity
{
    idle,
    loading, // signing in, projects or history
    saving,
    restoring
};

enum class Screen
{
    login,
    projects,
    dashboard
};

struct LoginModel
{
    Status status;
    // A sign-in is running; it can be cancelled.
    bool isSigningIn { false };
};

// Everything a project tile shows.
struct ProjectListItem
{
    juce::String id;
    juce::String name;
    juce::String description;
    juce::String category;
    bool isPublic { false };

    bool operator==(const ProjectListItem& other) const
    {
        return id == other.id && name == other.name && description == other.description
            && category == other.category && isPublic == other.isPublic;
    }

    bool operator!=(const ProjectListItem& other) const { return !(*this == other); }
};

struct ProjectGridModel
{
    std::vector<ProjectListItem> projects;
    // The project open here, highlighted on the grid.
    juce::String selectedProjectId;
    // The grid's own message, or a hint about what to do next.
    Status status;
    juce::String accountName;
    // The project file a new project would be created from; empty when there is none.
    juce::String newProjectFilePath;
    SessionActivity activity { SessionActivity::idle };
};

struct BranchListItem
{
    juce::String id;
    juce::String name;

    bool operator==(const BranchListItem& other) const { return id == other.id && name == other.name; }
    bool operator!=(const BranchListItem& other) const { return !(*this == other); }
};

// One saved version as the history list and detail card show it.
struct VersionListItem
{
    juce::String id;
    juce::String message;
    // Saved without a message of its own.
    bool isUntitled { false };
    juce::Time createdAt;
    juce::String sourceDaw;
    juce::String sourceFilename;
    juce::int64 sizeBytes { 0 };
    bool isOpenInDaw { false };

    bool operator==(const VersionListItem& other) const
    {
        return id == other.id && message == other.message && isUntitled == other.isUntitled
            && createdAt == other.createdAt && sourceDaw == other.sourceDaw
            && sourceFilename == other.sourceFilename && sizeBytes == other.sizeBytes
            && isOpenInDaw == other.isOpenInDaw;
    }

    bool operator!=(const VersionListItem& other) const { return !(*this == other); }
};

struct DashboardModel
{
    juce::String projectName;
    std::vector<BranchListItem> branches;
    juce::String selectedBranchId;
    // Newest first.
    std::vector<VersionListItem> versions;
    juce::String selectedVersionId;
    Status status;
    SessionActivity activity { SessionActivity::idle };
    // The working copy, when it is there; empty otherwise.
    juce::String workingFilePath;
};

struct SessionModel
{
    Screen screen { Screen::login };
    // Only the model of the screen shown is filled in.
    LoginModel login;
    ProjectGridModel grid;
    DashboardModel dashboard;
    // A save has just created a version: its message is spent, and what the next save takes
    // changed.
    bool messageWasSaved { false };
};
