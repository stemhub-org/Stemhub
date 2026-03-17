"use client";

import { useState, useEffect } from "react";
import { motion } from "framer-motion";
import { authFetch } from "@/lib/api";
import {
    Play,
    Star,
    GitFork,
    TrendingUp,
    Clock,
    Activity,
    Users,
    Sparkles,
    Zap,
    Box,
    Globe,
    Compass
} from "lucide-react";

interface ExploreProject {
    id: string;
    name: string;
    description: string | null;
    category: string | null;
    tags: string[];
    like_count: number;
    bpm: number | null;
    key: string | null;
    created_at: string;
    owner: {
        id: string;
        username: string;
    };
}

interface ExploreFeedItem {
    id: string;
    action_type: string;
    project_id: string;
    project_name: string;
    producer: {
        id: string;
        username: string;
    };
    created_at: string;
}

interface Producer {
    id: string;
    username: string;
    avatar_url: string | null;
    bio: string | null;
    follower_count: number;
    genres: string[];
}

interface Challenge {
    id: string;
    title: string;
    description: string;
    level: string;
    prize: string;
    ends_at: string;
    created_at: string;
}

interface EventItem {
    id: string;
    type: string;
    title: string;
    host_name: string;
    event_date: string;
    created_at: string;
}

export default function ExplorePage() {
    const [activeTab, setActiveTab] = useState('feed');
    const [feed, setFeed] = useState<ExploreFeedItem[]>([]);
    const [producers, setProducers] = useState<Producer[]>([]);
    const [trendingProjects, setTrendingProjects] = useState<ExploreProject[]>([]);
    const [challenges, setChallenges] = useState<Challenge[]>([]);
    const [events, setEvents] = useState<EventItem[]>([]);
    const [changelog, setChangelog] = useState<any[]>([]);
    const [isLoading, setIsLoading] = useState(true);
    const [isJoining, setIsJoining] = useState<Record<string, boolean>>({});
    const [isRegistering, setIsRegistering] = useState<Record<string, boolean>>({});

    useEffect(() => {
        const fetchExploreData = async () => {
            setIsLoading(true);
            try {
                const [feedRes, producersRes, projectsRes, challengesRes, eventsRes, changelogRes] = await Promise.all([
                    authFetch<ExploreFeedItem[]>("/explore/feed"),
                    authFetch<Producer[]>("/explore/producers"),
                    authFetch<ExploreProject[]>("/explore/projects?sort_by=trending"),
                    authFetch<Challenge[]>("/community/challenges?status=active"),
                    authFetch<EventItem[]>("/community/events"),
                    authFetch<any[]>("/explore/changelog").catch(() => [])
                ]);

                setFeed(Array.isArray(feedRes) ? feedRes : []);
                setProducers(Array.isArray(producersRes) ? producersRes : []);
                setTrendingProjects(Array.isArray(projectsRes) ? projectsRes : []);
                setChallenges(Array.isArray(challengesRes) ? challengesRes : []);
                setEvents(Array.isArray(eventsRes) ? eventsRes : []);
                setChangelog(Array.isArray(changelogRes) ? changelogRes : []);
            } catch (err) {
                console.error("Failed to fetch explore data:", err);
            } finally {
                setIsLoading(false);
            }
        };

        fetchExploreData();
    }, []);

    const handleJoinChallenge = async (challengeId: string) => {
        setIsJoining(prev => ({ ...prev, [challengeId]: true }));
        try {
            await authFetch(`/community/challenges/${challengeId}/join`, { method: "POST" });
            alert("Successfully joined challenge!");
        } catch (err: any) {
            alert(err.message || "Failed to join challenge");
        } finally {
            setIsJoining(prev => ({ ...prev, [challengeId]: false }));
        }
    };

    const handleRegisterEvent = async (eventId: string) => {
        setIsRegistering(prev => ({ ...prev, [eventId]: true }));
        try {
            await authFetch(`/community/events/${eventId}/register`, { method: "POST" });
            alert("Successfully registered for event!");
        } catch (err: any) {
            alert(err.message || "Failed to register for event");
        } finally {
            setIsRegistering(prev => ({ ...prev, [eventId]: false }));
        }
    };

    const formatTimeAgo = (dateString: string) => {
        const date = new Date(dateString);
        const now = new Date();
        const diffInMs = now.getTime() - date.getTime();
        const diffInMins = Math.floor(diffInMs / (1000 * 60));
        const diffInHrs = Math.floor(diffInMins / 60);
        const diffInDays = Math.floor(diffInHrs / 24);

        if (diffInMins < 60) return `${diffInMins}m ago`;
        if (diffInHrs < 24) return `${diffInHrs}h ago`;
        return `${diffInDays}d ago`;
    };

    const renderActivityFeed = () => (
        <div className="flex-1 space-y-8">
            <div className="flex items-center gap-3 mb-6">
                <div className="p-2 bg-accent/10 text-accent rounded-lg shadow-inner">
                    <Activity size={24} />
                </div>
                <div>
                    <h2 className="text-2xl font-bold tracking-tight text-foreground">Activity Feed</h2>
                    <p className="text-sm text-foreground-muted">Recent updates from people you follow</p>
                </div>
            </div>

            {feed.length === 0 && !isLoading && (
                <div className="text-center p-8 text-foreground-muted bg-background-secondary rounded-2xl border border-border-subtle">
                    No recent activity from producers you follow.
                </div>
            )}
            
            {feed.map((item, idx) => (
                <motion.div
                    key={item.id}
                    initial={{ opacity: 0, y: 15 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.5, delay: idx * 0.1 }}
                    className="bg-background-secondary dark:bg-background-tertiary border border-border-subtle rounded-2xl p-6 md:p-8 hover:border-accent/40 hover:shadow-[0_8px_30px_rgb(0,0,0,0.12)] transition-all duration-300 group"
                >
                    <div className="flex items-center justify-between mb-6">
                        <div className="flex items-center gap-4">
                            <div className="h-12 w-12 rounded-full bg-gradient-to-br from-[#9C57DF] to-[#C28CF0] flex items-center justify-center text-white font-bold shadow-lg ring-4 ring-background">
                                {item.producer.username.substring(0, 2).toUpperCase()}
                            </div>
                            <div>
                                <div className="flex items-center gap-2 mb-1">
                                    <span className="font-bold text-foreground text-lg group-hover:text-accent transition-colors">{item.producer.username}</span>
                                    <Sparkles size={16} className="text-accent" />
                                </div>
                                <span className="text-foreground-muted text-sm">created a {item.action_type.replace('_', ' ')}</span>
                            </div>
                        </div>
                        <span className="text-xs font-semibold px-3 py-1 bg-background rounded-full text-foreground-muted flex items-center gap-1.5 border border-border-subtle shadow-sm">
                            <Clock size={12} /> {formatTimeAgo(item.created_at)}
                        </span>
                    </div>

                    <div className="bg-background rounded-xl border border-border-subtle p-6 relative overflow-hidden">
                        <div className="absolute top-0 right-0 w-32 h-32 bg-accent/5 rounded-full blur-3xl -mr-10 -mt-10 pointer-events-none"></div>
                        <div className="flex justify-between items-start mb-4 relative z-10">
                            <div>
                                <h3 className="text-xl font-bold text-foreground hover:text-accent cursor-pointer transition-colors mb-2">{item.project_name}</h3>
                                <p className="text-sm text-foreground-muted leading-relaxed max-w-lg">Check out this new project by {item.producer.username}.</p>
                            </div>
                            <button className="bg-accent/10 hover:bg-accent text-accent hover:text-white px-5 py-2.5 rounded-xl text-sm font-semibold transition-all duration-300 shadow-[0_0_15px_rgba(156,87,223,0.15)] hover:shadow-[0_0_25px_rgba(156,87,223,0.4)] hover:-translate-y-0.5 flex items-center gap-2">
                                <Play size={16} fill="currentColor" /> Listen
                            </button>
                        </div>
                    </div>
                </motion.div>
            ))}
        </div>
    );

    const renderCardContainer = (Icon: any, title: string, subtitle: string, colorClass: string, children: React.ReactNode) => (
        <div className="bg-background-secondary dark:bg-background-tertiary border border-border-subtle rounded-2xl p-6 hover:shadow-lg transition-shadow duration-300 relative overflow-hidden group">
            <div className={`absolute top-0 right-0 w-24 h-24 rounded-full blur-3xl -mr-8 -mt-8 pointer-events-none opacity-20 group-hover:opacity-40 transition-opacity ${colorClass.includes('accent') ? 'bg-accent' : colorClass.includes('amber') ? 'bg-amber-500' : 'bg-blue-500'}`}></div>
            <div className="flex items-center gap-3 mb-6 relative z-10">
                <div className={`p-2 rounded-lg bg-background shadow-sm ${colorClass}`}>
                    <Icon size={20} />
                </div>
                <div>
                    <h3 className="font-bold text-foreground text-lg">{title}</h3>
                    <p className="text-xs text-foreground-muted">{subtitle}</p>
                </div>
            </div>
            <div className="relative z-10">
                {children}
            </div>
        </div>
    );

    const renderTrending = () => renderCardContainer(
        TrendingUp, "Trending Producers", "Fastest growing this week", "text-accent",
        <div className="space-y-2">
            {producers.length === 0 && <p className="text-sm text-foreground-muted text-center py-4">No producers found.</p>}
            {producers.slice(0, 4).map((producer, idx) => (
                <div key={producer.id} className="flex items-center justify-between p-3 rounded-xl bg-background border border-border-subtle hover:border-accent/40 hover:shadow-md transition-all cursor-pointer group/item">
                    <div className="flex items-center gap-4">
                        <span className="text-xs font-bold text-foreground-muted w-3">{idx + 1}</span>
                        {producer.avatar_url ? (
                            <img src={producer.avatar_url} alt={producer.username} className="h-10 w-10 rounded-full object-cover group-hover/item:scale-110 transition-transform" />
                        ) : (
                            <div className={`h-10 w-10 rounded-full bg-gradient-to-br from-pink-500 to-rose-400 flex items-center justify-center text-white text-xs font-bold shadow-sm group-hover/item:scale-110 group-hover/item:rotate-3 transition-transform`}>
                                {producer.username.substring(0, 2).toUpperCase()}
                            </div>
                        )}
                        <div>
                            <div className="font-bold text-foreground group-hover/item:text-accent transition-colors">{producer.username}</div>
                            <div className="text-xs text-foreground-muted">{producer.follower_count} followers</div>
                        </div>
                    </div>
                </div>
            ))}
        </div>
    );

    const renderHotThisWeek = () => renderCardContainer(
        Star, "Hot This Week", "Most starred projects", "text-amber-500",
        <div className="space-y-2">
            {trendingProjects.length === 0 && <p className="text-sm text-foreground-muted text-center py-4">No trending projects.</p>}
            {trendingProjects.slice(0, 4).map((project, idx) => (
                <div key={project.id} className="flex items-center justify-between p-3 rounded-xl bg-background border border-border-subtle hover:border-amber-500/40 hover:shadow-md transition-all cursor-pointer group/item">
                    <div className="flex items-center gap-3">
                        <div className={`w-2 h-2 rounded-full bg-amber-500 shadow-[0_0_8px_currentColor]`} />
                        <div>
                            <div className="font-bold text-foreground group-hover/item:text-amber-500 transition-colors truncate max-w-[140px]">{project.name}</div>
                            <div className="text-xs text-foreground-muted mt-0.5">{project.category}</div>
                        </div>
                    </div>
                    <div className="flex items-center gap-1.5 text-xs font-bold text-foreground-muted bg-foreground/5 px-2 py-1 rounded-md">
                        <Star size={12} className="text-amber-500" fill="currentColor" /> {project.like_count}
                    </div>
                </div>
            ))}
        </div>
    );

    const renderSuggestedCollaborators = () => renderCardContainer(
        Users, "Suggested Collabs", "Based on your style", "text-blue-500",
        <div className="space-y-3">
            {producers.length === 0 && <p className="text-sm text-foreground-muted text-center py-4">No suggestions available.</p>}
            {producers.slice(4, 7).map((collab, idx) => (
                <div key={collab.id} className="bg-background border border-border-subtle rounded-xl p-4 hover:border-blue-500/40 hover:shadow-md transition-all group/item">
                    <div className="flex items-center gap-3 mb-3 relative">
                        {collab.avatar_url ? (
                            <img src={collab.avatar_url} alt={collab.username} className="h-12 w-12 rounded-full object-cover group-hover/item:shadow-lg transition-shadow" />
                        ) : (
                            <div className={`h-12 w-12 rounded-full bg-gradient-to-br from-cyan-400 to-blue-500 flex items-center justify-center text-white text-sm font-bold shadow-md group-hover/item:shadow-lg transition-shadow`}>
                                {collab.username.substring(0, 2).toUpperCase()}
                            </div>
                        )}
                        <div>
                            <h4 className="font-bold text-foreground text-sm group-hover/item:text-blue-500 transition-colors">{collab.username}</h4>
                            <p className="text-[10px] text-foreground-muted mt-0.5">{collab.follower_count} followers</p>
                        </div>
                    </div>
                    <div className="flex gap-1.5 mb-4 flex-wrap">
                        {(collab.genres || []).slice(0, 3).map(g => (
                            <span key={g} className="px-2 py-1 rounded-md bg-foreground/5 text-foreground-muted border border-border-subtle text-[10px] font-medium">{g}</span>
                        ))}
                    </div>
                    <button className="w-full py-2 rounded-lg bg-blue-500/10 border border-blue-500/20 text-blue-500 font-bold text-xs hover:bg-blue-500 hover:text-white transition-all shadow-[0_0_10px_rgba(59,130,246,0.1)] hover:shadow-[0_4px_15px_rgba(59,130,246,0.3)]">
                        Connect
                    </button>
                </div>
            ))}
        </div>
    );

    const renderPlatformUpdates = () => (
        <div className="bg-gradient-to-b from-background-secondary to-background-tertiary border border-border-subtle rounded-2xl p-6 shadow-sm">
            <div className="flex items-center gap-3 mb-6">
                <div className="p-2 bg-accent/10 text-accent rounded-lg">
                    <Zap size={20} />
                </div>
                <div>
                    <h3 className="font-bold text-foreground text-lg">Changelog</h3>
                    <p className="text-xs text-foreground-muted">Platform updates</p>
                </div>
            </div>
            <div className="space-y-4">
                {changelog.length === 0 && <p className="text-sm text-foreground-muted text-center py-4">No recent updates.</p>}
                {changelog.slice(0, 4).map((update, idx) => {
                    return (
                        <div key={update.id} className="relative pl-6 pb-4 last:pb-0 border-l border-border-subtle group">
                            <div className="absolute left-[-17px] top-1 h-8 w-8 rounded-full bg-background border-4 border-background-secondary dark:border-background-tertiary flex items-center justify-center group-hover:bg-accent/10 group-hover:border-accent/20 transition-colors">
                                <Zap size={12} className="text-foreground-muted group-hover:text-accent transition-colors" />
                            </div>
                            <div className="bg-background border border-border-subtle rounded-xl p-4 hover:border-accent/30 hover:shadow-md transition-all ml-4">
                                <div className="flex items-center gap-2 mb-2">
                                    <span className="text-[10px] font-bold px-2 py-0.5 rounded-md bg-accent/10 text-accent border border-accent/20">{update.version_string}</span>
                                    <span className="text-[10px] text-foreground-muted/70 font-medium">{formatTimeAgo(update.created_at)}</span>
                                </div>
                                <h4 className="text-sm font-bold text-foreground group-hover:text-accent transition-colors mb-1.5">{update.title}</h4>
                                <p className="text-xs text-foreground-muted leading-relaxed">{update.description}</p>
                            </div>
                        </div>
                    );
                })}
            </div>
        </div>
    );

    const renderActiveChallenges = () => renderCardContainer(
        TrendingUp, "Active Challenges", "Join to win prizes", "text-accent",
        <div className="space-y-4">
            {challenges.length === 0 && <p className="text-sm text-foreground-muted text-center py-4">No active challenges right now.</p>}
            {challenges.map((challenge, idx) => {
                const endsAtDate = new Date(challenge.ends_at);
                const daysLeft = Math.max(0, Math.ceil((endsAtDate.getTime() - new Date().getTime()) / (1000 * 3600 * 24)));
                
                return (
                    <div key={challenge.id} className="bg-background border border-border-subtle rounded-xl p-5 hover:border-accent/40 hover:shadow-md transition-all group/item">
                        <div className="flex justify-between items-start mb-2">
                            <h4 className="font-bold text-foreground text-base group-hover/item:text-accent transition-colors">{challenge.title}</h4>
                            <span className="px-2 py-1 rounded-md bg-accent/10 text-accent border border-accent/20 text-[10px] font-extrabold uppercase tracking-widest">{challenge.level}</span>
                        </div>
                        <p className="text-xs text-foreground-muted mb-4">{challenge.description}</p>
                        
                        <div className="grid grid-cols-2 gap-3 mb-4">
                            <div className="bg-foreground/5 rounded-lg p-2 text-center">
                                <div className="text-[10px] uppercase text-foreground-muted font-bold tracking-wider mb-1">Participants</div>
                                <div className="font-bold text-foreground text-xs flex items-center justify-center gap-1.5"><Users size={12} /> --</div>
                            </div>
                            <div className="bg-foreground/5 rounded-lg p-2 text-center">
                                <div className="text-[10px] uppercase text-foreground-muted font-bold tracking-wider mb-1">Time Left</div>
                                <div className="font-bold text-amber-500 text-xs flex items-center justify-center gap-1.5"><Clock size={12} /> {daysLeft} days</div>
                            </div>
                        </div>

                        <div className="flex items-center justify-between mt-2 pt-4 border-t border-border-subtle">
                            <div className="text-xs">
                                <span className="text-foreground-muted">Prize: </span>
                                <span className="font-bold text-accent">{challenge.prize}</span>
                            </div>
                            <button 
                                onClick={() => handleJoinChallenge(challenge.id)}
                                disabled={isJoining[challenge.id]}
                                className="px-4 py-2 rounded-lg bg-accent text-white font-bold text-xs hover:bg-accent/90 disabled:opacity-50 disabled:cursor-not-allowed transition-all shadow-[0_4px_15px_rgba(156,87,223,0.3)] hover:-translate-y-0.5"
                            >
                                {isJoining[challenge.id] ? "Joining..." : "Join Now"}
                            </button>
                        </div>
                    </div>
                );
            })}
        </div>
    );

    const renderUpcomingEvents = () => renderCardContainer(
        Clock, "Upcoming Events", "Workshops & Streams", "text-blue-500",
        <div className="space-y-4">
            {events.length === 0 && <p className="text-sm text-foreground-muted text-center py-4">No upcoming events right now.</p>}
            {events.map((event, idx) => {
                const eventDate = new Date(event.event_date);
                const formattedDate = eventDate.toLocaleDateString('en-US', { month: 'short', day: 'numeric', year: 'numeric' });
                const formattedTime = eventDate.toLocaleTimeString('en-US', { hour: 'numeric', minute: '2-digit' });
                
                return (
                    <div key={event.id} className="bg-background border border-border-subtle rounded-xl p-5 hover:border-blue-500/40 hover:shadow-md transition-all group/item">
                        <div className="flex items-center gap-2 mb-3">
                            <span className={`w-2 h-2 rounded-full bg-blue-500 shadow-[0_0_8px_currentColor]`}></span>
                            <span className="text-[10px] font-extrabold uppercase tracking-widest text-foreground-muted">{event.type || "Event"}</span>
                        </div>
                        <h4 className="font-bold text-foreground text-base group-hover/item:text-blue-500 transition-colors mb-1">{event.title}</h4>
                        <p className="text-xs text-foreground-muted mb-4 flex items-center gap-1.5"><span className="text-foreground">Host:</span> {event.host_name}</p>
                        
                        <div className="bg-blue-500/5 rounded-lg border border-blue-500/10 p-3 mb-4 space-y-2">
                            <p className="text-xs font-medium text-foreground-muted flex items-center gap-2"><Clock size={14} className="text-blue-500" /> {formattedDate} • {formattedTime}</p>
                            <p className="text-xs font-medium text-foreground-muted flex items-center gap-2"><Users size={14} className="text-blue-500" /> -- attending</p>
                        </div>
                        <button 
                            onClick={() => handleRegisterEvent(event.id)}
                            disabled={isRegistering[event.id]}
                            className="w-full py-2.5 rounded-lg bg-transparent border-2 border-border-subtle text-foreground font-bold text-xs hover:border-blue-500 disabled:opacity-50 disabled:cursor-not-allowed hover:text-blue-500 transition-all hover:-translate-y-0.5 shadow-sm"
                        >
                            {isRegistering[event.id] ? "Registering..." : "Register for Event"}
                        </button>
                    </div>
                );
            })}
        </div>
    );

    return (
        <div className="min-h-full bg-background flex flex-col">
            <div className="w-full mx-auto space-y-6 pb-16 pt-10 px-4 md:px-8 xl:px-12 max-w-7xl">
                
                {/* Header & Animated Tabs */}
                <div className="flex flex-col items-center justify-center mb-12">
                    <motion.h1 
                        initial={{ opacity: 0, scale: 0.95 }}
                        animate={{ opacity: 1, scale: 1 }}
                        className="text-4xl font-extrabold text-foreground mb-10 flex items-center gap-4 tracking-tight"
                    >
                        <div className="p-3 bg-gradient-to-br from-accent/20 to-accent/5 rounded-2xl border border-accent/20 shadow-inner">
                            <Globe className="text-accent h-8 w-8" />
                        </div>
                        Explore
                    </motion.h1>
                    
                    <div className="p-1.5 bg-background-secondary/80 dark:bg-background-tertiary/80 backdrop-blur-md rounded-full border border-border-subtle shadow-lg relative w-full max-w-[600px] mx-auto overflow-hidden">
                        <div className="flex relative z-10">
                            {[
                                { id: 'feed', label: 'Activity Feed', icon: Activity },
                                { id: 'discover', label: 'Discover', icon: Compass },
                                { id: 'community', label: 'Community', icon: Users }
                            ].map(tab => {
                                const Icon = tab.icon;
                                const isActive = activeTab === tab.id;
                                
                                return (
                                    <button
                                        key={tab.id}
                                        onClick={() => setActiveTab(tab.id)}
                                        className={`relative flex-1 flex items-center justify-center gap-2 py-3 px-4 rounded-full text-sm font-bold transition-colors z-10 ${
                                            isActive ? 'text-white' : 'text-foreground-muted hover:text-foreground hover:bg-foreground/5'
                                        }`}
                                    >
                                        {isActive && (
                                            <motion.div
                                                layoutId="explore-active-tab"
                                                className="absolute inset-0 bg-accent rounded-full -z-10 shadow-[0_4px_20px_rgba(156,87,223,0.5)]"
                                                transition={{ type: "spring", stiffness: 400, damping: 30 }}
                                            />
                                        )}
                                        <Icon size={18} className={isActive ? 'text-white' : ''} />
                                        {tab.label}
                                    </button>
                                );
                            })}
                        </div>
                    </div>
                </div>

                {/* Tab Content Areas */}
                <div className="pt-2">
                    {activeTab === 'feed' && (
                        <motion.div 
                            initial={{ opacity: 0, y: 15 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4 }}
                            className="flex flex-col lg:flex-row gap-10"
                        >
                            <div className="flex-1 w-full max-w-3xl">
                                {renderActivityFeed()}
                            </div>
                            <div className="w-full lg:w-80 xl:w-96 shrink-0 space-y-10">
                                {renderPlatformUpdates()}
                            </div>
                        </motion.div>
                    )}

                    {activeTab === 'discover' && (
                        <motion.div 
                            initial={{ opacity: 0, y: 15 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4 }}
                            className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-8"
                        >
                            {renderTrending()}
                            {renderHotThisWeek()}
                            {renderSuggestedCollaborators()}
                        </motion.div>
                    )}

                    {activeTab === 'community' && (
                        <motion.div 
                            initial={{ opacity: 0, y: 15 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4 }}
                            className="grid grid-cols-1 md:grid-cols-2 gap-8 max-w-5xl mx-auto"
                        >
                            {renderActiveChallenges()}
                            {renderUpcomingEvents()}
                        </motion.div>
                    )}
                </div>

            </div>
        </div>
    );
}
