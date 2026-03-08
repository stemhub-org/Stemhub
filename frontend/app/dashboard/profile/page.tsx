"use client";

import { useEffect, useState, useRef } from "react";
import { useRouter } from "next/navigation";
import { motion } from "framer-motion";
import {
    MapPin,
    Link as LinkIcon,
    Calendar,
    Music,
    Heart,
    Headphones,
    GitBranch,
    Users,
    Edit2,
    Search,
    X,
    Loader2,
    Check
} from "lucide-react";

const GENRE_GROUPS = [
    { label: "Electronic & Dance", genres: ["breakbeat", "chicago-house", "club", "dance", "dancehall", "deep-house", "detroit-techno", "disco", "drum-and-bass", "dub", "dubstep", "edm", "electro", "electronic", "garage", "house", "idm", "minimal-techno", "post-dubstep", "progressive-house", "techno", "trance"] },
    { label: "Rock & Metal", genres: ["alt-rock", "alternative", "black-metal", "death-metal", "emo", "goth", "grindcore", "grunge", "hard-rock", "hardcore", "hardstyle", "heavy-metal", "indie", "indie-pop", "industrial", "metal", "metal-misc", "metalcore", "psych-rock", "punk", "punk-rock", "rock", "rock-n-roll", "rockabilly"] },
    { label: "Pop", genres: ["cantopop", "j-idol", "j-pop", "k-pop", "mandopop", "pop", "pop-film", "power-pop", "synth-pop"] },
    { label: "Hip-Hop & R&B", genres: ["afrobeat", "funk", "groove", "hip-hop", "r-n-b", "soul", "trip-hop"] },
    { label: "Acoustic & Folk", genres: ["acoustic", "bluegrass", "folk", "guitar", "piano", "romance", "sad", "singer-songwriter", "songwriter"] },
    { label: "Latin & Caribbean", genres: ["bossanova", "brazil", "forro", "latin", "latino", "pagode", "reggae", "reggaeton", "salsa", "samba", "sertanejo", "ska", "tango"] },
    { label: "World & Regional", genres: ["british", "french", "german", "indian", "iranian", "j-dance", "j-rock", "malay", "philippines-opm", "spanish", "swedish", "turkish", "world-music"] },
    { label: "Other & Moods", genres: ["ambient", "anime", "children", "chill", "classical", "comedy", "country", "disney", "gospel", "happy", "holidays", "honky-tonk", "jazz", "kids", "movies", "mpb", "new-age", "new-release", "opera", "party", "rainy-day", "road-trip", "show-tunes", "sleep", "soundtracks", "study", "summer", "work-out"] }
];

