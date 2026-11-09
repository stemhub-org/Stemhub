"use client";

import { useMutation, useQueryClient } from "@tanstack/react-query";
import type { ProfileUpdate, UserProfile } from "@/types/user";
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
