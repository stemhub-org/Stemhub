/** "just now", "45m ago", "5h ago", "3d ago", then the local date after a week. */
export function formatTimeAgo(dateString: string, now: Date = new Date()): string {
    const date = new Date(dateString);
    const diffMinutes = Math.floor((now.getTime() - date.getTime()) / 60000);
    const diffHours = Math.floor(diffMinutes / 60);
    const diffDays = Math.floor(diffHours / 24);

    if (diffMinutes < 1) return "just now";
    if (diffMinutes < 60) return `${diffMinutes}m ago`;
    if (diffHours < 24) return `${diffHours}h ago`;
    if (diffDays < 7) return `${diffDays}d ago`;
    return date.toLocaleDateString();
}

/** The full local date and time, for a tooltip next to a relative time. */
export function formatDateTime(dateString: string): string {
    return new Date(dateString).toLocaleString();
}
