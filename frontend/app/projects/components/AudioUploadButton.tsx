"use client";

import { useState, useRef } from "react";
import { UploadCloud, Loader2 } from "lucide-react";
import { API_URL } from "@/lib/api";

interface AudioUploadButtonProps {
    projectId?: string;
    onSuccess?: () => void;
}

export function AudioUploadButton({ projectId, onSuccess }: AudioUploadButtonProps) {
    const [isUploading, setIsUploading] = useState(false);
    const fileInputRef = useRef<HTMLInputElement>(null);

    const handleFileChange = async (event: React.ChangeEvent<HTMLInputElement>) => {
        const file = event.target.files?.[0];
        if (!file || !projectId) return;

        try {
            setIsUploading(true);

            const formData = new FormData();
            formData.append("file", file);

            const response = await fetch(`${API_URL}/projects/${projectId}/preview`, {
                method: "POST",
                credentials: "include",
                body: formData,
            });

            if (!response.ok) {
                const errorText = await response.text();
                throw new Error(errorText || "Upload failed");
            }

            // Safely call the success callback to silently update
            if (onSuccess) {
                onSuccess();
            }
        } catch (error) {
            console.error("Upload error:", error);
            alert("Failed to upload project preview. Please try again.");
        } finally {
            setIsUploading(false);
            if (fileInputRef.current) {
                fileInputRef.current.value = "";
            }
        }
    };

    return (
        <>
            <input
                type="file"
                ref={fileInputRef}
                onChange={handleFileChange}
                accept="audio/mpeg, audio/wav, audio/mp3, .mp3, .wav"
                className="hidden"
                disabled={!projectId || isUploading}
                aria-hidden="true"
            />
            <button
                type="button"
                onClick={() => fileInputRef.current?.click()}
                disabled={!projectId || isUploading}
                className="inline-flex items-center gap-2 rounded-xl bg-accent px-4 py-2 text-sm font-medium text-white transition-opacity hover:opacity-90 disabled:opacity-50 disabled:cursor-not-allowed"
            >
                {isUploading ? (
                    <Loader2 className="size-4 animate-spin" aria-hidden />
                ) : (
                    <UploadCloud className="size-4" aria-hidden />
                )}
                {isUploading ? "Uploading..." : "Upload Preview"}
            </button>
        </>
    );
}
