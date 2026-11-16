// Mirrors the backend's rules (backend/src/stemhub/schemas.py) so forms can
// explain a problem before calling the server. The backend stays the judge.

export const USERNAME_MIN_LENGTH = 3;
export const USERNAME_MAX_LENGTH = 32;
export const PASSWORD_MIN_LENGTH = 8;
// bcrypt's limit: the backend refuses longer passwords.
export const PASSWORD_MAX_BYTES = 72;

export function usernameError(username: string): string | null {
    const length = username.trim().replace(/\s+/g, " ").length;
    if (length < USERNAME_MIN_LENGTH || length > USERNAME_MAX_LENGTH) {
        return `Username must be between ${USERNAME_MIN_LENGTH} and ${USERNAME_MAX_LENGTH} characters`;
    }
    return null;
}

export function newPasswordError(password: string): string | null {
    if (password.length < PASSWORD_MIN_LENGTH) {
        return `Password must be at least ${PASSWORD_MIN_LENGTH} characters`;
    }
    if (new TextEncoder().encode(password).length > PASSWORD_MAX_BYTES) {
        return `Password must be at most ${PASSWORD_MAX_BYTES} bytes`;
    }
    return null;
}
