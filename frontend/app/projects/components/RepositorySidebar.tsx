"use client";

import { RepositoryAudioPlayer } from "./RepositoryAudioPlayer";
import { ContributionActivity } from "./ContributionActivity";
import { TopContributors } from "./TopContributors";
import type { DailyActivity, ContributorStats } from "@/types/project";

interface RepositorySidebarProps {
    dailyActivity?: DailyActivity[];
    totalCommits?: number;
    totalContributors?: number;
    contributors?: ContributorStats[];
}

export function RepositorySidebar({
    dailyActivity = [],
    totalCommits = 0,
    totalContributors = 0,
    contributors = [],
}: RepositorySidebarProps) {
    return (
        <div className="flex flex-col gap-8 p-6">
            <RepositoryAudioPlayer />
            <div className="border-t border-foreground/[0.08] pt-6">
                <ContributionActivity
                    dailyActivity={dailyActivity}
                    totalCommits={totalCommits}
                    totalContributors={totalContributors}
                />
            </div>
            <div className="border-t border-foreground/[0.08] pt-6">
                <TopContributors contributors={contributors} />
            </div>
        </div>
    );
}
