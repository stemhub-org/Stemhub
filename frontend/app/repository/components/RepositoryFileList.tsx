"use client";

import { Folder, FileAudio } from "lucide-react";

type RepositoryItem = {
    name: string;
    type: "folder" | "file";
    commitMessage: string;
    updated: string;
};

const PLACEHOLDER_ITEMS: RepositoryItem[] = [
    {
        name: "Drums",
        type: "folder",
        commitMessage: "Updated kick pattern with sidechain",
        updated: "2h ago",
    },
    {
        name: "Bassline",
        type: "folder",
        commitMessage: "Added sub bass layer",
        updated: "3h ago",
    },
    {
        name: "Vocals",
        type: "folder",
        commitMessage: "Merged vocal harmonies",
        updated: "5h ago",
    },
    {
        name: "Main_Project.als",
        type: "file",
        commitMessage: "Reorganized mixer tracks",
        updated: "1h ago",
    },
    {
        name: "Bass_Bus.wav",
        type: "file",
        commitMessage: "Exported with new compression",
        updated: "2h ago",
    },
    {
        name: "Master_v3.wav",
        type: "file",
        commitMessage: "Final master with limiting",
        updated: "1d ago",
    },
];

function ItemIcon({ type }: { type: RepositoryItem["type"] }) {
    const iconClass = "size-5 shrink-0 text-accent";
    if (type === "folder") {
        return <Folder className={iconClass} aria-hidden />;
    }
    return (
        <FileAudio className={iconClass} aria-hidden />
    );
}

export function RepositoryFileList() {
    return (
        <div>
            <div className="border-b border-foreground/[0.08] bg-foreground/[0.02] px-6 py-3">
                <div className="grid grid-cols-[1fr_2fr_auto] gap-4 text-xs font-medium uppercase tracking-wide text-foreground/60">
                    <span>Name</span>
                    <span>Commit message</span>
                    <span>Updated</span>
                </div>
            </div>
            <ul className="divide-y divide-foreground/[0.06]" role="list">
                {PLACEHOLDER_ITEMS.map((item) => (
                    <li key={item.name}>
                        <button
                            type="button"
                            className="grid w-full grid-cols-[1fr_2fr_auto] gap-4 px-6 py-4 text-left transition-colors hover:bg-foreground/[0.03]"
                        >
                            <span className="flex min-w-0 items-center gap-3 truncate">
                                <ItemIcon type={item.type} />
                                <span className="truncate font-medium text-foreground">
                                    {item.name}
                                </span>
                            </span>
                            <span className="truncate text-sm text-foreground/70">
                                {item.commitMessage}
                            </span>
                            <span className="shrink-0 text-sm text-foreground/60">
                                {item.updated}
                            </span>
                        </button>
                    </li>
                ))}
            </ul>
        </div>
    );
}
