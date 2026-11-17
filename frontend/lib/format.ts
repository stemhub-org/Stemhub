/** A byte count in the unit that keeps the number short: "512 B", "1.5 KB", "5.0 MB", "1.5 GB". */
export function formatBytes(bytes: number | null): string {
    if (bytes == null) return "—";
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 ** 2) return `${(bytes / 1024).toFixed(1)} KB`;
    if (bytes < 1024 ** 3) return `${(bytes / 1024 ** 2).toFixed(1)} MB`;
    return `${(bytes / 1024 ** 3).toFixed(1)} GB`;
}
