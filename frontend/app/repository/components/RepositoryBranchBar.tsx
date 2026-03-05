"use client";

import { useState, useRef, useEffect } from "react";
import { GitBranch, ChevronDown } from "lucide-react";

const PLACEHOLDER_BRANCHES = ["main", "develop", "feature/vocals", "fix/mix"];
const PLACEHOLDER_CURRENT_BRANCH = "main";

export function RepositoryBranchBar() {
    const [isOpen, setIsOpen] = useState(false);
    const [selectedBranch, setSelectedBranch] = useState(PLACEHOLDER_CURRENT_BRANCH);
    const dropdownRef = useRef<HTMLDivElement>(null);

    useEffect(() => {
        function handleClickOutside(event: MouseEvent) {
            if (dropdownRef.current && !dropdownRef.current.contains(event.target as Node)) {
                setIsOpen(false);
            }
        }
        document.addEventListener("mousedown", handleClickOutside);
        return () => document.removeEventListener("mousedown", handleClickOutside);
    }, []);

    return (
        <div className="flex flex-wrap items-center gap-4">
            <div className="relative" ref={dropdownRef}>
                <button
                    type="button"
                    onClick={() => setIsOpen((prev) => !prev)}
                    className="flex items-center gap-2 rounded-xl bg-background-secondary border border-border-subtle px-4 py-2.5 text-sm font-medium text-foreground transition-colors hover:bg-background-tertiary"
                    aria-expanded={isOpen}
                    aria-haspopup="listbox"
                    aria-label="Sélectionner la branche"
                >
                    <GitBranch className="size-4 text-foreground/60" aria-hidden />
                    <span>{selectedBranch}</span>
                    <ChevronDown
                        className={`size-4 text-foreground/60 transition-transform ${isOpen ? "rotate-180" : ""}`}
                        aria-hidden
                    />
                </button>
                {isOpen && (
                    <ul
                        role="listbox"
                        className="absolute left-0 top-full z-10 mt-1 min-w-[12rem] rounded-xl bg-background-secondary border border-border-subtle py-1 shadow-lg"
                    >
                        {PLACEHOLDER_BRANCHES.map((branch) => (
                            <li key={branch} role="option">
                                <button
                                    type="button"
                                    onClick={() => {
                                        setSelectedBranch(branch);
                                        setIsOpen(false);
                                    }}
                                    className={`flex w-full items-center gap-2 px-4 py-2.5 text-left text-sm transition-colors hover:bg-background-tertiary ${selectedBranch === branch ? "font-medium text-accent-blue" : "text-foreground"}`}
                                >
                                    <GitBranch className="size-4 shrink-0 text-foreground/60" aria-hidden />
                                    {branch}
                                </button>
                            </li>
                        ))}
                    </ul>
                )}
            </div>
            <div className="flex items-center gap-2 text-sm text-foreground/70">
                <GitBranch className="size-4 shrink-0 text-foreground/60" aria-hidden />
                <span>{PLACEHOLDER_BRANCHES.length} Versions</span>
            </div>
        </div>
    );
}
