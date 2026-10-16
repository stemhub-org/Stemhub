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
    message: string | null;
    created_at: string;
    branch_name: string;
    author: OwnerSummary | null;
    source_daw: string | null;
    source_project_filename: string | null;
}

export interface MixerDiffSummary {
    total_changes: number;
    inserts_changed: number;
    slots_changed: number;
    parameter_changes: number;
}

// `insert_index` uses FL Studio's mixer numbering (Master = 0) and is null for
// project-level changes such as `project_file_changed`. Known `type` values
// include `project_file_changed`, `insert_*` and `slot_*` changes (for example
// `slot_dry_wet_changed`); the UI renders `message` and never branches on it.
export interface MixerDiffChange {
    type: string;
    insert_index: number | null;
    insert_name: string | null;
    slot_index: number | null;
    before: unknown;
    after: unknown;
    message: string;
}

export interface VersionDiffHistoryEntry {
    version: VersionWithAuthor;
    compared_to_version_id: string | null;
    status: "initial" | "compared" | "unsupported";
    status_message: string | null;
    summary: MixerDiffSummary | null;
    changes: MixerDiffChange[];
}

// ── Asset summary (from GET /versions/{id}/assets) ──
// One audio or MIDI file of a version, besides the project file. Sourced from
// the version's manifest (spec §7). `id` is "{sha256}:{index}" and `path` is
// relative to the project folder. `name` is a display name, not the basename:
// the file name without its extension for manifest v2, the stored name for v1.
// Use `path` for the file name and folder. `file_type` is derived from the
// path and is display-only.
export interface AssetSummary {
    id: string;
    path: string;
    name: string;
    file_type: string | null;
    size_bytes: number | null;
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
    total_versions: number;
    total_contributors: number;
}

// ── Top contributors ──
export interface ContributorStats {
    user_id: string;
    username: string;
    initials: string;
    versions: number;
}

export interface TopContributorsResponse {
    contributors: ContributorStats[];
}
