"use client";

import { useMutation, useQueryClient } from "@tanstack/react-query";
import type {
    AccountDeletion,
    AccountDeletionRequest,
    EmailChangeRequest,
    PasswordChangeRequest,
    ProfileUpdate,
    UserProfile,
} from "@/types/user";
import { ApiError, apiFetch } from "./api";
import { queryKeys } from "./queries";

/** Saves profile fields (PUT /auth/me) and replaces the cached signed-in user. */
export function useUpdateProfile() {
    const queryClient = useQueryClient();
    return useMutation<UserProfile, ApiError, ProfileUpdate>({
        mutationFn: (changes) =>
            apiFetch<UserProfile>("/auth/me", { method: "PUT", body: JSON.stringify(changes) }),
        onSuccess: (user) => {
            // Merged so fields only /auth/me returns (terms_accepted) are kept.
            queryClient.setQueryData<UserProfile>(queryKeys.currentUser, (cached) => ({ ...cached, ...user }));
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
            // Merged so fields only /auth/me returns (terms_accepted) are kept.
            queryClient.setQueryData<UserProfile>(queryKeys.currentUser, (cached) => ({ ...cached, ...user }));
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

/** Accepts the current Terms of Service and Privacy Policy (POST /auth/me/terms). */
export function useAcceptTerms() {
    const queryClient = useQueryClient();
    return useMutation<UserProfile, ApiError, void>({
        mutationFn: () =>
            apiFetch<UserProfile>("/auth/me/terms", { method: "POST", body: JSON.stringify({ accept_terms: true }) }),
        onSuccess: (user) => {
            queryClient.setQueryData(queryKeys.currentUser, user);
        },
    });
}

/** Asks for the account to be deleted (POST /auth/me/delete); it can be restored by signing in within 30 days. */
export function useDeleteAccount() {
    return useMutation<AccountDeletion, ApiError, AccountDeletionRequest>({
        mutationFn: (request) =>
            apiFetch<AccountDeletion>("/auth/me/delete", { method: "POST", body: JSON.stringify(request) }),
    });
}
