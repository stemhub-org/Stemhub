// ── Signed-in user (GET /auth/me, backend `UserResponse`) ──
export interface UserProfile {
    id: string;
    email: string;
    username: string;
    avatar_url: string | null;
    bio: string | null;
    location: string | null;
    website: string | null;
    genres: string[] | null;
    created_at: string;
    is_active: boolean;
    is_admin: boolean;
    /** From GET /auth/me: false until the current Terms of Service and Privacy Policy are accepted. */
    terms_accepted?: boolean;
    /** From GET /auth/me: external accounts the user signs in with, e.g. ["google"]. */
    linked_providers?: string[];
}

// ── Profile edits (PUT /auth/me, backend `UserUpdate`). Omitted fields are left unchanged. ──
export interface ProfileUpdate {
    username?: string;
    avatar_url?: string | null;
    bio?: string | null;
    location?: string | null;
    website?: string | null;
    genres?: string[] | null;
}

// ── PUT /auth/me/email (backend `EmailChangeRequest`) ──
export interface EmailChangeRequest {
    new_email: string;
}

// ── PUT /auth/me/password (backend `PasswordChangeRequest`) ──
export interface PasswordChangeRequest {
    current_password: string;
    new_password: string;
}

// ── Account deletion (POST /auth/me/delete) ──
export interface AccountDeletionRequest {
    confirm_username: string;
    /** Not needed for accounts that sign in with Google only. */
    password?: string;
}

export interface AccountDeletion {
    /** The data is erased then, unless the user signs in again before. */
    erased_at: string;
}

