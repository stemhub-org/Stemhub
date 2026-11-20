"use client";

import { Loader2 } from "lucide-react";
import { Card } from "@/components/ui/Card";
import { ErrorState } from "@/components/ui/ErrorState";
import { changeLabel, formatChangeValue, insertLabel, mixerDiffSummaryLine } from "@/lib/mixerDiff";
import { usePullRequestDiff } from "@/lib/queries";
import type { MixerDiffChange } from "@/types/project";
import type { PullRequestMixerDiff } from "@/types/pullRequest";

interface PullRequestDiffProps {
    pullRequestId: string;
    /** Branch that would receive the changes (left column). */
    targetName: string;
    /** Branch being proposed (right column). */
    sourceName: string;
}

interface InsertGroup {
    label: string;
    changes: MixerDiffChange[];
}

/** Consecutive changes of the same insert, in the order the backend sent them. */
function groupByInsert(changes: MixerDiffChange[]): InsertGroup[] {
    const groups: InsertGroup[] = [];
    for (const change of changes) {
        const label = insertLabel(change);
        const last = groups[groups.length - 1];
        if (last && last.label === label) last.changes.push(change);
        else groups.push({ label, changes: [change] });
    }
    return groups;
}

/** Why there is no table, for every status other than "compared". */
function statusMessage(diff: PullRequestMixerDiff, targetName: string, sourceName: string): string {
    switch (diff.status) {
        case "no_source_version":
            return `${sourceName} has no version yet: there is nothing to accept.`;
        case "no_target_version":
            return `${targetName} has no version yet: accepting will start its history with the latest version of ${sourceName}.`;
        case "target_moved":
            return `${targetName} has new versions since this pull request was opened, so it can no longer be accepted. Open a new pull request to review them.`;
        default:
            return diff.status_message ?? "These versions can't be compared.";
    }
}

function ChangesTable({ changes, targetName, sourceName }: { changes: MixerDiffChange[]; targetName: string; sourceName: string }) {
    return (
        <div className="overflow-x-auto">
            <table className="w-full text-left text-sm">
                <thead>
                    <tr className="border-b border-border-subtle text-xs uppercase tracking-wide text-foreground-muted">
                        <th scope="col" className="px-6 py-3 font-medium">Change</th>
                        <th scope="col" className="w-1/3 px-6 py-3 font-medium">{targetName}</th>
                        <th scope="col" className="w-1/3 px-6 py-3 font-medium">{sourceName}</th>
                    </tr>
                </thead>
                {groupByInsert(changes).map((group, groupIndex) => (
                    <tbody key={`${group.label}-${groupIndex}`} className="border-b border-border-subtle last:border-b-0">
                        <tr>
                            <th scope="rowgroup" colSpan={3} className="px-6 pb-1 pt-4 font-medium">
                                {group.label}
                            </th>
                        </tr>
                        {group.changes.map((change, index) => (
                            <tr key={`${change.type}-${change.slot_index ?? "insert"}-${index}`}>
                                <th scope="row" className="px-6 py-2 pl-10 font-normal text-foreground-muted">
                                    {changeLabel(change)}
                                </th>
                                <td className="px-6 py-2 text-foreground-muted">{formatChangeValue(change.before)}</td>
                                <td className="px-6 py-2 font-medium text-accent">{formatChangeValue(change.after)}</td>
                            </tr>
                        ))}
                    </tbody>
                ))}
            </table>
        </div>
    );
}

function DiffBody({ diff, targetName, sourceName }: { diff: PullRequestMixerDiff; targetName: string; sourceName: string }) {
    if (diff.status !== "compared") {
        return <p className="px-6 py-5 text-sm text-foreground-muted">{statusMessage(diff, targetName, sourceName)}</p>;
    }
    if (diff.changes.length === 0) {
        return <p className="px-6 py-5 text-sm text-foreground-muted">No mixer changes.</p>;
    }
    // A change that belongs to no insert (the project file itself) has no before/after to put in columns.
    const projectChanges = diff.changes.filter((change) => change.insert_index === null);
    const insertChanges = diff.changes.filter((change) => change.insert_index !== null);
    return (
        <>
            {projectChanges.map((change, index) => (
                <p key={`${change.type}-${index}`} className="px-6 py-5 text-sm text-foreground-muted">
                    {change.message}
                </p>
            ))}
            {insertChanges.length > 0 && <ChangesTable changes={insertChanges} targetName={targetName} sourceName={sourceName} />}
        </>
    );
}

/** The mixer changes accepting an open pull request would apply: target branch on the left, source on the right. */
export function PullRequestDiff({ pullRequestId, targetName, sourceName }: PullRequestDiffProps) {
    const { data, isPending, isError, error, refetch } = usePullRequestDiff(pullRequestId, true);

    return (
        <Card padding="none">
            <div className="flex flex-wrap items-baseline justify-between gap-2 border-b border-border-subtle px-6 py-4">
                <h2 className="text-base font-medium" style={{ fontFamily: "var(--font-syne)" }}>
                    Changes
                </h2>
                {data?.summary && data.summary.total_changes > 0 && (
                    <span className="text-sm text-foreground-muted">{mixerDiffSummaryLine(data.summary)}</span>
                )}
            </div>
            {isPending ? (
                <div className="py-6">
                    <Loader2 className="mx-auto size-5 animate-spin text-accent" aria-label="Loading changes…" />
                </div>
            ) : isError ? (
                <div className="p-6">
                    <ErrorState title="Couldn't load the changes" error={error} onRetry={() => void refetch()} />
                </div>
            ) : (
                <DiffBody diff={data} targetName={targetName} sourceName={sourceName} />
            )}
        </Card>
    );
}
