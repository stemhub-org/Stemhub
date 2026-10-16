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
