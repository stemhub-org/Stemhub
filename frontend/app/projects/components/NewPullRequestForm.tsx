"use client";

import { useState } from "react";
import type { FormEvent } from "react";
import { Button } from "@/components/ui/Button";
import { Input } from "@/components/ui/Input";
import { Textarea } from "@/components/ui/Textarea";
import { errorMessage } from "@/lib/api";
import { useCreatePullRequest } from "@/lib/queries";
import type { Branch } from "@/types/project";
import type { PullRequest } from "@/types/pullRequest";

interface NewPullRequestFormProps {
    projectId: string;
    /** The project's live branches. */
    branches: Branch[];
    onCreated: (pullRequest: PullRequest) => void;
}

const selectClassName =
    "w-full rounded-lg border border-border-subtle bg-background px-3 py-2.5 text-sm focus:border-accent focus:outline-none";

/** Proposes bringing one branch into another; by default into main. */
export function NewPullRequestForm({ projectId, branches, onCreated }: NewPullRequestFormProps) {
    const defaultTarget = branches.find((branch) => branch.name === "main") ?? branches[0];
    const [targetId, setTargetId] = useState(defaultTarget?.id ?? "");
    const [sourceId, setSourceId] = useState(branches.find((branch) => branch.id !== targetId)?.id ?? "");
    const [title, setTitle] = useState("");
    const [description, setDescription] = useState("");
    const createPullRequest = useCreatePullRequest(projectId);

    if (branches.length < 2) {
        return <p className="text-sm text-foreground-muted">Create another branch to open a pull request.</p>;
    }

    const canSubmit = sourceId !== "" && targetId !== "" && sourceId !== targetId && title.trim() !== "";

    const handleSubmit = (event: FormEvent) => {
        event.preventDefault();
        if (!canSubmit) return;
        createPullRequest.mutate(
            {
                source_branch_id: sourceId,
                target_branch_id: targetId,
                title: title.trim(),
                ...(description.trim() ? { description: description.trim() } : {}),
            },
            { onSuccess: (created) => onCreated(created) },
        );
    };

    return (
        <form onSubmit={handleSubmit} className="flex flex-col gap-3">
            <div className="flex flex-wrap gap-3">
                <label className="flex min-w-40 flex-1 flex-col gap-1 text-sm">
                    From
                    <select className={selectClassName} value={sourceId} onChange={(e) => setSourceId(e.target.value)}>
                        {branches.map((branch) => (
                            <option key={branch.id} value={branch.id}>
                                {branch.name}
                            </option>
                        ))}
                    </select>
                </label>
                <label className="flex min-w-40 flex-1 flex-col gap-1 text-sm">
                    Into
                    <select className={selectClassName} value={targetId} onChange={(e) => setTargetId(e.target.value)}>
                        {branches.map((branch) => (
                            <option key={branch.id} value={branch.id}>
                                {branch.name}
                            </option>
                        ))}
                    </select>
                </label>
            </div>
            {sourceId === targetId && (
                <p className="text-sm text-foreground-muted">Pick two different branches.</p>
            )}
            <label className="flex flex-col gap-1 text-sm">
                Title
                <Input value={title} maxLength={255} onChange={(e) => setTitle(e.target.value)} />
            </label>
            <label className="flex flex-col gap-1 text-sm">
                Description (optional)
                <Textarea value={description} rows={3} onChange={(e) => setDescription(e.target.value)} />
            </label>
            {createPullRequest.isError && (
                <p role="alert" className="text-sm text-red-500">
                    {errorMessage(createPullRequest.error, "Couldn't open the pull request")}
                </p>
            )}
            <div>
                <Button type="submit" disabled={!canSubmit} loading={createPullRequest.isPending}>
                    Open pull request
                </Button>
            </div>
        </form>
    );
}
