"use client";

import { useState } from "react";
import { motion } from "framer-motion";
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

export default function ExplorePage() {
    const [activeTab, setActiveTab] = useState('feed');

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

            {/* Feed Item 1 */}
            <motion.div
                initial={{ opacity: 0, y: 15 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.5, delay: 0.1 }}
                className="bg-background-secondary dark:bg-background-tertiary border border-border-subtle rounded-2xl p-6 md:p-8 hover:border-accent/40 hover:shadow-[0_8px_30px_rgb(0,0,0,0.12)] transition-all duration-300 group"
            >
                <div className="flex items-center justify-between mb-6">
                    <div className="flex items-center gap-4">
                        <div className="h-12 w-12 rounded-full bg-gradient-to-br from-[#9C57DF] to-[#C28CF0] flex items-center justify-center text-white font-bold shadow-lg ring-4 ring-background">
                            MB
                        </div>
                        <div>
                            <div className="flex items-center gap-2 mb-1">
                                <span className="font-bold text-foreground text-lg group-hover:text-accent transition-colors">Metro Boomin</span>
                                <Sparkles size={16} className="text-accent" />
                            </div>
                            <span className="text-foreground-muted text-sm">released a new project</span>
                        </div>
                    </div>
                    <span className="text-xs font-semibold px-3 py-1 bg-background rounded-full text-foreground-muted flex items-center gap-1.5 border border-border-subtle shadow-sm">
                        <Clock size={12} /> 23m ago
                    </span>
                </div>

                <div className="bg-background rounded-xl border border-border-subtle p-6 relative overflow-hidden">
                    <div className="absolute top-0 right-0 w-32 h-32 bg-accent/5 rounded-full blur-3xl -mr-10 -mt-10 pointer-events-none"></div>
                    <div className="flex justify-between items-start mb-4 relative z-10">
                        <div>
                            <h3 className="text-xl font-bold text-foreground hover:text-accent cursor-pointer transition-colors mb-2">Future Hndrxx Vol. 2</h3>
                            <p className="text-sm text-foreground-muted leading-relaxed max-w-lg">Dark trap beats collection featuring heavy experimental 808s and complex hi-hat rolls.</p>
                        </div>
                        <button className="bg-accent/10 hover:bg-accent text-accent hover:text-white px-5 py-2.5 rounded-xl text-sm font-semibold transition-all duration-300 shadow-[0_0_15px_rgba(156,87,223,0.15)] hover:shadow-[0_0_25px_rgba(156,87,223,0.4)] hover:-translate-y-0.5 flex items-center gap-2">
                            <Play size={16} fill="currentColor" /> Listen
                        </button>
                    </div>
                    
                    <div className="flex flex-wrap gap-2 mt-4 mb-6 relative z-10">
                        <span className="px-3 py-1.5 rounded-lg bg-accent/5 text-accent border border-accent/20 text-xs font-semibold shadow-sm hover:bg-accent hover:text-white transition-colors cursor-pointer">Trap</span>
                        <span className="px-3 py-1.5 rounded-lg bg-accent/5 text-accent border border-accent/20 text-xs font-semibold shadow-sm hover:bg-accent hover:text-white transition-colors cursor-pointer">Dark</span>
                        <span className="px-3 py-1.5 rounded-lg bg-accent/5 text-accent border border-accent/20 text-xs font-semibold shadow-sm hover:bg-accent hover:text-white transition-colors cursor-pointer">808s</span>
                    </div>

                    <div className="grid grid-cols-3 gap-4 mb-6 max-w-md relative z-10">
                        <div className="bg-background-secondary dark:bg-background-tertiary rounded-lg border border-border-subtle p-3 text-center transition-transform hover:scale-105">
                            <div className="text-[10px] uppercase text-foreground-muted font-bold tracking-widest mb-1.5">BPM</div>
                            <div className="font-bold text-foreground text-base">140</div>
                        </div>
                        <div className="bg-background-secondary dark:bg-background-tertiary rounded-lg border border-border-subtle p-3 text-center transition-transform hover:scale-105">
                            <div className="text-[10px] uppercase text-foreground-muted font-bold tracking-widest mb-1.5">Key</div>
                            <div className="font-bold text-foreground text-base">F# min</div>
                        </div>
                        <div className="bg-background-secondary dark:bg-background-tertiary rounded-lg border border-border-subtle p-3 text-center transition-transform hover:scale-105">
                            <div className="text-[10px] uppercase text-foreground-muted font-bold tracking-widest mb-1.5">Duration</div>
                            <div className="font-bold text-foreground text-base">3:42</div>
                        </div>
                    </div>

                    <div className="flex items-center gap-6 text-sm font-medium text-foreground-muted border-t border-border-subtle pt-5 relative z-10">
                        <span className="flex items-center gap-2 hover:text-accent cursor-pointer transition-colors"><Star size={16} /> 243</span>
                        <span className="flex items-center gap-2 hover:text-accent cursor-pointer transition-colors"><GitFork size={16} /> 18</span>
                        <span className="flex items-center gap-2 hover:text-accent cursor-pointer transition-colors"><Play size={16} /> 1,547</span>
                    </div>
                </div>
            </motion.div>

            {/* Feed Item 2 */}
            <motion.div
                initial={{ opacity: 0, y: 15 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.5, delay: 0.2 }}
                className="bg-background-secondary dark:bg-background-tertiary border border-border-subtle rounded-2xl p-6 md:p-8 hover:border-blue-500/40 hover:shadow-[0_8px_30px_rgb(0,0,0,0.12)] transition-all duration-300 group"
            >
                <div className="flex items-center justify-between mb-6">
                    <div className="flex items-center gap-4">
                        <div className="h-12 w-12 rounded-full bg-gradient-to-br from-blue-500 to-indigo-600 flex items-center justify-center text-white font-bold shadow-lg ring-4 ring-background">
                            D5
                        </div>
                        <div>
                            <div className="flex items-center gap-2 mb-1">
                                <span className="font-bold text-foreground text-lg group-hover:text-blue-500 transition-colors">deadmau5</span>
                                <Box size={16} className="text-blue-500" />
                            </div>
                            <span className="text-foreground-muted text-sm">pushed v2.4 to <span className="text-foreground font-medium">Vocal-Chop-Toolkit</span></span>
                        </div>
                    </div>
                    <span className="text-xs font-semibold px-3 py-1 bg-background rounded-full text-foreground-muted flex items-center gap-1.5 border border-border-subtle shadow-sm">
                        <Clock size={12} /> 1h ago
                    </span>
                </div>

                <div className="bg-background rounded-xl border border-border-subtle p-6 relative overflow-hidden">
                    <div className="absolute top-0 right-0 w-32 h-32 bg-blue-500/5 rounded-full blur-3xl -mr-10 -mt-10 pointer-events-none"></div>
                    <div className="flex justify-between items-start mb-4 relative z-10">
                        <div>
                            <h3 className="text-xl font-bold text-foreground hover:text-blue-500 cursor-pointer transition-colors mb-2">Vocal-Chop-Toolkit</h3>
                            <p className="text-sm text-foreground-muted leading-relaxed max-w-lg">Added custom grain delay processing chain and optimized CPU usage for live performances.</p>
                        </div>
                        <button className="bg-blue-500/10 hover:bg-blue-500 text-blue-500 hover:text-white px-5 py-2.5 rounded-xl text-sm font-semibold transition-all duration-300 shadow-[0_0_15px_rgba(59,130,246,0.15)] hover:shadow-[0_0_25px_rgba(59,130,246,0.4)] hover:-translate-y-0.5 flex items-center gap-2">
                            <GitFork size={16} /> Fork
                        </button>
                    </div>
                    
                    <div className="flex flex-wrap gap-2 mt-4 mb-6 relative z-10">
                        <span className="px-3 py-1.5 rounded-lg bg-blue-500/5 text-blue-500 border border-blue-500/20 text-xs font-semibold shadow-sm hover:bg-blue-500 hover:text-white transition-colors cursor-pointer">Vocal Processing</span>
                        <span className="px-3 py-1.5 rounded-lg bg-blue-500/5 text-blue-500 border border-blue-500/20 text-xs font-semibold shadow-sm hover:bg-blue-500 hover:text-white transition-colors cursor-pointer">Sound Design</span>
                    </div>

                    <div className="flex items-center gap-6 text-sm font-medium text-foreground-muted border-t border-border-subtle pt-5 relative z-10">
                        <span className="flex items-center gap-2 hover:text-blue-500 cursor-pointer transition-colors"><Star size={16} /> 534</span>
                        <span className="flex items-center gap-2 hover:text-blue-500 cursor-pointer transition-colors"><GitFork size={16} /> 67</span>
                    </div>
                </div>
            </motion.div>
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
            {[
                { name: 'Madeon', followers: '12.4k', color: 'from-pink-500 to-rose-400', initial: 'MD', rise: '+234' },
                { name: 'REZZ', followers: '8.2k', color: 'from-purple-500 to-indigo-500', initial: 'RZ', rise: '+189' },
                { name: 'Illenium', followers: '15.8k', color: 'from-cyan-500 to-blue-500', initial: 'IL', rise: '+156' },
                { name: 'Zedd', followers: '22.1k', color: 'from-amber-400 to-orange-500', initial: 'ZD', rise: '+142' }
            ].map((producer, idx) => (
                <div key={idx} className="flex items-center justify-between p-3 rounded-xl bg-background border border-border-subtle hover:border-accent/40 hover:shadow-md transition-all cursor-pointer group/item">
                    <div className="flex items-center gap-4">
                        <span className="text-xs font-bold text-foreground-muted w-3">{idx + 1}</span>
                        <div className={`h-10 w-10 rounded-full bg-gradient-to-br ${producer.color} flex items-center justify-center text-white text-xs font-bold shadow-sm group-hover/item:scale-110 group-hover/item:rotate-3 transition-transform`}>
                            {producer.initial}
                        </div>
                        <div>
                            <div className="font-bold text-foreground group-hover/item:text-accent transition-colors">{producer.name}</div>
                            <div className="text-xs text-foreground-muted">{producer.followers} followers</div>
                        </div>
                    </div>
                    <div className="flex flex-col items-end">
                        <div className="text-xs font-bold text-emerald-500 bg-emerald-500/10 px-2 py-1 rounded-md">{producer.rise}</div>
                    </div>
                </div>
            ))}
        </div>
    );

    const renderHotThisWeek = () => renderCardContainer(
        Star, "Hot This Week", "Most starred projects", "text-amber-500",
        <div className="space-y-2">
            {[
                { name: 'Hyperpop-Starter-Kit', genre: 'Hyperpop', stars: '2.4k', color: 'bg-pink-500' },
                { name: 'Lofi-Beats-Collection', genre: 'Lo-Fi', stars: '1.8k', color: 'bg-indigo-400' },
                { name: 'Techno-Grooves', genre: 'Techno', stars: '1.2k', color: 'bg-teal-500' },
                { name: 'House-Vocals-Vol1', genre: 'House', stars: '850', color: 'bg-amber-500' }
            ].map((project, idx) => (
                <div key={idx} className="flex items-center justify-between p-3 rounded-xl bg-background border border-border-subtle hover:border-amber-500/40 hover:shadow-md transition-all cursor-pointer group/item">
                    <div className="flex items-center gap-3">
                        <div className={`w-2 h-2 rounded-full ${project.color} shadow-[0_0_8px_currentColor]`} />
                        <div>
                            <div className="font-bold text-foreground group-hover/item:text-amber-500 transition-colors truncate max-w-[140px]">{project.name}</div>
                            <div className="text-xs text-foreground-muted mt-0.5">{project.genre}</div>
                        </div>
                    </div>
                    <div className="flex items-center gap-1.5 text-xs font-bold text-foreground-muted bg-foreground/5 px-2 py-1 rounded-md">
                        <Star size={12} className="text-amber-500" fill="currentColor" /> {project.stars}
                    </div>
                </div>
            ))}
        </div>
    );

    const renderSuggestedCollaborators = () => renderCardContainer(
        Users, "Suggested Collabs", "Based on your style", "text-blue-500",
        <div className="space-y-3">
            {[
                { name: 'ODESZA', genres: ['Electronic', 'Indie'], mutual: 12, initials: 'OD', color: 'bg-gradient-to-br from-pink-500 to-orange-400' },
                { name: 'San Holo', genres: ['Future Bass'], mutual: 8, initials: 'SH', color: 'bg-gradient-to-br from-cyan-400 to-blue-500' },
                { name: 'Kasbo', genres: ['Melodic', 'Chill'], mutual: 15, initials: 'KB', color: 'bg-gradient-to-br from-purple-500 to-indigo-500' }
            ].map((collab, idx) => (
                <div key={idx} className="bg-background border border-border-subtle rounded-xl p-4 hover:border-blue-500/40 hover:shadow-md transition-all group/item">
                    <div className="flex items-center gap-3 mb-3 relative">
                        <div className={`h-12 w-12 rounded-full ${collab.color} flex items-center justify-center text-white text-sm font-bold shadow-md group-hover/item:shadow-lg transition-shadow`}>
                            {collab.initials}
                        </div>
                        <div>
                            <h4 className="font-bold text-foreground text-sm group-hover/item:text-blue-500 transition-colors">{collab.name}</h4>
                            <p className="text-[10px] text-foreground-muted mt-0.5">{collab.mutual} mutual connections</p>
                        </div>
                    </div>
                    <div className="flex gap-1.5 mb-4 flex-wrap">
                        {collab.genres.map(g => (
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
                {[
                    { version: 'v2.4.0', time: '2 days ago', title: 'Real-time Collaboration', desc: 'Work on projects simultaneously with your team in real-time.', icon: Users },
                    { version: 'v2.3.5', time: '1 week ago', title: 'Advanced Waveform Analysis', desc: 'New spectral view and frequency isolation tools added to player.', icon: Activity },
                    { version: 'v2.3.0', time: '2 weeks ago', title: 'Stem Export Presets', desc: 'Save custom export configurations for faster workflow.', icon: Box }
                ].map((update, idx) => {
                    const Icon = update.icon;
                    return (
                        <div key={idx} className="relative pl-6 pb-4 last:pb-0 border-l border-border-subtle group">
                            <div className="absolute left-[-17px] top-1 h-8 w-8 rounded-full bg-background border-4 border-background-secondary dark:border-background-tertiary flex items-center justify-center group-hover:bg-accent/10 group-hover:border-accent/20 transition-colors">
                                <Icon size={12} className="text-foreground-muted group-hover:text-accent transition-colors" />
                            </div>
                            <div className="bg-background border border-border-subtle rounded-xl p-4 hover:border-accent/30 hover:shadow-md transition-all ml-4">
                                <div className="flex items-center gap-2 mb-2">
                                    <span className="text-[10px] font-bold px-2 py-0.5 rounded-md bg-accent/10 text-accent border border-accent/20">{update.version}</span>
                                    <span className="text-[10px] text-foreground-muted/70 font-medium">{update.time}</span>
                                </div>
                                <h4 className="text-sm font-bold text-foreground group-hover:text-accent transition-colors mb-1.5">{update.title}</h4>
                                <p className="text-xs text-foreground-muted leading-relaxed">{update.desc}</p>
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
            {[
                { title: '7-Day Beat Challenge', desc: 'Create a full beat in under 3 hours using only stock plugins.', level: 'Intermediate', joined: '2,847', daysLeft: 3, prize: '$500 USD' },
                { title: 'Vocal Chop Remix', desc: 'Remix the provided stem using only vocal samples.', level: 'Advanced', joined: '1,523', daysLeft: 6, prize: 'Featured Spotlight' }
            ].map((challenge, idx) => (
                <div key={idx} className="bg-background border border-border-subtle rounded-xl p-5 hover:border-accent/40 hover:shadow-md transition-all group/item">
                    <div className="flex justify-between items-start mb-2">
                        <h4 className="font-bold text-foreground text-base group-hover/item:text-accent transition-colors">{challenge.title}</h4>
                        <span className="px-2 py-1 rounded-md bg-accent/10 text-accent border border-accent/20 text-[10px] font-extrabold uppercase tracking-widest">{challenge.level}</span>
                    </div>
                    <p className="text-xs text-foreground-muted mb-4">{challenge.desc}</p>
                    
                    <div className="grid grid-cols-2 gap-3 mb-4">
                        <div className="bg-foreground/5 rounded-lg p-2 text-center">
                            <div className="text-[10px] uppercase text-foreground-muted font-bold tracking-wider mb-1">Participants</div>
                            <div className="font-bold text-foreground text-xs flex items-center justify-center gap-1.5"><Users size={12} /> {challenge.joined}</div>
                        </div>
                        <div className="bg-foreground/5 rounded-lg p-2 text-center">
                            <div className="text-[10px] uppercase text-foreground-muted font-bold tracking-wider mb-1">Time Left</div>
                            <div className="font-bold text-amber-500 text-xs flex items-center justify-center gap-1.5"><Clock size={12} /> {challenge.daysLeft} days</div>
                        </div>
                    </div>

                    <div className="flex items-center justify-between mt-2 pt-4 border-t border-border-subtle">
                        <div className="text-xs">
                            <span className="text-foreground-muted">Prize: </span>
                            <span className="font-bold text-accent">{challenge.prize}</span>
                        </div>
                        <button className="px-4 py-2 rounded-lg bg-accent text-white font-bold text-xs hover:bg-accent/90 transition-all shadow-[0_4px_15px_rgba(156,87,223,0.3)] hover:-translate-y-0.5">
                            Join Now
                        </button>
                    </div>
                </div>
            ))}
        </div>
    );

    const renderUpcomingEvents = () => renderCardContainer(
        Clock, "Upcoming Events", "Workshops & Streams", "text-blue-500",
        <div className="space-y-4">
            {[
                { title: 'Ableton Live Masterclass', type: 'Workshop', host: 'Madeon', date: 'Mar 14, 2026 • 7:00 PM PST', attending: '342', color: 'bg-emerald-500' },
                { title: 'Mixing & Mastering Q&A', type: 'Live Stream', host: 'deadmau5', date: 'Mar 16, 2026 • 3:00 PM PST', attending: '1,247', color: 'bg-indigo-500' }
            ].map((event, idx) => (
                <div key={idx} className="bg-background border border-border-subtle rounded-xl p-5 hover:border-blue-500/40 hover:shadow-md transition-all group/item">
                    <div className="flex items-center gap-2 mb-3">
                        <span className={`w-2 h-2 rounded-full ${event.color} shadow-[0_0_8px_currentColor]`}></span>
                        <span className="text-[10px] font-extrabold uppercase tracking-widest text-foreground-muted">{event.type}</span>
                    </div>
                    <h4 className="font-bold text-foreground text-base group-hover/item:text-blue-500 transition-colors mb-1">{event.title}</h4>
                    <p className="text-xs text-foreground-muted mb-4 flex items-center gap-1.5"><span className="text-foreground">Host:</span> {event.host}</p>
                    
                    <div className="bg-blue-500/5 rounded-lg border border-blue-500/10 p-3 mb-4 space-y-2">
                        <p className="text-xs font-medium text-foreground-muted flex items-center gap-2"><Clock size={14} className="text-blue-500" /> {event.date}</p>
                        <p className="text-xs font-medium text-foreground-muted flex items-center gap-2"><Users size={14} className="text-blue-500" /> {event.attending} attending</p>
                    </div>
                    <button className="w-full py-2.5 rounded-lg bg-transparent border-2 border-border-subtle text-foreground font-bold text-xs hover:border-blue-500 hover:text-blue-500 transition-all hover:-translate-y-0.5 shadow-sm">
                        Register for Event
                    </button>
                </div>
            ))}
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
