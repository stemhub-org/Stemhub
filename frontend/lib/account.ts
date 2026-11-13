"use client";

import { useMutation, useQueryClient } from "@tanstack/react-query";
import type { EmailChangeRequest, PasswordChangeRequest, ProfileUpdate, UserProfile } from "@/types/user";
import { ApiError, apiFetch } from "./api";
import { queryKeys } from "./queries";

/** Saves profile fields (PUT /auth/me) and replaces the cached signed-in user. */
export function useUpdateProfile() {
    const queryClient = useQueryClient();
    return useMutation<UserProfile, ApiError, ProfileUpdate>({
        mutationFn: (changes) =>
            apiFetch<UserProfile>("/auth/me", { method: "PUT", body: JSON.stringify(changes) }),
        onSuccess: (user) => {
            queryClient.setQueryData(queryKeys.currentUser, user);
        },
    });
}

/** Changes the sign-in email (PUT /auth/me/email) and replaces the cached signed-in user. */
export function useChangeEmail() {
    const queryClient = useQueryClient();
    return useMutation<UserProfile, ApiError, EmailChangeRequest>({
        mutationFn: (request) =>
            apiFetch<UserProfile>("/auth/me/email", { method: "PUT", body: JSON.stringify(request) }),
        onSuccess: (user) => {
            queryClient.setQueryData(queryKeys.currentUser, user);
        },
    });
}

/** Changes the password (PUT /auth/me/password). The signed-in user is unchanged. */
export function useChangePassword() {
    return useMutation<void, ApiError, PasswordChangeRequest>({
        mutationFn: async (request) => {
            await apiFetch<unknown>("/auth/me/password", { method: "PUT", body: JSON.stringify(request) });
        },
    });
}
