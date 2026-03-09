// ── Lightweight owner/user reference ──
export interface OwnerSummary {
    id: string;
    username: string;
}

// ── Branch ──
export interface Branch {
    id: string;
    name: string;
    project_id: string;
    created_at: string;
    is_deleted: boolean;
    deleted_at: string | null;
}

// ── Version with author (from summary endpoint) ──
export interface VersionWithAuthor {
    id: string;
    commit_message: string | null;
    created_at: string;
    branch_name: string;
    author: OwnerSummary | null;
    has_artifact: boolean;
}

// ── Track ──
export interface Track {
    id: string;
    version_id: string;
    name: string;
    file_type: string;
    storage_path: string | null;
    created_at: string | null;
}

// ── Project detail ──
export interface ProjectDetail {
    id: string;
    name: string;
    description: string | null;
    category: string;
    is_public: boolean;
    created_at: string;
    owner: OwnerSummary;
}

// ── Full summary response ──
export interface ProjectSummaryResponse {
    project: ProjectDetail;
    branches: Branch[];
    recent_versions: VersionWithAuthor[];
    latest_version_id: string | null;
    has_preview: boolean;
}

// ── Collaborators ──
export interface Collaborator {
    project_id: string;
    user_id: string;
    role: string;
    created_at: string;
    user: {
        id: string;
        email: string;
        username: string;
        avatar_url: string | null;
    } | null;
}

// ── Activity stats ──
export interface DailyActivity {
    date: string;
    count: number;
}

export interface ActivityStatsResponse {
    daily_activity: DailyActivity[];
    total_commits: number;
    total_contributors: number;
}

// ── Top contributors ──
export interface ContributorStats {
    user_id: string;
    username: string;
    initials: string;
    commits: number;
}

export interface TopContributorsResponse {
    contributors: ContributorStats[];
}
