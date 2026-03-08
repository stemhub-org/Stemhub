"use client";

import { Play, Pause } from "lucide-react";
import { useRef, useState, useEffect } from "react";
import { Track } from "@/types/project";
import { API_URL } from "@/lib/api";

function formatTimeAgo(dateString: string | null) {
    if (!dateString) return "Unknown date";
    const date = new Date(dateString);
    const now = new Date();
    const seconds = Math.floor((now.getTime() - date.getTime()) / 1000);
    
    if (seconds < 60) return "Just now";
    
    const minutes = Math.floor(seconds / 60);
    if (minutes < 60) return `${minutes} minute${minutes > 1 ? 's' : ''} ago`;
    
    const hours = Math.floor(minutes / 60);
    if (hours < 24) return `${hours} hour${hours > 1 ? 's' : ''} ago`;
    
    const days = Math.floor(hours / 24);
    if (days < 30) return `${days} day${days > 1 ? 's' : ''} ago`;
    
    const months = Math.floor(days / 30);
    if (months < 12) return `${months} month${months > 1 ? 's' : ''} ago`;
    
    const years = Math.floor(months / 12);
    return `${years} year${years > 1 ? 's' : ''} ago`;
}

const PLACEHOLDER_SAMPLE_RATE = "48 kHz";
const PLACEHOLDER_BIT_DEPTH = "24 bit";

function WaveformPlaceholder({ progress }: { progress: number }) {
    const bars = [
        0.4, 0.7, 0.9, 0.6, 0.3, 0.5, 0.8, 0.9, 0.7, 0.4, 0.3, 0.5, 0.6, 0.8,
        0.9, 0.7, 0.5, 0.4, 0.6, 0.9, 0.8, 0.6, 0.3, 0.4, 0.5, 0.7, 0.9, 0.8,
        0.6, 0.4, 0.3, 0.5, 0.7, 0.9, 0.8, 0.5, 0.4, 0.6, 0.7, 0.9, 0.8, 0.6,
        0.4, 0.3, 0.5, 0.7, 0.9, 0.8, 0.6, 0.4,
    ];

    const progressIndex = Math.floor(bars.length * progress);

    return (
        <div className="flex h-14 w-full items-center rounded-lg bg-background-tertiary px-4">
            <div className="flex h-full w-full items-end gap-[1px]">
                {bars.map((height, index) => {
                    const baseHeight = 18;
                    const extra = height * 26;
                    const barHeight = baseHeight + extra;
                    const isPlayed = index <= progressIndex;

                    return (
                        <div
                            key={index}
                            className="flex-1 rounded-[1px]"
                            style={{
                                height: `${barHeight}px`,
                                backgroundColor: isPlayed
                                    ? "var(--accent)"
                                    : "var(--border-subtle)",
                            }}
                        />
                    );
                })}
            </div>
        </div>
    );
}

export function RepositoryAudioPlayer({ track }: { track?: Track }) {
    const audioRef = useRef<HTMLAudioElement>(null);
    const [isPlaying, setIsPlaying] = useState(false);
    const [currentTime, setCurrentTime] = useState(0);
    const [duration, setDuration] = useState(0);
    const [token, setToken] = useState<string | null>(null);

    useEffect(() => {
        setToken(localStorage.getItem("token"));
    }, []);

    const togglePlay = () => {
        if (!audioRef.current || !track) return;
        if (isPlaying) {
            audioRef.current.pause();
        } else {
            audioRef.current.play().catch(console.error);
        }
    };

    const formatTime = (time: number) => {
        if (!time || isNaN(time)) return "0:00";
        const minutes = Math.floor(time / 60);
        const seconds = Math.floor(time % 60);
        return `${minutes}:${seconds.toString().padStart(2, "0")}`;
    };

    const progress = duration > 0 ? currentTime / duration : 0;

    return (
        <div className="flex flex-col gap-6 relative">
            {track && token && (
                <audio
                    ref={audioRef}
                    src={`${API_URL}/tracks/${track.id}/audio?token=${token}`}
                    onTimeUpdate={(e) => setCurrentTime(e.currentTarget.currentTime)}
                    onLoadedMetadata={(e) => setDuration(e.currentTarget.duration)}
                    onPlay={() => setIsPlaying(true)}
                    onPause={() => setIsPlaying(false)}
                    onEnded={() => setIsPlaying(false)}
                />
            )}
            <div>
                <div className="flex items-center justify-between">
                    <p className="text-base font-medium text-foreground">
                        {track ? "Track Preview" : "No Track Available"}
                    </p>
                    {track && track.created_at && (
                        <p className="text-sm text-foreground/50">
                            Uploaded {formatTimeAgo(track.created_at)}
                        </p>
                    )}
                </div>
                <p className="mt-0.5 text-sm text-foreground/70">
                    {track ? track.name : "Upload an audio file to preview"}
                </p>
            </div>
            <WaveformPlaceholder progress={progress} />
            <div className="flex items-center gap-4">
                <button
                    type="button"
                    onClick={togglePlay}
                    disabled={!track}
                    className="flex size-12 shrink-0 items-center justify-center rounded-full bg-accent text-white transition-opacity hover:opacity-90 disabled:opacity-50 disabled:cursor-not-allowed"
                    aria-label={isPlaying ? "Pause" : "Play"}
                >
                    {isPlaying ? (
                        <Pause className="size-6 fill-current" aria-hidden />
                    ) : (
                        <Play className="size-6 fill-current ml-1" aria-hidden />
                    )}
                </button>
                <div className="min-w-0 flex-1">
                    <div className="h-1.5 w-full rounded-full bg-foreground/10 overflow-hidden relative" 
                         onClick={(e) => {
                             if (!audioRef.current || !track) return;
                             const rect = e.currentTarget.getBoundingClientRect();
                             const x = e.clientX - rect.left;
                             const pct = x / rect.width;
                             audioRef.current.currentTime = pct * duration;
                         }}
                         style={{ cursor: track ? 'pointer' : 'default' }}
                    >
                        <div 
                            className="h-full absolute left-0 top-0 rounded-full bg-accent" 
                            style={{ width: `${progress * 100}%` }}
                        />
                    </div>
                    <p className="mt-1 text-sm text-foreground/60">
                        {formatTime(currentTime)} / {formatTime(duration)}
                    </p>
                </div>
            </div>
        </div>
    );
}