export default function ProfilePage() {
    const router = useRouter();
    const [user, setUser] = useState<any>(null);
    const [loading, setLoading] = useState(true);
    const [isEditModalOpen, setIsEditModalOpen] = useState(false);
    const [editForm, setEditForm] = useState<{
        username: string;
        location: string;
        website: string;
        avatar_url: string;
        bio: string;
        genres: string[];
    }>({
        username: "",
        location: "",
        website: "",
        avatar_url: "",
        bio: "",
        genres: []
    });
    const [isGenreModalOpen, setIsGenreModalOpen] = useState(false);
    const [genreSearch, setGenreSearch] = useState("");
    const [isSaving, setIsSaving] = useState(false);
    const [locationSuggestions, setLocationSuggestions] = useState<any[]>([]);
    const [isSearchingLocation, setIsSearchingLocation] = useState(false);
    const searchTimeoutRef = useRef<NodeJS.Timeout | null>(null);
    const fileInputRef = useRef<HTMLInputElement>(null);
    const [featuredFavorites, setFeaturedFavorites] = useState<Record<string, boolean>>({});

    const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
        const file = e.target.files?.[0];
        if (file) {
            const reader = new FileReader();
            reader.onloadend = () => {
                setEditForm((prev) => ({ ...prev, avatar_url: reader.result as string }));
            };
            reader.readAsDataURL(file);
        }
    };

    useEffect(() => {
        const fetchUser = async () => {
            try {
                const apiUrl = process.env.NEXT_PUBLIC_API_URL || "http://localhost:8000";
                const response = await fetch(`${apiUrl}/auth/me`, {
                    credentials: "include"
                });

                if (!response.ok) {
                    throw new Error("Session expirée");
                }

                const data = await response.json();
                setUser(data);
                setEditForm({
                    username: data.username || "",
                    location: data.location || "",
                    website: data.website || "",
                    avatar_url: data.avatar_url || "",
                    bio: data.bio || "",
                    genres: data.genres || []
                });
            } catch (err) {
                router.push("/login");
            } finally {
                setLoading(false);
            }
        };

        fetchUser();
    }, [router]);

    if (loading) {
        return (
            <div className="flex h-full min-h-[50vh] items-center justify-center">
                <div className="h-8 w-8 animate-spin rounded-full border-2 border-accent border-t-transparent" />
            </div>
        );
    }

    const handleLocationSearch = (query: string) => {
        setEditForm({ ...editForm, location: query });

        if (searchTimeoutRef.current) {
            clearTimeout(searchTimeoutRef.current);
        }

        if (query.length < 3) {
            setLocationSuggestions([]);
            return;
        }

        searchTimeoutRef.current = setTimeout(async () => {
            setIsSearchingLocation(true);
            try {
                // Nominatim restricts to 1 request/second.
                // We also avoid custom headers like User-Agent to prevent CORS preflight blocks in newer browsers.
                const response = await fetch(`https://nominatim.openstreetmap.org/search?format=json&addressdetails=1&q=${encodeURIComponent(query)}&limit=5`);
                const data = await response.json();
                setLocationSuggestions(data);
            } catch (error) {
                console.error("Error searching location:", error);
            } finally {
                setIsSearchingLocation(false);
            }
        }, 600); // 600ms debounce
    };

    const selectLocation = (suggestion: any) => {
        setEditForm({ ...editForm, location: suggestion.display_name });
        setLocationSuggestions([]);
    };

    const handleUpdateProfile = async (e: React.FormEvent) => {
        e.preventDefault();
        setIsSaving(true);
        try {
            const apiUrl = process.env.NEXT_PUBLIC_API_URL || "http://localhost:8000";
            const response = await fetch(`${apiUrl}/auth/me`, {
                method: "PUT",
                headers: {
                    "Content-Type": "application/json",
                },
                body: JSON.stringify(editForm),
                credentials: "include"
            });

            if (!response.ok) {
                const errorData = await response.json();
                throw new Error(errorData.detail || "Erreur lors de la mise à jour");
            }

            const updatedUser = await response.json();
            setUser(updatedUser);
            setIsEditModalOpen(false);
        } catch (err: any) {
            alert(err.message);
        } finally {
            setIsSaving(false);
        }
    };

    const handleSaveGenres = async () => {
        setIsSaving(true);
        try {
            const apiUrl = process.env.NEXT_PUBLIC_API_URL || "http://localhost:8000";
            const response = await fetch(`${apiUrl}/auth/me`, {
                method: "PUT",
                headers: {
                    "Content-Type": "application/json",
                },
                body: JSON.stringify({ genres: editForm.genres }),
                credentials: "include"
            });

            if (!response.ok) {
                const errorData = await response.json();
                throw new Error(errorData.detail || "Erreur lors de la mise à jour des styles de production");
            }

            const updatedUser = await response.json();
            setUser(updatedUser);
            setIsGenreModalOpen(false);
        } catch (err: any) {
            alert(err.message);
        } finally {
            setIsSaving(false);
        }
    };

    const { username, email, avatar_url, created_at, location, website, bio } = user || {};
    const joinedDate = created_at ? new Date(created_at).toLocaleDateString('en-US', { month: 'long', year: 'numeric' }) : 'Unknown Date';
    const initials = username ? username.substring(0, 2).toUpperCase() : '??';
    return (
        <div className="max-w-7xl mx-auto space-y-8 pb-12">
            {/* Header Section */}
            <motion.div
                initial={{ opacity: 0, y: 10 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.4 }}
                className="flex flex-col md:flex-row gap-8 items-start relative rounded-2xl border border-foreground/[0.08] bg-background-tertiary p-8 backdrop-blur-xl"
            >
                <div className="absolute right-8 top-8 z-10">
                    {!isEditModalOpen ? (
                        <button
                            onClick={() => setIsEditModalOpen(true)}
                            className="px-5 py-2.5 bg-accent hover:bg-accent/90 text-white text-sm font-medium rounded-xl transition-colors shadow-lg shadow-accent/20 flex items-center gap-2 relative"
                        >
                            <Edit2 size={16} /> Edit Profile
                        </button>
                    ) : (
                        <div className="flex gap-3">
                            <button
                                onClick={() => {
                                    setEditForm({
                                        username: user?.username || "",
                                        location: user?.location || "",
                                        website: user?.website || "",
                                        avatar_url: user?.avatar_url || "",
                                        bio: user?.bio || "",
                                        genres: user?.genres || []
                                    });
                                    setIsEditModalOpen(false);
                                }}
                                className="px-5 py-2.5 border border-white/10 hover:bg-white/5 text-white text-sm font-medium rounded-xl transition-colors"
                            >
                                Cancel
                            </button>
                            <button
                                onClick={handleUpdateProfile}
                                disabled={isSaving}
                                className="px-5 py-2.5 bg-accent hover:bg-accent/90 text-white text-sm font-medium rounded-xl transition-colors shadow-lg shadow-accent/20 flex items-center gap-2"
                            >
                                {isSaving ? <Loader2 size={16} className="animate-spin" /> : "Save"}
                            </button>
                        </div>
                    )}
                </div>

                <div className="relative group shrink-0">
                    <input
                        type="file"
                        ref={fileInputRef}
                        onChange={handleFileChange}
                        accept="image/*"
                        className="hidden"
                    />
                    {editForm.avatar_url || avatar_url ? (
                        <div className="relative">
                            <img
                                src={isEditModalOpen ? editForm.avatar_url || avatar_url : avatar_url}
                                alt={username}
                                className={`h-32 w-32 md:h-40 md:w-40 rounded-full object-cover border-4 border-background-secondary shadow-xl transition-all ${isEditModalOpen ? "cursor-pointer hover:brightness-75" : ""}`}
                                onClick={() => isEditModalOpen && fileInputRef.current?.click()}
                            />
                            {isEditModalOpen && (
                                <button
                                    onClick={() => fileInputRef.current?.click()}
                                    className="absolute bottom-1 right-1 bg-accent p-2 rounded-full text-white shadow-lg hover:bg-accent/90 transition-all border-2 border-background-secondary"
                                    title="Change picture"
                                >
                                    <Edit2 size={16} />
                                </button>
                            )}
                        </div>
                    ) : (
                        <div
                            className={`h-32 w-32 md:h-40 md:w-40 rounded-full bg-gradient-to-tr from-accent to-purple-400 flex items-center justify-center text-white text-5xl font-bold shadow-xl relative ${isEditModalOpen ? "cursor-pointer hover:brightness-90" : ""}`}
                            onClick={() => isEditModalOpen && fileInputRef.current?.click()}
                        >
                            {initials}
                            {isEditModalOpen && (
                                <button
                                    onClick={() => fileInputRef.current?.click()}
                                    className="absolute bottom-1 right-1 bg-accent p-2 rounded-full text-white shadow-lg hover:bg-accent/90 transition-all border-2 border-background-secondary"
                                    title="Change picture"
                                >
                                    <Edit2 size={16} />
                                </button>
                            )}
                        </div>
                    )}
                </div>

                <div className="space-y-4 pt-2 flex-1 relative">
                    <div>
                        {isEditModalOpen ? (
                            <input
                                type="text"
                                value={editForm.username}
                                onChange={(e) => setEditForm({ ...editForm, username: e.target.value })}
                                className="text-4xl font-medium tracking-tight mb-1 bg-transparent border-b border-accent/50 focus:border-accent focus:outline-none w-full max-w-sm text-foreground"
                                style={{ fontFamily: "var(--font-syne)" }}
                                placeholder="Pseudo"
                            />
                        ) : (
                            <h1 className="text-4xl font-medium tracking-tight mb-1" style={{ fontFamily: "var(--font-syne)" }}>{username || 'Producer'}</h1>
                        )}
                        <p className="text-foreground/60 text-base">{email}</p>
                    </div>

                    {isEditModalOpen ? (
                        <textarea
                            value={editForm.bio}
                            onChange={(e) => setEditForm({ ...editForm, bio: e.target.value })}
                            placeholder="Write a short summary about yourself..."
                            className="w-full bg-transparent border border-white/20 rounded-xl p-3 text-sm text-foreground focus:border-accent focus:outline-none placeholder:text-foreground/30 resize-none h-24 max-w-2xl"
                        />
                    ) : bio ? (
                        <p className="text-foreground/80 text-sm max-w-2xl leading-relaxed">
                            {bio}
                        </p>
                    ) : (
                        <p className="text-foreground/50 italic text-sm max-w-2xl">
                            No biography provided.
                        </p>
                    )}

                    <div className="flex flex-col gap-3 text-sm text-foreground/60 font-light mt-4">
                        {isEditModalOpen ? (
                            <div className="flex flex-col gap-3 w-full max-w-md">
                                <div className="flex items-center gap-2 relative">
                                    <MapPin size={16} className="text-accent" />
                                    <input
                                        type="text"
                                        value={editForm.location}
                                        onChange={(e) => handleLocationSearch(e.target.value)}
                                        className="bg-transparent border-b border-white/20 focus:border-accent focus:outline-none w-full text-foreground placeholder:text-foreground/30 py-1"
                                        placeholder="Location (e.g. Paris, France)"
                                    />
                                    {isSearchingLocation && <Loader2 size={14} className="animate-spin text-accent absolute right-2" />}
                                    {locationSuggestions.length > 0 && (
                                        <div className="absolute z-[60] left-6 top-8 w-[calc(100%-1.5rem)] bg-black border border-white/20 rounded-xl overflow-hidden shadow-2xl backdrop-blur-3xl">
                                            {locationSuggestions.map((suggestion: any) => (
                                                <button
                                                    key={suggestion.place_id}
                                                    type="button"
                                                    onClick={() => selectLocation(suggestion)}
                                                    className="w-full px-4 py-3 text-left hover:bg-white/10 text-sm text-white transition-colors border-b border-white/10 last:border-none"
                                                >
                                                    {suggestion.display_name}
                                                </button>
                                            ))}
                                        </div>
                                    )}
                                </div>
                                <div className="flex items-center gap-2">
                                    <LinkIcon size={16} className="text-accent" />
                                    <input
                                        type="text"
                                        value={editForm.website}
                                        onChange={(e) => setEditForm({ ...editForm, website: e.target.value })}
                                        className="bg-transparent border-b border-white/20 focus:border-accent focus:outline-none w-full text-foreground placeholder:text-foreground/30 py-1"
                                        placeholder="Website URL"
                                    />
                                </div>
                            </div>
                        ) : (
                            <div className="flex flex-wrap items-center gap-6">
                                {location && (
                                    <span className="flex items-center gap-1.5 hover:text-foreground transition-colors cursor-default">
                                        <MapPin size={16} className="text-accent" /> {location}
                                    </span>
                                )}
                                {website && (
                                    <span className="flex items-center gap-1.5">
                                        <LinkIcon size={16} className="text-accent" />
                                        <a href={website.startsWith('http') ? website : `https://${website}`} target="_blank" rel="noopener noreferrer" className="text-accent hover:underline">
                                            {website.replace(/^https?:\/\//, '')}
                                        </a>
                                    </span>
                                )}
                                <span className="flex items-center gap-1.5"><Calendar size={16} /> Joined {joinedDate}</span>
                            </div>
                        )}
                        {isEditModalOpen && (
                            <span className="flex items-center gap-1.5 mt-1"><Calendar size={16} /> Joined {joinedDate}</span>
                        )}
                    </div>

                    <div className="flex items-center gap-6 text-sm pt-2">
                        <div><span className="font-medium text-foreground text-base">0</span> <span className="text-foreground/60 font-light">followers</span></div>
                        <div><span className="font-medium text-foreground text-base">0</span> <span className="text-foreground/60 font-light">following</span></div>
                    </div>
                </div>
            </motion.div>

            {/* Production Styles */}
            <motion.div
                initial={{ opacity: 0, y: 10 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.4, delay: 0.1 }}
                className="rounded-2xl border border-foreground/[0.08] bg-background-tertiary p-6 backdrop-blur-xl"
            >
                <div className="flex items-center justify-between mb-5">
                    <div className="flex items-center gap-2">
                        <Music size={18} className="text-accent" />
                        <h2 className="text-base font-medium" style={{ fontFamily: "var(--font-syne)" }}>Production Styles</h2>
                    </div>
                    <button
                        onClick={() => setIsGenreModalOpen(true)}
                        className="flex items-center gap-2 text-xs text-foreground/60 hover:text-foreground border border-foreground/10 px-4 py-2 rounded-xl transition-colors hover:bg-foreground/5"
                    >
                        <Edit2 size={14} /> Edit
                    </button>
                </div>
                <div className="flex flex-wrap gap-3">
                    {user?.genres && user.genres.length > 0 ? (
                        user.genres.map((genre: string) => (
                            <span key={genre} className="px-4 py-2 rounded-xl border border-accent/20 bg-accent/10 text-accent text-sm font-medium capitalize">
                                {genre.replace(/-/g, ' ')}
                            </span>
                        ))
                    ) : (
                        <p className="text-sm text-foreground/40 italic">No production styles selected.</p>
                    )}
                </div>
            </motion.div>

            {/* Genre Selection Modal */}
            {isGenreModalOpen && (
                <div className="fixed inset-0 z-[100] flex items-center justify-center p-4">
                    <motion.div
                        initial={{ opacity: 0 }}
                        animate={{ opacity: 1 }}
                        exit={{ opacity: 0 }}
                        onClick={() => setIsGenreModalOpen(false)}
                        className="absolute inset-0 bg-black/60 backdrop-blur-sm"
                    />
                    <motion.div
                        initial={{ opacity: 0, scale: 0.95, y: 20 }}
                        animate={{ opacity: 1, scale: 1, y: 0 }}
                        className="relative w-full max-w-4xl max-h-[80vh] bg-background-secondary border border-white/10 rounded-2xl shadow-2xl overflow-hidden flex flex-col"
                    >
                        <div className="p-6 border-b border-white/10 flex items-center justify-between">
                            <div>
                                <h2 className="text-xl font-medium" style={{ fontFamily: "var(--font-syne)" }}>Select Production Styles</h2>
                                <p className="text-sm text-foreground/50 mt-1">Choose the genres that best describe your music.</p>
                            </div>
                            <button
                                onClick={() => setIsGenreModalOpen(false)}
                                className="p-2 hover:bg-white/5 rounded-full transition-colors"
                            >
                                <X size={20} />
                            </button>
                        </div>

                        <div className="px-6 pt-6 pb-2 border-b border-white/10 relative">
                            <Search size={16} className="absolute left-10 top-1/2 -translate-y-1/2 text-foreground/40 mt-2" />
                            <input
                                type="text"
                                placeholder="Search styles..."
                                value={genreSearch}
                                onChange={(e) => setGenreSearch(e.target.value)}
                                className="w-full bg-background/50 border border-white/10 rounded-xl py-3 pl-11 pr-4 text-sm text-foreground focus:outline-none focus:border-accent transition-colors"
                            />
                        </div>

                        <div className="flex-1 overflow-y-auto p-6 space-y-8 custom-scrollbar">
                            {GENRE_GROUPS.map((group) => {
                                const filteredGenres = group.genres.filter(g =>
                                    g.replace(/-/g, ' ').toLowerCase().includes(genreSearch.toLowerCase())
                                );

                                if (filteredGenres.length === 0) return null;

                                return (
                                    <div key={group.label} className="space-y-4">
                                        <h3 className="text-xs font-bold uppercase tracking-wider text-accent border-l-2 border-accent pl-3">
                                            {group.label}
                                        </h3>
                                        <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5 gap-2">
                                            {filteredGenres.map((genre) => {
                                                const isSelected = editForm.genres.includes(genre);
                                                return (
                                                    <button
                                                        key={genre}
                                                        onClick={() => {
                                                            const newGenres = isSelected
                                                                ? editForm.genres.filter(g => g !== genre)
                                                                : [...editForm.genres, genre];
                                                            setEditForm({ ...editForm, genres: newGenres });
                                                        }}
                                                        className={`px-3 py-2 rounded-lg text-xs font-medium transition-all flex items-center justify-between border ${isSelected
                                                            ? "bg-accent border-accent text-white"
                                                            : "bg-white/5 border-white/10 text-foreground/60 hover:border-white/20 hover:text-foreground"
                                                            }`}
                                                    >
                                                        <span className="capitalize text-left">{genre.replace(/-/g, ' ')}</span>
                                                        {isSelected && <Check size={12} className="shrink-0 ml-1" />}
                                                    </button>
                                                );
                                            })}
                                        </div>
                                    </div>
                                );
                            })}

                            {GENRE_GROUPS.every(group =>
                                group.genres.filter(g => g.replace(/-/g, ' ').toLowerCase().includes(genreSearch.toLowerCase())).length === 0
                            ) && (
                                    <div className="text-center py-10 text-foreground/40 text-sm italic">
                                        No genres found matching "{genreSearch}"
                                    </div>
                                )}
                        </div>

                        <div className="p-6 border-t border-white/10 bg-black/20 flex justify-end gap-3">
                            <button
                                onClick={() => {
                                    setEditForm({ ...editForm, genres: user?.genres || [] });
                                    setEditForm({ ...editForm, genres: user?.genres || [] });
                                    setIsGenreModalOpen(false);
                                }}
                                className="px-5 py-2 text-sm font-medium hover:bg-white/5 rounded-xl transition-colors disabled:opacity-50"
                                disabled={isSaving}
                            >
                                Cancel
                            </button>
                            <button
                                onClick={handleSaveGenres}
                                disabled={isSaving}
                                className="px-6 py-2 bg-accent text-white text-sm font-medium rounded-xl hover:bg-accent/90 transition-colors shadow-lg shadow-accent/20 flex items-center gap-2"
                            >
                                {isSaving ? <Loader2 size={16} className="animate-spin" /> : "Confirm Selection"}
                            </button>
                        </div>
                    </motion.div>
                </div >
            )
            }

            {/* Grid for Featured Projects & Activity */}
            <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">

                {/* Featured Projects */}
                <motion.div
                    initial={{ opacity: 0, y: 10 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.4, delay: 0.2 }}
                    className="lg:col-span-2 space-y-5"
                >
                    <div className="flex items-center gap-2 mb-2">
                        <Heart size={18} className="text-accent" />
                        <h2 className="text-base font-medium" style={{ fontFamily: "var(--font-syne)" }}>Featured Projects <span className="text-foreground/40 font-light text-sm tracking-normal">• Pinned</span></h2>
                    </div>

                    <div className="space-y-4">
                        {/* Project Card 1 */}
                        <div
                            className="rounded-2xl border border-foreground/[0.08] bg-background-tertiary p-6 hover:bg-background-secondary/30 transition-all cursor-pointer backdrop-blur-xl group"
                            onClick={() => router.push("/projects")}
                        >
                            <div className="flex items-start justify-between mb-4">
                                <div>
                                    <h3 className="text-lg font-medium flex items-center gap-2 group-hover:text-accent transition-colors">
                                        <div className="w-2 h-2 rounded-full bg-accent"></div>
                                        <span>Dubstep-Track</span>
                                    </h3>
                                    <p className="text-sm text-foreground/60 mt-1 font-light">Experimental dubstep track</p>
                                </div>
                                <span className="text-xs text-foreground/40 font-light">2h ago</span>
                            </div>

                            <div className="flex items-center gap-3 mb-5">
                                <span className="px-2.5 py-1 rounded-md bg-foreground/5 border border-foreground/10 text-xs font-medium">140 BPM</span>
                                <span className="px-2.5 py-1 rounded-md bg-foreground/5 border border-foreground/10 text-xs font-medium">C minor</span>
                                <span className="flex items-center gap-1.5 text-xs text-foreground/60 ml-2 font-light"><Users size={14} /> 3 contributors</span>
                            </div>

                            <div className="flex items-center gap-5 text-sm text-foreground/50 font-light">
                                <button
                                    type="button"
                                    onClick={(e) => {
                                        e.stopPropagation();
                                        setFeaturedFavorites((prev) => ({
                                            ...prev,
                                            dubstep: !prev.dubstep
                                        }));
                                    }}
                                    className="flex items-center gap-1.5 hover:text-accent transition-colors"
                                >
                                    <Heart
                                        size={16}
                                        className={featuredFavorites.dubstep ? "text-red-500 fill-red-500" : "text-foreground/70"}
                                    />
                                    <span>12</span>
                                </button>
                                <span className="flex items-center gap-1.5 hover:text-blue-400 transition-colors">
                                    <GitBranch size={16} /> 4 versions
                                </span>
                            </div>
                        </div>

                        {/* Project Card 2 */}
                        <div className="rounded-2xl border border-foreground/[0.08] bg-background-tertiary p-6 hover:bg-background-secondary/30 transition-all cursor-pointer backdrop-blur-xl group">
                            <div className="flex items-start justify-between mb-4">
                                <div>
                                    <h3 className="text-lg font-medium flex items-center gap-2 group-hover:text-accent transition-colors">
                                        <div className="w-2 h-2 rounded-full bg-accent"></div>
                                        <span>Melodic-House-EP</span>
                                    </h3>
                                    <p className="text-sm text-foreground/60 mt-1 font-light">Deep melodic house collection</p>
                                </div>
                                <span className="text-xs text-foreground/40 font-light">1d ago</span>
                            </div>

                            <div className="flex items-center gap-3 mb-5">
                                <span className="px-2.5 py-1 rounded-md bg-foreground/5 border border-foreground/10 text-xs font-medium">124 BPM</span>
                                <span className="px-2.5 py-1 rounded-md bg-foreground/5 border border-foreground/10 text-xs font-medium">A minor</span>
                                <span className="flex items-center gap-1.5 text-xs text-foreground/60 ml-2 font-light"><Users size={14} /> 2 contributors</span>
                            </div>

                            <div className="flex items-center gap-5 text-sm text-foreground/50 font-light">
                                <button
                                    type="button"
                                    onClick={(e) => {
                                        e.stopPropagation();
                                        setFeaturedFavorites((prev) => ({
                                            ...prev,
                                            melodic: !prev.melodic
                                        }));
                                    }}
                                    className="flex items-center gap-1.5 hover:text-accent transition-colors"
                                >
                                    <Heart
                                        size={16}
                                        className={featuredFavorites.melodic ? "text-red-500 fill-red-500" : "text-foreground/70"}
                                    />
                                    <span>9</span>
                                </button>
                                <span className="flex items-center gap-1.5 hover:text-blue-400 transition-colors">
                                    <GitBranch size={16} /> 5 versions
                                </span>
                            </div>
                        </div>

                        {/* Project Card 3 */}
                        <div className="rounded-2xl border border-foreground/[0.08] bg-background-tertiary p-6 hover:bg-background-secondary/30 transition-all cursor-pointer backdrop-blur-xl group">
                            <div className="flex items-start justify-between mb-4">
                                <div>
                                    <h3 className="text-lg font-medium flex items-center gap-2 group-hover:text-accent transition-colors">
                                        <div className="w-2 h-2 rounded-full bg-accent"></div>
                                        <span>Bass-Experiments</span>
                                    </h3>
                                    <p className="text-sm text-foreground/60 mt-1 font-light">Sub bass design toolkit</p>
                                </div>
                                <span className="text-xs text-foreground/40 font-light">3d ago</span>
                            </div>

                            <div className="flex items-center gap-3 mb-5">
                                <span className="px-2.5 py-1 rounded-md bg-foreground/5 border border-foreground/10 text-xs font-medium">140 BPM</span>
                                <span className="px-2.5 py-1 rounded-md bg-foreground/5 border border-foreground/10 text-xs font-medium">E minor</span>
                                <span className="flex items-center gap-1.5 text-xs text-foreground/60 ml-2 font-light"><Users size={14} /> 1 contributor</span>
                            </div>

                            <div className="flex items-center gap-5 text-sm text-foreground/50 font-light">
                                <button
                                    type="button"
                                    onClick={(e) => {
                                        e.stopPropagation();
                                        setFeaturedFavorites((prev) => ({
                                            ...prev,
                                            bass: !prev.bass
                                        }));
                                    }}
                                    className="flex items-center gap-1.5 hover:text-accent transition-colors"
                                >
                                    <Heart
                                        size={16}
                                        className={featuredFavorites.bass ? "text-red-500 fill-red-500" : "text-foreground/70"}
                                    />
                                    <span>6</span>
                                </button>
                                <span className="flex items-center gap-1.5 hover:text-blue-400 transition-colors">
                                    <GitBranch size={16} /> 3 versions
                                </span>
                            </div>
                        </div>
                    </div>
                </motion.div>

                {/* Activity and Stats */}
                <motion.div
                    initial={{ opacity: 0, y: 10 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.4, delay: 0.3 }}
                    className="space-y-6"
                >
                    <div className="flex items-center gap-2 mb-2">
                        <Headphones size={18} className="text-foreground/80" />
                        <h2 className="text-base font-medium" style={{ fontFamily: "var(--font-syne)" }}>Activity</h2>
                    </div>

                    <div className="rounded-2xl border border-foreground/[0.08] bg-background-tertiary p-6 backdrop-blur-xl">
                        <div className="flex flex-col gap-1 mb-6">
                            <span className="text-xs text-foreground/50 font-light">Last year</span>
                            <span className="text-3xl font-medium tracking-tight">696 <span className="text-sm font-light text-foreground/50 tracking-normal">contributions</span></span>
                        </div>

                        {/* Contribution Graph Mock */}
                        <div className="mt-2">
                            <div className="grid grid-cols-[repeat(26,minmax(0,1fr))] gap-1 opacity-80">
                                {[...Array(26 * 7)].map((_, i) => (
                                    <div
                                        key={i}
                                        className={`w-full aspect-square rounded-[2px] ${Math.random() > 0.7
                                            ? Math.random() > 0.5 ? 'bg-accent' : 'bg-accent/60'
                                            : Math.random() > 0.8 ? 'bg-accent/40' : 'bg-foreground/5'
                                            }`}
                                    />
                                ))}
                            </div>
                            <div className="flex items-center justify-between text-[10px] text-foreground/40 mt-4 pt-3 border-t border-foreground/5">
                                <span>Less</span>
                                <div className="flex gap-1.5">
                                    <div className="w-2.5 h-2.5 rounded-[2px] bg-foreground/5"></div>
                                    <div className="w-2.5 h-2.5 rounded-[2px] bg-accent/40"></div>
                                    <div className="w-2.5 h-2.5 rounded-[2px] bg-accent/60"></div>
                                    <div className="w-2.5 h-2.5 rounded-[2px] bg-accent"></div>
                                    <div className="w-2.5 h-2.5 rounded-[2px] bg-[#fff] border border-accent/20"></div>
                                </div>
                                <span>More</span>
                            </div>
                        </div>
                    </div>

                    <div className="grid grid-cols-2 gap-4">
                        <div className="flex flex-col rounded-2xl border border-foreground/[0.08] bg-background-tertiary p-5 backdrop-blur-xl hover:bg-background-secondary/30 transition-colors cursor-pointer">
                            <span className="text-xs text-foreground/50 font-light">Projects</span>
                            <p className="text-2xl font-medium mt-1 tracking-tight">24</p>
                        </div>
                        <div className="flex flex-col rounded-2xl border border-foreground/[0.08] bg-background-tertiary p-5 backdrop-blur-xl hover:bg-background-secondary/30 transition-colors cursor-pointer">
                            <span className="text-xs text-foreground/50 font-light">Collaborations</span>
                            <p className="text-2xl font-medium mt-1 tracking-tight">18</p>
                        </div>
                    </div>
                </motion.div>

            </div>

        </div >
    );
}
