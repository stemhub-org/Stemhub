"use client";

import { useState } from "react";
import { ArrowRight, ChevronDown } from "lucide-react";
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
    onCancel?: () => void;
}

const labelClassName = "text-sm font-medium text-foreground";

// Same look as the Input component; the native arrow is replaced so it sits inside the padding.
const selectClassName =
    "w-full appearance-none rounded-lg border border-border-subtle bg-background py-2.5 pl-3 pr-9 text-sm text-foreground focus:border-accent focus:outline-none focus:ring-1 focus:ring-accent/50 transition-colors";

interface BranchSelectProps {
    label: string;
    value: string;
    branches: Branch[];
    onChange: (branchId: string) => void;
}

function BranchSelect({ label, value, branches, onChange }: BranchSelectProps) {
    return (
        <label className="flex min-w-0 flex-1 flex-col gap-1.5">
            <span className={labelClassName}>{label}</span>
            <span className="relative">
                <select className={selectClassName} value={value} onChange={(e) => onChange(e.target.value)}>
                    {branches.map((branch) => (
                        <option key={branch.id} value={branch.id}>
                            {branch.name}
                        </option>
                    ))}
                </select>
                <ChevronDown
                    className="pointer-events-none absolute right-3 top-1/2 size-4 -translate-y-1/2 text-foreground-muted"
                    aria-hidden="true"
                />
            </span>
        </label>
    );
}

/** Proposes bringing one branch into another; by default into main. */
export function NewPullRequestForm({ projectId, branches, onCreated, onCancel }: NewPullRequestFormProps) {
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
        <form onSubmit={handleSubmit} className="flex max-w-3xl flex-col gap-5">
            <div>
                <h2 className="text-base font-medium" style={{ fontFamily: "var(--font-syne)" }}>
                    New pull request
                </h2>
                <p className="text-sm text-foreground-muted">Propose bringing one branch into another.</p>
            </div>
            <div className="flex flex-col gap-3 sm:flex-row sm:items-end">
                <BranchSelect label="From" value={sourceId} branches={branches} onChange={setSourceId} />
                <ArrowRight className="hidden size-4 shrink-0 text-foreground-muted sm:mb-3 sm:block" aria-hidden="true" />
                <BranchSelect label="Into" value={targetId} branches={branches} onChange={setTargetId} />
            </div>
            {sourceId === targetId && (
                <p className="-mt-2 text-sm text-foreground-muted">Pick two different branches.</p>
            )}
            <label className="flex flex-col gap-1.5">
                <span className={labelClassName}>Title</span>
                <Input
                    value={title}
                    maxLength={255}
                    placeholder="e.g. Louder drop on the chorus"
                    onChange={(e) => setTitle(e.target.value)}
                />
            </label>
            <label className="flex flex-col gap-1.5">
                <span className={labelClassName}>
                    Description <span className="font-normal text-foreground-muted">(optional)</span>
                </span>
                <Textarea
                    value={description}
                    rows={3}
                    placeholder="What changed, and what should the reviewer listen for?"
                    onChange={(e) => setDescription(e.target.value)}
                />
            </label>
            {createPullRequest.isError && (
                <p role="alert" className="text-sm text-red-500">
                    {errorMessage(createPullRequest.error, "Couldn't open the pull request")}
                </p>
            )}
            <div className="flex justify-end gap-3">
                {onCancel && (
                    <Button type="button" variant="ghost" onClick={onCancel}>
                        Cancel
                    </Button>
                )}
                <Button type="submit" disabled={!canSubmit} loading={createPullRequest.isPending}>
                    Open pull request
                </Button>
            </div>
        </form>
    );
}
