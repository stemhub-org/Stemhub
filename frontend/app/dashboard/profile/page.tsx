"use client";

import { motion } from "framer-motion";
import {
    MapPin,
    Link as LinkIcon,
    Calendar,
    Music,
    Star,
    Headphones,
    GitBranch,
    Users,
    Edit2
} from "lucide-react";

export default function ProfilePage() {
    return (
        <div className="max-w-7xl mx-auto space-y-8 pb-12">
            {/* Header Section */}
            <motion.div
                initial={{ opacity: 0, y: 10 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.4 }}
                className="flex flex-col md:flex-row gap-8 items-start relative rounded-2xl border border-foreground/[0.08] bg-background-secondary/10 p-8 backdrop-blur-xl"
            >
                <div className="absolute right-8 top-8">
                    <button className="px-5 py-2.5 bg-accent hover:bg-accent/90 text-white text-sm font-medium rounded-xl transition-colors shadow-lg shadow-accent/20">
                        Edit Profile
                    </button>
                </div>

                <div className="h-32 w-32 md:h-40 md:w-40 rounded-full bg-gradient-to-tr from-accent to-purple-400 flex items-center justify-center text-white text-5xl font-bold shrink-0">
                    SK
                </div>

                <div className="space-y-4 pt-2 flex-1">
                    <div>
                        <h1 className="text-4xl font-medium tracking-tight mb-1" style={{ fontFamily: "var(--font-syne)" }}>Skrillex</h1>
                        <p className="text-foreground/60 text-base">@skrillex</p>
                    </div>

                    <p className="text-foreground/80 text-sm max-w-2xl leading-relaxed">
                        Electronic music producer & sound designer. Specializing in bass music, dubstep, and experimental sound design. Always pushing boundaries and collaborating with talented artists worldwide.
                    </p>

                    <div className="flex flex-wrap items-center gap-6 text-sm text-foreground/60 font-light">
                        <span className="flex items-center gap-1.5"><MapPin size={16} /> Los Angeles, CA</span>
                        <span className="flex items-center gap-1.5"><LinkIcon size={16} className="text-accent" /> <a href="#" className="text-accent hover:underline">skrillex.com</a></span>
                        <span className="flex items-center gap-1.5"><Calendar size={16} /> Joined January 2024</span>
                    </div>

                    <div className="flex items-center gap-6 text-sm pt-2">
                        <div><span className="font-medium text-foreground text-base">2.4k</span> <span className="text-foreground/60 font-light">followers</span></div>
                        <div><span className="font-medium text-foreground text-base">342</span> <span className="text-foreground/60 font-light">following</span></div>
                    </div>
                </div>
            </motion.div>

            {/* Production Styles */}
            <motion.div
                initial={{ opacity: 0, y: 10 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.4, delay: 0.1 }}
                className="rounded-2xl border border-foreground/[0.08] bg-background-secondary/30 p-6 backdrop-blur-xl"
            >
                <div className="flex items-center justify-between mb-5">
                    <div className="flex items-center gap-2">
                        <Music size={18} className="text-accent" />
                        <h2 className="text-base font-medium" style={{ fontFamily: "var(--font-syne)" }}>Production Styles</h2>
                    </div>
                    <button className="flex items-center gap-2 text-xs text-foreground/60 hover:text-foreground border border-foreground/10 px-4 py-2 rounded-xl transition-colors hover:bg-foreground/5">
                        <Edit2 size={14} /> Edit
                    </button>
                </div>
                <div className="flex items-center gap-3">
                    <span className="px-4 py-2 rounded-xl border border-accent/20 bg-accent/10 text-accent text-sm font-medium">Trap</span>
                    <span className="px-4 py-2 rounded-xl border border-accent/20 bg-accent/10 text-accent text-sm font-medium">Bass Music</span>
                    <span className="px-4 py-2 rounded-xl border border-accent/20 bg-accent/10 text-accent text-sm font-medium">Dubstep</span>
                </div>
            </motion.div>

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
                        <Star size={18} className="text-accent" />
                        <h2 className="text-base font-medium" style={{ fontFamily: "var(--font-syne)" }}>Featured Projects <span className="text-foreground/40 font-light text-sm tracking-normal">• Pinned</span></h2>
                    </div>

                    <div className="space-y-4">
                        {/* Project Card 1 */}
                        <div className="rounded-2xl border border-foreground/[0.08] bg-background-secondary/10 p-6 hover:bg-background-secondary/30 transition-all cursor-pointer backdrop-blur-xl group">
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
                                <span className="flex items-center gap-1.5 hover:text-accent transition-colors"><Star size={16} /> 1243</span>
                                <span className="flex items-center gap-1.5 hover:text-blue-400 transition-colors"><GitBranch size={16} /> 87</span>
                            </div>
                        </div>

                        {/* Project Card 2 */}
                        <div className="rounded-2xl border border-foreground/[0.08] bg-background-secondary/10 p-6 hover:bg-background-secondary/30 transition-all cursor-pointer backdrop-blur-xl group">
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
                                <span className="flex items-center gap-1.5 hover:text-accent transition-colors"><Star size={16} /> 892</span>
                                <span className="flex items-center gap-1.5 hover:text-blue-400 transition-colors"><GitBranch size={16} /> 54</span>
                            </div>
                        </div>

                        {/* Project Card 3 */}
                        <div className="rounded-2xl border border-foreground/[0.08] bg-background-secondary/10 p-6 hover:bg-background-secondary/30 transition-all cursor-pointer backdrop-blur-xl group">
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
                                <span className="flex items-center gap-1.5 hover:text-accent transition-colors"><Star size={16} /> 892</span>
                                <span className="flex items-center gap-1.5 hover:text-blue-400 transition-colors"><GitBranch size={16} /> 54</span>
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

                    <div className="rounded-2xl border border-foreground/[0.08] bg-background-secondary/10 p-6 backdrop-blur-xl">
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
                        <div className="flex flex-col rounded-2xl border border-foreground/[0.08] bg-background-secondary/10 p-5 backdrop-blur-xl hover:bg-background-secondary/30 transition-colors cursor-pointer">
                            <span className="text-xs text-foreground/50 font-light">Projects</span>
                            <p className="text-2xl font-medium mt-1 tracking-tight">24</p>
                        </div>
                        <div className="flex flex-col rounded-2xl border border-foreground/[0.08] bg-background-secondary/10 p-5 backdrop-blur-xl hover:bg-background-secondary/30 transition-colors cursor-pointer">
                            <span className="text-xs text-foreground/50 font-light">Collaborations</span>
                            <p className="text-2xl font-medium mt-1 tracking-tight">18</p>
                        </div>
                    </div>
                </motion.div>

            </div>
        </div>
    );
}
