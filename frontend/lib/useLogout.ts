"use client";

import { useQueryClient } from "@tanstack/react-query";
import { useRouter } from "next/navigation";
import { apiFetch } from "@/lib/api";
import { clearAccessToken } from "@/lib/authToken";

/** Ends the session on the server, forgets it here and goes to the login page. */
export function useLogout(): () => Promise<void> {
    const router = useRouter();
    const queryClient = useQueryClient();
    return async () => {
        // Sign out locally even when the server can't be reached.
        await apiFetch("/auth/logout", { method: "POST", redirectOnUnauthorized: false }).catch(() => undefined);
        clearAccessToken();
        // Don't show this user's data to whoever signs in next.
        queryClient.clear();
        router.push("/login");
    };
}
