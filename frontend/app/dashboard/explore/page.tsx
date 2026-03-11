"use client";

import { useState } from "react";
import { motion, AnimatePresence } from "framer-motion";
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
        <div className="flex-1 space-y-6">
            <div className="flex items-center gap-2 mb-6">
                <Activity size={20} className="text-accent" />
                <h2 className="text-xl font-semibold tracking-tight text-foreground">Activity Feed</h2>
                <span className="text-sm text-foreground-muted ml-2">• Following updates</span>
            </div>

            {/* Feed Item 1 */}
            <motion.div
                initial={{ opacity: 0, y: 10 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.4, delay: 0.1 }}
                className="bg-background-secondary dark:bg-background-tertiary border border-border-subtle rounded-xl p-6 hover:border-accent/40 transition-colors"
            >
                <div className="flex items-center justify-between mb-4">
                    <div className="flex items-center gap-3">
                        <div className="h-10 w-10 rounded-full bg-gradient-to-br from-[#9C57DF] to-[#C28CF0] flex items-center justify-center text-white font-bold shadow-md">
                            MB
                        </div>
                        <div>
                            <div className="flex items-center gap-2">
                                <Sparkles size={14} className="text-accent" />
                                <span className="font-semibold text-foreground">Metro Boomin</span>
                                <span className="text-foreground-muted text-sm">released a new project</span>
                            </div>
                        </div>
                    </div>
                    <span className="text-xs text-foreground-muted flex items-center gap-1">
                        <Clock size={12} /> 23m ago
                    </span>
                </div>

                <div className="bg-background rounded-lg border border-border-subtle p-5">
                    <div className="flex justify-between items-start mb-2">
                        <div>
                            <h3 className="text-lg font-semibold text-foreground hover:text-accent cursor-pointer transition-colors">Future Hndrxx Vol. 2</h3>
                            <p className="text-sm text-foreground-muted mt-1">Dark trap beats collection with experimental 808s</p>
                        </div>
                        <button className="bg-accent/10 hover:bg-accent text-accent hover:text-white px-4 py-2 rounded-lg text-sm font-medium transition-all duration-300 shadow-[0_0_15px_rgba(156,87,223,0.15)] hover:shadow-[0_0_20px_rgba(156,87,223,0.4)] flex items-center gap-2">
                            <Play size={14} fill="currentColor" /> Listen
                        </button>
                    </div>
                    
                    <div className="flex flex-wrap gap-2 mt-4 mb-5">
                        <span className="px-2.5 py-1 rounded bg-accent/5 text-accent border border-accent/20 text-xs font-medium">Trap</span>
                        <span className="px-2.5 py-1 rounded bg-accent/5 text-accent border border-accent/20 text-xs font-medium">Dark</span>
                        <span className="px-2.5 py-1 rounded bg-accent/5 text-accent border border-accent/20 text-xs font-medium">808s</span>
                    </div>

                    <div className="grid grid-cols-3 gap-3 mb-5 max-w-sm">
                        <div className="bg-background-secondary dark:bg-background-tertiary rounded border border-border-subtle p-2">
                            <div className="text-[10px] uppercase text-foreground-muted font-bold tracking-wider mb-1">BPM</div>
                            <div className="font-semibold text-foreground text-sm">140</div>
                        </div>
                        <div className="bg-background-secondary dark:bg-background-tertiary rounded border border-border-subtle p-2">
                            <div className="text-[10px] uppercase text-foreground-muted font-bold tracking-wider mb-1">Key</div>
                            <div className="font-semibold text-foreground text-sm">F# minor</div>
                        </div>
                        <div className="bg-background-secondary dark:bg-background-tertiary rounded border border-border-subtle p-2">
                            <div className="text-[10px] uppercase text-foreground-muted font-bold tracking-wider mb-1">Duration</div>
                            <div className="font-semibold text-foreground text-sm">3:42</div>
                        </div>
                    </div>

                    <div className="flex items-center gap-5 text-sm text-foreground-muted border-t border-border-subtle pt-4">
                        <span className="flex items-center gap-1.5 hover:text-accent cursor-pointer transition-colors"><Star size={14} /> 243</span>
                        <span className="flex items-center gap-1.5 hover:text-accent cursor-pointer transition-colors"><GitFork size={14} /> 18</span>
                        <span className="flex items-center gap-1.5 hover:text-accent cursor-pointer transition-colors"><Play size={14} /> 1,547</span>
                    </div>
                </div>
            </motion.div>

            {/* Feed Item 2 */}
            <motion.div
                initial={{ opacity: 0, y: 10 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.4, delay: 0.2 }}
                className="bg-background-secondary dark:bg-background-tertiary border border-border-subtle rounded-xl p-6 hover:border-accent/40 transition-colors"
            >
                <div className="flex items-center justify-between mb-4">
                    <div className="flex items-center gap-3">
                        <div className="h-10 w-10 rounded-full bg-gradient-to-br from-blue-500 to-indigo-600 flex items-center justify-center text-white font-bold shadow-md">
                            D5
                        </div>
                        <div>
                            <div className="flex items-center gap-2">
                                <Box size={14} className="text-blue-500" />
                                <span className="font-semibold text-foreground">deadmau5</span>
                                <span className="text-foreground-muted text-sm">pushed v2.4 to</span>
                            </div>
                        </div>
                    </div>
                    <span className="text-xs text-foreground-muted flex items-center gap-1">
                        <Clock size={12} /> 1h ago
                    </span>
                </div>

                <div className="bg-background rounded-lg border border-border-subtle p-5">
                    <div className="flex justify-between items-start mb-2">
                        <div>
                            <h3 className="text-lg font-semibold text-foreground hover:text-accent cursor-pointer transition-colors">Vocal-Chop-Toolkit</h3>
                            <p className="text-sm text-foreground-muted mt-1">Added custom grain delay processing chain</p>
                        </div>
                        <button className="bg-accent/10 hover:bg-accent text-accent hover:text-white px-4 py-2 rounded-lg text-sm font-medium transition-all duration-300 shadow-[0_0_15px_rgba(156,87,223,0.15)] hover:shadow-[0_0_20px_rgba(156,87,223,0.4)] flex items-center gap-2">
                            <Play size={14} fill="currentColor" /> Listen
                        </button>
                    </div>
                    
                    <div className="flex flex-wrap gap-2 mt-4 mb-5">
                        <span className="px-2.5 py-1 rounded bg-accent/5 text-accent border border-accent/20 text-xs font-medium">Vocal Processing</span>
                        <span className="px-2.5 py-1 rounded bg-accent/5 text-accent border border-accent/20 text-xs font-medium">Sound Design</span>
                    </div>

                    <div className="flex items-center gap-5 text-sm text-foreground-muted border-t border-border-subtle pt-4">
                        <span className="flex items-center gap-1.5 hover:text-accent cursor-pointer transition-colors"><Star size={14} /> 534</span>
                        <span className="flex items-center gap-1.5 hover:text-accent cursor-pointer transition-colors"><GitFork size={14} /> 67</span>
                    </div>
                </div>
            </motion.div>

            {/* Feed Item 3 */}
            <motion.div
                initial={{ opacity: 0, y: 10 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.4, delay: 0.3 }}
                className="bg-background-secondary dark:bg-background-tertiary border border-border-subtle rounded-xl p-6 hover:border-accent/40 transition-colors"
            >
                <div className="flex items-center justify-between mb-4">
                    <div className="flex items-center gap-3">
                        <div className="h-10 w-10 rounded-full bg-gradient-to-br from-pink-500 to-rose-500 flex items-center justify-center text-white font-bold shadow-md">
                            PR
                        </div>
                        <div>
                            <div className="flex items-center gap-2">
                                <Sparkles size={14} className="text-accent" />
                                <span className="font-semibold text-foreground">Porter Robinson</span>
                                <span className="text-foreground-muted text-sm">released a new project</span>
                            </div>
                        </div>
                    </div>
                    <span className="text-xs text-foreground-muted flex items-center gap-1">
                        <Clock size={12} /> 6h ago
                    </span>
                </div>

                <div className="bg-background rounded-lg border border-border-subtle p-5">
                    <div className="flex justify-between items-start mb-2">
                        <div>
                            <h3 className="text-lg font-semibold text-foreground hover:text-accent cursor-pointer transition-colors">Nurture-Stems-Pack</h3>
                            <p className="text-sm text-foreground-muted mt-1">Official stems from Nurture album for remixing</p>
                        </div>
                        <button className="bg-accent/10 hover:bg-accent text-accent hover:text-white px-4 py-2 rounded-lg text-sm font-medium transition-all duration-300 shadow-[0_0_15px_rgba(156,87,223,0.15)] hover:shadow-[0_0_20px_rgba(156,87,223,0.4)] flex items-center gap-2">
                            <Play size={14} fill="currentColor" /> Listen
                        </button>
                    </div>
                    
                    <div className="flex flex-wrap gap-2 mt-4 mb-5">
                        <span className="px-2.5 py-1 rounded bg-accent/5 text-accent border border-accent/20 text-xs font-medium">Future Bass</span>
                        <span className="px-2.5 py-1 rounded bg-accent/5 text-accent border border-accent/20 text-xs font-medium">Melodic</span>
                        <span className="px-2.5 py-1 rounded bg-accent/5 text-accent border border-accent/20 text-xs font-medium">Remix Stems</span>
                    </div>

                    <div className="grid grid-cols-3 gap-3 mb-5 max-w-sm">
                        <div className="bg-background-secondary dark:bg-background-tertiary rounded border border-border-subtle p-2">
                            <div className="text-[10px] uppercase text-foreground-muted font-bold tracking-wider mb-1">BPM</div>
                            <div className="font-semibold text-foreground text-sm">128</div>
                        </div>
                        <div className="bg-background-secondary dark:bg-background-tertiary rounded border border-border-subtle p-2">
                            <div className="text-[10px] uppercase text-foreground-muted font-bold tracking-wider mb-1">Key</div>
                            <div className="font-semibold text-foreground text-sm">C major</div>
                        </div>
                        <div className="bg-background-secondary dark:bg-background-tertiary rounded border border-border-subtle p-2">
                            <div className="text-[10px] uppercase text-foreground-muted font-bold tracking-wider mb-1">Duration</div>
                            <div className="font-semibold text-foreground text-sm">4:18</div>
                        </div>
                    </div>

                    <div className="flex items-center gap-5 text-sm text-foreground-muted border-t border-border-subtle pt-4">
                        <span className="flex items-center gap-1.5 hover:text-accent cursor-pointer transition-colors"><Star size={14} /> 892</span>
                        <span className="flex items-center gap-1.5 hover:text-accent cursor-pointer transition-colors"><GitFork size={14} /> 124</span>
                        <span className="flex items-center gap-1.5 hover:text-accent cursor-pointer transition-colors"><Play size={14} /> 5,201</span>
                    </div>
                </div>
            </motion.div>
        </div>
    );

    const renderTrending = () => (
        <div>
            <div className="flex items-center gap-2 mb-4">
                <TrendingUp size={18} className="text-accent" />
                <h3 className="font-semibold text-foreground">Trending Producers</h3>
            </div>
            <div className="bg-background-secondary dark:bg-background-tertiary border border-border-subtle rounded-xl p-2 space-y-1">
                {[
                    { name: 'Madeon', followers: '12.4k', color: 'from-pink-500 to-rose-400', initial: 'MD', rise: '+234' },
                    { name: 'REZZ', followers: '8.2k', color: 'from-purple-500 to-indigo-500', initial: 'RZ', rise: '+189' },
                    { name: 'Illenium', followers: '15.8k', color: 'from-cyan-500 to-blue-500', initial: 'IL', rise: '+156' },
                    { name: 'Zedd', followers: '22.1k', color: 'from-amber-400 to-orange-500', initial: 'ZD', rise: '+142' }
                ].map((producer, idx) => (
                    <div key={idx} className="flex items-center justify-between p-3 rounded-lg hover:bg-background transition-colors cursor-pointer group">
                        <div className="flex items-center gap-3">
                            <span className="text-xs font-bold text-foreground-muted w-3">{idx + 1}</span>
                            <div className={`h-8 w-8 rounded-full bg-gradient-to-br ${producer.color} flex items-center justify-center text-white text-xs font-bold shadow-sm group-hover:scale-110 transition-transform`}>
                                {producer.initial}
                            </div>
                            <div>
                                <div className="text-sm font-semibold text-foreground group-hover:text-accent transition-colors">{producer.name}</div>
                                <div className="text-[10px] text-foreground-muted">{producer.followers} followers</div>
                            </div>
                        </div>
                        <div className="text-xs font-medium text-emerald-500">{producer.rise}</div>
                    </div>
                ))}
            </div>
        </div>
    );

    const renderHotThisWeek = () => (
        <div>
            <div className="flex items-center gap-2 mb-4">
                <Star size={18} className="text-accent" />
                <h3 className="font-semibold text-foreground">Hot This Week</h3>
            </div>
            <div className="bg-background-secondary dark:bg-background-tertiary border border-border-subtle rounded-xl p-2 space-y-1">
                {[
                    { name: 'Hyperpop-Starter-Kit', genre: 'Hyperpop', stars: '2.4k', color: 'bg-pink-500' },
                    { name: 'Lofi-Beats-Collection', genre: 'Lo-Fi', stars: '1.8k', color: 'bg-indigo-400' },
                    { name: 'Techno-Grooves', genre: 'Techno', stars: '1.2k', color: 'bg-teal-500' }
                ].map((project, idx) => (
                    <div key={idx} className="flex items-center justify-between p-3 rounded-lg hover:bg-background transition-colors cursor-pointer group">
                        <div className="flex items-center gap-3">
                            <div className={`w-1.5 h-1.5 rounded-full ${project.color}`} />
                            <div>
                                <div className="text-sm font-semibold text-foreground group-hover:text-accent transition-colors truncate max-w-[140px]">{project.name}</div>
                                <div className="text-[10px] text-foreground-muted">{project.genre}</div>
                            </div>
                        </div>
                        <div className="flex items-center gap-1 text-xs text-foreground-muted">
                            <Star size={10} className="text-yellow-500" /> {project.stars}
                        </div>
                    </div>
                ))}
            </div>
        </div>
    );

    const renderPlatformUpdates = () => (
        <div>
            <div className="flex items-center gap-2 mb-4">
                <Zap size={18} className="text-accent" />
                <h3 className="font-semibold text-foreground">Platform Updates</h3>
            </div>
            <div className="space-y-3">
                {[
                    { version: 'v2.4.0', time: '2 days ago', title: 'Real-time Collaboration', desc: 'Work on projects simultaneously with your team', icon: Users },
                    { version: 'v2.3.5', time: '1 week ago', title: 'Advanced Waveform Analysis', desc: 'New spectral view and frequency isolation tools', icon: Activity },
                    { version: 'v2.3.0', time: '2 weeks ago', title: 'Stem Export Presets', desc: 'Save custom export configurations for faster workflow', icon: Box }
                ].map((update, idx) => {
                    const Icon = update.icon;
                    return (
                        <div key={idx} className="bg-background-secondary dark:bg-background-tertiary border border-border-subtle rounded-xl p-4 hover:border-accent/30 transition-colors group cursor-pointer">
                            <div className="flex items-start gap-3">
                                <div className="h-8 w-8 rounded-lg bg-accent/10 border border-accent/20 flex items-center justify-center shrink-0 group-hover:bg-accent/20 transition-colors">
                                    <Icon size={14} className="text-accent" />
                                </div>
                                <div>
                                    <div className="flex items-center gap-2 mb-1">
                                        <span className="text-[10px] font-mono px-1.5 py-0.5 rounded bg-foreground/5 text-foreground-muted border border-border-subtle">{update.version}</span>
                                        <span className="text-[10px] text-foreground-muted/70">{update.time}</span>
                                    </div>
                                    <h4 className="text-sm font-semibold text-foreground group-hover:text-accent transition-colors mb-1.5">{update.title}</h4>
                                    <p className="text-xs text-foreground-muted leading-relaxed">{update.desc}</p>
                                </div>
                            </div>
                        </div>
                    );
                })}
            </div>
        </div>
    );

    const renderSuggestedCollaborators = () => (
        <div>
            <div className="flex items-center gap-2 mb-4">
                <Users size={18} className="text-accent" />
                <h3 className="font-semibold text-foreground">Suggested Collaborators</h3>
            </div>
            <div className="space-y-3">
                {[
                    { name: 'ODESZA', genres: ['Electronic', 'Indie'], mutual: 12, initials: 'OD', color: 'bg-pink-500' },
                    { name: 'San Holo', genres: ['Future Bass'], mutual: 8, initials: 'SH', color: 'bg-cyan-500' },
                    { name: 'Kasbo', genres: ['Melodic', 'Chill'], mutual: 15, initials: 'KB', color: 'bg-purple-500' }
                ].map((collab, idx) => (
                    <div key={idx} className="bg-background-secondary dark:bg-background-tertiary border border-border-subtle rounded-xl p-4 hover:border-accent/30 transition-colors">
                        <div className="flex items-center gap-3 mb-3">
                            <div className={`h-10 w-10 rounded-full ${collab.color} flex items-center justify-center text-white text-xs font-bold shadow-md`}>
                                {collab.initials}
                            </div>
                            <div>
                                <h4 className="text-sm font-semibold text-foreground">{collab.name}</h4>
                                <div className="flex gap-1 mt-1 flex-wrap">
                                    {collab.genres.map(g => (
                                        <span key={g} className="px-1.5 py-0.5 rounded bg-foreground/5 text-foreground-muted border border-border-subtle text-[10px]">{g}</span>
                                    ))}
                                </div>
                            </div>
                        </div>
                        <p className="text-xs text-foreground-muted mb-3">{collab.mutual} mutual connections</p>
                        <button className="w-full py-2 rounded-lg bg-accent/10 border border-accent/20 text-accent font-medium text-xs hover:bg-accent hover:text-white transition-all">
                            Connect
                        </button>
                    </div>
                ))}
            </div>
        </div>
    );

    const renderActiveChallenges = () => (
        <div>
            <div className="flex items-center gap-2 mb-4">
                <TrendingUp size={18} className="text-accent" />
                <h3 className="font-semibold text-foreground">Active Challenges</h3>
            </div>
            <div className="space-y-3">
                {[
                    { title: '7-Day Beat Challenge', desc: 'Create a full beat in under 3 hours', level: 'Intermediate', joined: '2,847', daysLeft: 3, prize: 'Prize: $500' },
                    { title: 'Vocal Chop Remix', desc: 'Remix using only vocal samples', level: 'Advanced', joined: '1,523', daysLeft: 6, prize: 'Prize: Featured' }
                ].map((challenge, idx) => (
                    <div key={idx} className="bg-background-secondary dark:bg-background-tertiary border border-border-subtle rounded-xl p-4 hover:border-accent/30 transition-colors">
                        <h4 className="text-sm font-semibold text-foreground mb-1">{challenge.title}</h4>
                        <p className="text-xs text-foreground-muted mb-3">{challenge.desc}</p>
                        <div className="flex items-center gap-2 mb-4">
                            <span className="px-1.5 py-0.5 rounded bg-accent/10 text-accent border border-accent/20 text-[10px] font-medium">{challenge.level}</span>
                            <span className="text-[10px] text-foreground-muted">• {challenge.joined} joined</span>
                        </div>
                        <div className="flex items-center justify-between text-xs mb-3 border-t border-border-subtle pt-3 mt-1">
                            <span className="text-foreground-muted flex items-center gap-1"><Clock size={12} /> Ends in {challenge.daysLeft} days</span>
                            <span className="font-semibold text-accent">{challenge.prize}</span>
                        </div>
                        <button className="w-full py-2 rounded-lg bg-accent text-white font-medium text-xs hover:bg-accent/90 transition-all shadow-[0_0_10px_rgba(156,87,223,0.3)]">
                            Join Challenge
                        </button>
                    </div>
                ))}
            </div>
        </div>
    );

    const renderUpcomingEvents = () => (
        <div>
            <div className="flex items-center gap-2 mb-4">
                <Clock size={18} className="text-accent" />
                <h3 className="font-semibold text-foreground">Upcoming Events</h3>
            </div>
            <div className="space-y-3">
                {[
                    { title: 'Ableton Live Masterclass', type: 'Workshop', host: 'Madeon', date: 'Mar 14, 2026 • 7:00 PM PST', attending: '342' },
                    { title: 'Mixing & Mastering Q&A', type: 'Live Stream', host: 'deadmau5', date: 'Mar 16, 2026 • 3:00 PM PST', attending: '1,247' }
                ].map((event, idx) => (
                    <div key={idx} className="bg-background-secondary dark:bg-background-tertiary border border-border-subtle rounded-xl p-4 hover:border-accent/30 transition-colors">
                        <span className="inline-block px-1.5 py-0.5 rounded bg-accent/10 text-accent border border-accent/20 text-[10px] font-medium mb-2">{event.type}</span>
                        <h4 className="text-sm font-semibold text-foreground mb-1">{event.title}</h4>
                        <p className="text-xs text-foreground-muted mb-3">Hosted by {event.host}</p>
                        <div className="space-y-1.5 mb-4">
                            <p className="text-xs text-foreground-muted flex items-center gap-1.5"><Clock size={10} /> {event.date}</p>
                            <p className="text-xs text-foreground-muted flex items-center gap-1.5"><Users size={10} /> {event.attending} attending</p>
                        </div>
                        <button className="w-full py-2 rounded-lg bg-transparent border border-border-subtle text-foreground font-medium text-xs hover:border-accent hover:text-accent transition-all">
                            Register
                        </button>
                    </div>
                ))}
            </div>
        </div>
    );

    return (
        <div className="min-h-full bg-background flex flex-col">
            <div className="w-full mx-auto space-y-6 pb-12 pt-8 px-4 lg:px-8 2xl:px-12 max-w-7xl">
                
                {/* Header & Tabs */}
                <div>
                    <h1 className="text-2xl font-bold text-foreground mb-6 flex items-center gap-2">
                        <Globe className="text-accent" /> Explore
                    </h1>
                    
                    <div className="flex gap-6 border-b border-border-subtle">
                        {[
                            { id: 'feed', label: 'Activity Feed', icon: Activity },
                            { id: 'discover', label: 'Discover', icon: Compass },
                            { id: 'community', label: 'Community', icon: Users }
                        ].map(tab => {
                            const Icon = tab.icon;
                            return (
                                <button
                                    key={tab.id}
                                    onClick={() => setActiveTab(tab.id)}
                                    className={`flex items-center gap-2 pb-3 px-1 border-b-2 font-medium transition-colors ${
                                        activeTab === tab.id
                                            ? 'border-accent text-accent'
                                            : 'border-transparent text-foreground-muted hover:text-foreground hover:border-border-subtle'
                                    }`}
                                >
                                    <Icon size={16} />
                                    {tab.label}
                                </button>
                            );
                        })}
                    </div>
                </div>

                {/* Tab Content Areas */}
                <div className="pt-4">
                    {activeTab === 'feed' && (
                        <motion.div 
                            initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.3 }}
                            className="flex flex-col lg:flex-row gap-8"
                        >
                            <div className="flex-1 w-full max-w-3xl">
                                {renderActivityFeed()}
                            </div>
                            <div className="w-full lg:w-80 shrink-0 space-y-8">
                                {renderPlatformUpdates()}
                            </div>
                        </motion.div>
                    )}

                    {activeTab === 'discover' && (
                        <motion.div 
                            initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.3 }}
                            className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-8"
                        >
                            {renderTrending()}
                            {renderHotThisWeek()}
                            {renderSuggestedCollaborators()}
                        </motion.div>
                    )}

                    {activeTab === 'community' && (
                        <motion.div 
                            initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.3 }}
                            className="grid grid-cols-1 md:grid-cols-2 gap-8 max-w-4xl"
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
