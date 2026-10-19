// Extensions the backend accepts for an uploaded preview, keyed by the media
// type it serves them with (see routers/projects.py).
const PREVIEW_EXTENSIONS: Record<string, string> = {
    "audio/wav": ".wav",
    "audio/x-wav": ".wav",
    "audio/wave": ".wav",
    "audio/mpeg": ".mp3",
    "audio/ogg": ".ogg",
    "audio/flac": ".flac",
    "audio/x-flac": ".flac",
};

function sanitizeFilenamePart(value: string): string {
    return value
        .trim()
        .toLowerCase()
        .replace(/[^a-z0-9-_]+/g, "-")
        .replace(/-+/g, "-")
        .replace(/^-|-$/g, "") || "project";
}

/**
 * Reads the filename from a Content-Disposition header, preferring the
 * RFC 5987 `filename*` form. Returns null when the header is missing,
 * unreadable (cross-origin responses only expose it when the server allows
 * it) or has no filename.
 */
export function filenameFromContentDisposition(header: string | null): string | null {
    if (!header) return null;

    const encoded = /filename\*\s*=\s*(?:[\w-]+)?'[^']*'([^;]+)/i.exec(header);
    if (encoded) {
        try {
            const decoded = decodeURIComponent(encoded[1].trim().replace(/^"|"$/g, ""));
            if (decoded) return decoded;
        } catch {
            // Malformed percent-encoding: fall back to the plain `filename`.
        }
    }

    const plain = /filename\s*=\s*("([^"]*)"|[^;]+)/i.exec(header);
    const value = (plain?.[2] ?? plain?.[1] ?? "").trim();
    return value || null;
}

/**
 * Name for a downloaded preview: the server's filename when readable,
 * otherwise "<project>-preview" plus the extension of the uploaded file,
 * recovered from the response's media type.
 */
export function previewFilename(
    contentDisposition: string | null,
    contentType: string | null,
    projectName: string,
): string {
    const serverName = filenameFromContentDisposition(contentDisposition);
    if (serverName) return serverName;

    const mediaType = (contentType ?? "").split(";")[0].trim().toLowerCase();
    const extension = PREVIEW_EXTENSIONS[mediaType] ?? "";
    return `${sanitizeFilenamePart(projectName)}-preview${extension}`;
}

/** Hands a blob to the browser as a file download. */
export function saveBlob(blob: Blob, filename: string): void {
    const url = window.URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = filename;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    window.URL.revokeObjectURL(url);
}
