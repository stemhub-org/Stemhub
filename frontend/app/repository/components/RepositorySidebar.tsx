"use client";

import { RepositoryAudioPlayer } from "./RepositoryAudioPlayer";
import { ContributionActivity } from "./ContributionActivity";
import { TopContributors } from "./TopContributors";

export function RepositorySidebar() {
    return (
        <div className="flex flex-col gap-8 p-6">
            <RepositoryAudioPlayer />
            <div className="border-t border-foreground/[0.08] pt-6">
                <ContributionActivity />
            </div>
            <div className="border-t border-foreground/[0.08] pt-6">
                <TopContributors />
            </div>
        </div>
    );
}
