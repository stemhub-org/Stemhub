// Where the browser keeps the access token. A remembered sign-in uses
// localStorage, which survives closing the browser, like its 30-day cookie;
// otherwise sessionStorage, which the browser clears with its session cookie.

const KEY = "access_token";

export function getAccessToken(): string | null {
    if (typeof window === "undefined") return null;
    return sessionStorage.getItem(KEY) ?? localStorage.getItem(KEY);
}

export function saveAccessToken(token: string, { remember }: { remember: boolean }): void {
    clearAccessToken();
    (remember ? localStorage : sessionStorage).setItem(KEY, token);
}

export function clearAccessToken(): void {
    sessionStorage.removeItem(KEY);
    localStorage.removeItem(KEY);
}
