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
