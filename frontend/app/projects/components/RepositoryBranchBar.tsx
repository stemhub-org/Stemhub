"use client";

import { useState, useRef, useEffect } from "react";
import { GitBranch, ChevronDown, Trash2, Plus } from "lucide-react";
import type { Branch } from "@/types/project";

interface RepositoryBranchBarProps {
    branches: Branch[];
    selectedBranchId: string;
    onBranchChange: (branchId: string) => void;
    isOwner?: boolean;
    onDelete?: (branchId: string) => void;
    onCreate?: (branchName: string) => Promise<void>;
}

export function RepositoryBranchBar({
    branches,
    selectedBranchId,
    onBranchChange,
    isOwner,
    onDelete,
    onCreate,
}: RepositoryBranchBarProps) {
    const [isOpen, setIsOpen] = useState(false);
    const [isCreating, setIsCreating] = useState(false);
    const [newBranchName, setNewBranchName] = useState("");
    const [isSubmitting, setIsSubmitting] = useState(false);
    const dropdownRef = useRef<HTMLDivElement>(null);

    useEffect(() => {
        function handleClickOutside(event: MouseEvent) {
            if (dropdownRef.current && !dropdownRef.current.contains(event.target as Node)) {
                setIsOpen(false);
                setIsCreating(false);
                setNewBranchName("");
            }
        }
        document.addEventListener("mousedown", handleClickOutside);
        return () => document.removeEventListener("mousedown", handleClickOutside);
    }, []);

    const selectedBranchName = branches.find((branch) => branch.id === selectedBranchId)?.name || "main";

    return (
        <div className="flex flex-wrap items-center gap-4">
            <div className="relative" ref={dropdownRef}>
                <button
                    type="button"
                    onClick={() => setIsOpen((prev) => !prev)}
                    className="flex items-center gap-2 rounded-xl bg-background-secondary dark:bg-background-tertiary border border-border-subtle px-4 py-2.5 text-sm font-medium text-foreground transition-colors hover:bg-background-tertiary"
                    aria-expanded={isOpen}
                    aria-haspopup="listbox"
                    aria-label="Sélectionner la version"
                >
                    <GitBranch className="size-4 text-foreground/60" aria-hidden />
                    <span>{selectedBranchName}</span>
                    <ChevronDown
                        className={`size-4 text-foreground/60 transition-transform ${isOpen ? "rotate-180" : ""}`}
                        aria-hidden
                    />
                </button>
                {isOpen && (
                    <ul
                        role="listbox"
                        className="absolute left-0 top-full z-10 mt-1 min-w-[12rem] rounded-xl bg-background-secondary dark:bg-background-tertiary border border-border-subtle py-1 shadow-lg"
                    >
                        {branches.map((branch) => (
                            <li key={branch.id} role="option" className="flex items-center justify-between hover:bg-background-tertiary">
                                <button
                                    type="button"
                                    onClick={() => {
                                        onBranchChange(branch.id);
                                        setIsOpen(false);
                                    }}
                                    className={`flex w-full items-center gap-2 px-4 py-2.5 text-left text-sm transition-colors ${selectedBranchId === branch.id ? "font-medium text-accent" : "text-foreground"}`}
                                >
                                    <GitBranch className="size-4 shrink-0 text-foreground/60" aria-hidden />
                                    {branch.name}
                                </button>
                                {isOwner && branch.name !== "main" && (
                                    <button
                                        type="button"
                                        onClick={(e) => {
                                            e.stopPropagation();
                                            if (onDelete && window.confirm(`Are you sure you want to delete version '${branch.name}'?`)) {
                                                onDelete(branch.id);
                                            }
                                        }}
                                        className="px-3 py-2 text-foreground/40 hover:text-red-500 transition-colors"
                                        title="Delete Workspace"
                                    >
                                        <Trash2 className="size-4" />
                                    </button>
                                )}
                            </li>
                        ))}
                        {onCreate && (
                            <li className="border-t border-border-subtle p-2 mt-1">
                                {isCreating ? (
                                    <form 
                                        onSubmit={async (e) => {
                                            e.preventDefault();
                                            if (!newBranchName.trim()) return;
                                            setIsSubmitting(true);
                                            try {
                                                await onCreate(newBranchName.trim());
                                                setIsOpen(false);
                                                setIsCreating(false);
                                                setNewBranchName("");
                                            } catch (err) {
                                                console.error(err);
                                            } finally {
                                                setIsSubmitting(false);
                                            }
                                        }} 
                                        className="flex flex-col gap-2"
                                    >
                                        <input 
                                            type="text"
                                            value={newBranchName}
                                            onChange={e => setNewBranchName(e.target.value)}
                                            placeholder="New version name..."
                                            className="w-full bg-background-secondary dark:bg-background-tertiary border border-border-subtle rounded px-2 py-1.5 text-sm focus:outline-none focus:border-accent"
                                            disabled={isSubmitting}
                                            autoFocus
                                        />
                                        <div className="flex items-center justify-end gap-2">
                                            <button 
                                                type="button" 
                                                onClick={() => {
                                                    setIsCreating(false);
                                                    setNewBranchName("");
                                                }} 
                                                className="px-2 py-1 text-xs font-medium text-foreground/60 hover:text-foreground transition-colors disabled:opacity-50"
                                                disabled={isSubmitting}
                                            >
                                                Cancel
                                            </button>
                                            <button 
                                                type="submit" 
                                                disabled={!newBranchName.trim() || isSubmitting} 
                                                className="px-3 py-1 text-xs font-medium bg-accent text-white rounded hover:bg-accent/90 disabled:opacity-50 transition-colors"
                                            >
                                                {isSubmitting ? "Creating..." : "Create"}
                                            </button>
                                        </div>
                                    </form>
                                ) : (
                                    <button
                                        type="button"
                                        onClick={() => setIsCreating(true)}
                                        className="flex w-full items-center gap-2 px-2 py-2 text-left text-sm font-medium text-accent/90 transition-colors hover:text-accent hover:bg-accent/10 rounded"
                                    >
                                        <Plus className="size-4 shrink-0" />
                                        Create new version
                                    </button>
                                )}
                            </li>
                        )}
                    </ul>
                )}
            </div>
            <div className="flex items-center gap-2 text-sm text-foreground/70">
                <GitBranch className="size-4 shrink-0 text-foreground/60" aria-hidden />
                <span>
                    {branches.length} {branches.length === 1 ? "Version" : "Versions"}
                </span>
            </div>
        </div>
    );
}
