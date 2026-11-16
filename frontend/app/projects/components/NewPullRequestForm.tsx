"use client";

import { useEffect, useId, useRef, useState } from "react";
import { Check, ChevronDown, GitBranch } from "lucide-react";
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

const labelClassName = "text-sm font-medium text-foreground";

interface BranchSelectProps {
    label: string;
    value: string;
    branches: Branch[];
    onChange: (branchId: string) => void;
}

/**
 * A branch dropdown drawn by StemHub (same look as the BranchPicker on the
 * Project tab) rather than the browser's native menu, which can't be styled.
 */
function BranchSelect({ label, value, branches, onChange }: BranchSelectProps) {
    const [open, setOpen] = useState(false);
    const rootRef = useRef<HTMLDivElement>(null);
    const labelId = useId();
    const selected = branches.find((branch) => branch.id === value);

    useEffect(() => {
        if (!open) return;
        const closeOnOutsideClick = (event: MouseEvent) => {
            if (!rootRef.current?.contains(event.target as Node)) setOpen(false);
        };
        const closeOnEscape = (event: KeyboardEvent) => {
            if (event.key === "Escape") setOpen(false);
        };
        document.addEventListener("mousedown", closeOnOutsideClick);
        document.addEventListener("keydown", closeOnEscape);
        return () => {
            document.removeEventListener("mousedown", closeOnOutsideClick);
            document.removeEventListener("keydown", closeOnEscape);
        };
    }, [open]);

    return (
        <div ref={rootRef} className="relative flex min-w-0 flex-col gap-1.5">
            <span id={labelId} className={labelClassName}>
                {label}
            </span>
            <button
                type="button"
                aria-labelledby={labelId}
                aria-haspopup="listbox"
                aria-expanded={open}
                onClick={() => setOpen((wasOpen) => !wasOpen)}
                className="flex w-full items-center gap-2 rounded-lg border border-border-subtle bg-background px-3 py-2.5 text-left text-sm text-foreground transition-colors hover:bg-background-tertiary focus:border-accent focus:outline-none focus:ring-1 focus:ring-accent/50"
            >
                <GitBranch className="size-4 shrink-0 text-foreground/60" aria-hidden="true" />
                <span className="min-w-0 flex-1 truncate">{selected?.name ?? "Choose a branch"}</span>
                <ChevronDown
                    className={`size-4 shrink-0 text-foreground/60 transition-transform ${open ? "rotate-180" : ""}`}
                    aria-hidden="true"
                />
            </button>
            {open && (
                <ul
                    role="listbox"
                    aria-labelledby={labelId}
                    className="absolute left-0 right-0 top-full z-20 mt-1 max-h-64 overflow-y-auto rounded-xl border border-border-subtle bg-background-secondary py-1 shadow-lg dark:bg-background-tertiary"
                >
                    {branches.map((branch) => {
                        const isSelected = branch.id === value;
                        return (
                            <li
                                key={branch.id}
                                role="option"
                                aria-selected={isSelected}
                                onClick={() => {
                                    onChange(branch.id);
                                    setOpen(false);
                                }}
                                className={`flex cursor-pointer items-center gap-2 px-4 py-2.5 text-sm transition-colors hover:bg-background-tertiary ${isSelected ? "font-medium text-accent" : "text-foreground"}`}
                            >
                                <GitBranch className="size-4 shrink-0 text-foreground/60" aria-hidden="true" />
                                <span className="min-w-0 flex-1 truncate">{branch.name}</span>
                                {isSelected && <Check className="size-4 shrink-0" aria-hidden="true" />}
                            </li>
                        );
                    })}
                </ul>
            )}
        </div>
    );
}

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
        <form onSubmit={handleSubmit} className="flex flex-col gap-4">
            <div>
                <h2 className="text-base font-medium" style={{ fontFamily: "var(--font-syne)" }}>
                    New pull request
                </h2>
                <p className="text-sm text-foreground-muted">Propose bringing one branch into another.</p>
            </div>
            <BranchSelect label="From" value={sourceId} branches={branches} onChange={setSourceId} />
            <BranchSelect label="Into" value={targetId} branches={branches} onChange={setTargetId} />
            {sourceId === targetId && (
                <p className="-mt-1 text-sm text-foreground-muted">Pick two different branches.</p>
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
            <Button type="submit" className="w-full" disabled={!canSubmit} loading={createPullRequest.isPending}>
                Open pull request
            </Button>
        </form>
    );
}
