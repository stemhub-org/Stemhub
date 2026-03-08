"use client";

import { useState, useRef } from "react";
import { UploadCloud, Loader2 } from "lucide-react";
import { API_URL } from "@/lib/api";

interface AudioUploadButtonProps {
    versionId?: string;
    onSuccess?: () => void;
}

export function AudioUploadButton({ versionId, onSuccess }: AudioUploadButtonProps) {
    const [isUploading, setIsUploading] = useState(false);
    const fileInputRef = useRef<HTMLInputElement>(null);

    const handleFileChange = async (event: React.ChangeEvent<HTMLInputElement>) => {
        const file = event.target.files?.[0];
        if (!file || !versionId) return;

        try {
            setIsUploading(true);
            const token = localStorage.getItem("token");
            if (!token) throw new Error("No authentication token found");

            const formData = new FormData();
            formData.append("file", file);

            const response = await fetch(`${API_URL}/versions/${versionId}/tracks/upload`, {
                method: "POST",
                headers: {
                    Authorization: `Bearer ${token}`,
                },
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
            alert("Failed to upload audio file. Please try again.");
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
                disabled={!versionId || isUploading}
                aria-hidden="true"
            />
            <button
                type="button"
                onClick={() => fileInputRef.current?.click()}
                disabled={!versionId || isUploading}
                className="inline-flex items-center gap-2 rounded-xl bg-accent px-4 py-2 text-sm font-medium text-white transition-opacity hover:opacity-90 disabled:opacity-50 disabled:cursor-not-allowed"
            >
                {isUploading ? (
                    <Loader2 className="size-4 animate-spin" aria-hidden />
                ) : (
                    <UploadCloud className="size-4" aria-hidden />
                )}
                {isUploading ? "Uploading..." : "Upload Audio"}
            </button>
        </>
    );
}
