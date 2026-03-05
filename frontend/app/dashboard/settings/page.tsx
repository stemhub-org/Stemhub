"use client";

import { useState } from "react";
import { motion, AnimatePresence } from "framer-motion";
import {
    User,
    Lock,
    Link as LinkIcon,
    HardDrive,
    Bell,
    Save,
    UploadCloud,
    Trash2,
    ShieldAlert,
    CheckCircle2,
    Github
} from "lucide-react";

const TABS = [
    { id: "profile", label: "Profile", icon: User },
    { id: "account", label: "Account", icon: Lock },
    { id: "integrations", label: "Integrations", icon: LinkIcon },
    { id: "storage", label: "Storage & Billing", icon: HardDrive },
    { id: "notifications", label: "Notifications", icon: Bell },
];

export default function SettingsPage() {
    const [activeTab, setActiveTab] = useState("profile");

    return (
        <div className="max-w-7xl mx-auto space-y-10 pb-12">
            <motion.div
                initial={{ opacity: 0, y: 10 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.4 }}
            >
                <h1 className="text-3xl font-light mb-2 tracking-tight" style={{ fontFamily: "var(--font-syne)" }}>
                    Settings
                </h1>
                <p className="text-foreground/60 font-light">
                    Manage your account preferences, integrations, and billing.
                </p>
            </motion.div>

            <div className="flex flex-col md:flex-row gap-8">
                {/* Sidebar Navigation */}
                <motion.div
                    initial={{ opacity: 0, x: -10 }}
                    animate={{ opacity: 1, x: 0 }}
                    transition={{ duration: 0.4, delay: 0.1 }}
                    className="w-full md:w-64 shrink-0 space-y-1"
                >
                    {TABS.map((tab) => {
                        const Icon = tab.icon;
                        const isActive = activeTab === tab.id;
                        return (
                            <button
                                key={tab.id}
                                onClick={() => setActiveTab(tab.id)}
                                className={`w-full flex items-center gap-3 px-4 py-3 rounded-xl transition-all duration-200 text-sm font-light ${isActive
                                        ? "bg-accent/10 text-accent font-medium border border-accent/20"
                                        : "text-foreground/70 hover:bg-foreground/5 hover:text-foreground border border-transparent"
                                    }`}
                            >
                                <Icon size={18} strokeWidth={isActive ? 2.5 : 2} />
                                {tab.label}
                            </button>
                        );
                    })}
                </motion.div>

                {/* Content Area */}
                <motion.div
                    initial={{ opacity: 0, y: 10 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.4, delay: 0.2 }}
                    className="flex-1 rounded-2xl border border-foreground/[0.08] bg-background-secondary/30 backdrop-blur-xl p-8 min-h-[500px]"
                >
                    <AnimatePresence mode="wait">
                        {activeTab === "profile" && <ProfileSettings key="profile" />}
                        {activeTab === "account" && <AccountSettings key="account" />}
                        {activeTab === "integrations" && <IntegrationSettings key="integrations" />}
                        {activeTab === "storage" && <StorageSettings key="storage" />}
                        {activeTab === "notifications" && <NotificationSettings key="notifications" />}
                    </AnimatePresence>
                </motion.div>
            </div>
        </div>
    );
}

function ProfileSettings() {
    return (
        <motion.div
            initial={{ opacity: 0, y: 10 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -10 }}
            transition={{ duration: 0.3 }}
            className="space-y-8"
        >
            <div>
                <h2 className="text-xl font-medium tracking-tight mb-1" style={{ fontFamily: "var(--font-syne)" }}>Public Profile</h2>
                <p className="text-sm text-foreground/50 font-light">This information will be displayed publicly on your producer page.</p>
            </div>

            <div className="flex items-center gap-6 pb-6 border-b border-foreground/5">
                <div className="h-24 w-24 rounded-full bg-gradient-to-br from-accent/20 to-accent/5 border border-accent/20 flex items-center justify-center relative overflow-hidden group cursor-pointer">
                    <User size={32} className="text-accent/50" />
                    <div className="absolute inset-0 bg-background/80 flex flex-col items-center justify-center opacity-0 group-hover:opacity-100 transition-opacity">
                        <UploadCloud size={20} className="text-accent mb-1" />
                        <span className="text-[10px] font-medium text-accent">Upload</span>
                    </div>
                </div>
                <div>
                    <h3 className="text-sm font-medium mb-1">Profile Picture</h3>
                    <p className="text-xs text-foreground/50 font-light mb-3">JPEG, PNG, or WEBP. Max 2MB.</p>
                    <div className="flex gap-3">
                        <button className="px-4 py-2 rounded-lg bg-foreground/5 hover:bg-foreground/10 text-xs font-medium transition-colors border border-foreground/10">
                            Change
                        </button>
                        <button className="px-4 py-2 rounded-lg text-red-400 hover:bg-red-400/10 text-xs font-medium transition-colors">
                            Remove
                        </button>
                    </div>
                </div>
            </div>

            <div className="space-y-5">
                <div className="space-y-2">
                    <label className="text-sm font-medium text-foreground/80">Username</label>
                    <input
                        type="text"
                        defaultValue="erwan_prod"
                        className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 py-3 px-4 text-sm font-light text-foreground focus:border-accent/40 focus:outline-none transition-all"
                    />
                </div>
                <div className="space-y-2">
                    <label className="text-sm font-medium text-foreground/80">Artist Name</label>
                    <input
                        type="text"
                        defaultValue="Erwan S."
                        className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 py-3 px-4 text-sm font-light text-foreground focus:border-accent/40 focus:outline-none transition-all"
                    />
                </div>
                <div className="space-y-2">
                    <label className="text-sm font-medium text-foreground/80">Bio</label>
                    <textarea
                        rows={4}
                        defaultValue="Electronic music producer based in Paris. Specializing in Synthwave and Cyberpunk aesthetics."
                        className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 py-3 px-4 text-sm font-light text-foreground focus:border-accent/40 focus:outline-none transition-all resize-none"
                    />
                </div>
            </div>

            <div className="pt-4 flex justify-end">
                <button className="flex items-center gap-2 px-6 py-2.5 rounded-xl bg-accent hover:bg-accent/90 text-white text-sm font-medium transition-all shadow-lg shadow-accent/20">
                    <Save size={16} /> Save Changes
                </button>
            </div>
        </motion.div>
    );
}

function AccountSettings() {
    return (
        <motion.div
            initial={{ opacity: 0, y: 10 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -10 }}
            transition={{ duration: 0.3 }}
            className="space-y-8"
        >
            <div>
                <h2 className="text-xl font-medium tracking-tight mb-1" style={{ fontFamily: "var(--font-syne)" }}>Account Security</h2>
                <p className="text-sm text-foreground/50 font-light">Manage your password and security preferences.</p>
            </div>

            <div className="space-y-5 pb-8 border-b border-foreground/5">
                <div className="space-y-2">
                    <label className="text-sm font-medium text-foreground/80">Email Address</label>
                    <div className="flex gap-4">
                        <input
                            type="email"
                            defaultValue="erwan@stemhub.com"
                            disabled
                            className="w-full rounded-xl border border-foreground/[0.08] bg-background/50 py-3 px-4 text-sm font-light text-foreground/60 cursor-not-allowed"
                        />
                        <button className="shrink-0 px-4 py-3 rounded-xl bg-foreground/5 hover:bg-foreground/10 text-sm font-medium transition-colors border border-foreground/10">
                            Change
                        </button>
                    </div>
                </div>

                <div className="space-y-2 pt-2">
                    <label className="text-sm font-medium text-foreground/80">Current Password</label>
                    <input
                        type="password"
                        placeholder="••••••••"
                        className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 py-3 px-4 text-sm font-light text-foreground focus:border-accent/40 focus:outline-none transition-all"
                    />
                </div>
                <div className="grid grid-cols-1 md:grid-cols-2 gap-5">
                    <div className="space-y-2">
                        <label className="text-sm font-medium text-foreground/80">New Password</label>
                        <input
                            type="password"
                            className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 py-3 px-4 text-sm font-light text-foreground focus:border-accent/40 focus:outline-none transition-all"
                        />
                    </div>
                    <div className="space-y-2">
                        <label className="text-sm font-medium text-foreground/80">Confirm New Password</label>
                        <input
                            type="password"
                            className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 py-3 px-4 text-sm font-light text-foreground focus:border-accent/40 focus:outline-none transition-all"
                        />
                    </div>
                </div>

                <div className="pt-2">
                    <button className="px-6 py-2.5 rounded-xl bg-foreground/5 hover:bg-foreground/10 text-sm font-medium transition-colors border border-foreground/10 text-foreground">
                        Update Password
                    </button>
                </div>
            </div>

            <div className="space-y-4">
                <div className="flex items-center gap-3 text-red-500">
                    <ShieldAlert size={20} />
                    <h2 className="text-xl font-medium tracking-tight" style={{ fontFamily: "var(--font-syne)" }}>Danger Zone</h2>
                </div>
                <div className="p-5 rounded-xl border border-red-500/20 bg-red-500/5 flex flex-col md:flex-row items-start md:items-center justify-between gap-4">
                    <div>
                        <h3 className="text-sm font-medium text-red-400 mb-1">Delete Account</h3>
                        <p className="text-xs text-foreground/60 font-light">Permanently remove your account and all your projects. This action cannot be undone.</p>
                    </div>
                    <button className="shrink-0 flex items-center gap-2 px-5 py-2.5 rounded-xl bg-red-500/10 hover:bg-red-500/20 text-red-500 text-sm font-medium transition-colors border border-red-500/20">
                        <Trash2 size={16} /> Delete Account
                    </button>
                </div>
            </div>
        </motion.div>
    );
}

function IntegrationSettings() {
    return (
        <motion.div
            initial={{ opacity: 0, y: 10 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -10 }}
            transition={{ duration: 0.3 }}
            className="space-y-8"
        >
            <div>
                <h2 className="text-xl font-medium tracking-tight mb-1" style={{ fontFamily: "var(--font-syne)" }}>Connected Apps & DAWs</h2>
                <p className="text-sm text-foreground/50 font-light">Link your Digital Audio Workstations and third-party services.</p>
            </div>

            <div className="space-y-4">
                <h3 className="text-sm font-medium text-foreground/80 mb-3">Authentication</h3>

                {/* Google OAuth connected state */}
                <div className="flex items-center justify-between p-5 rounded-xl border border-accent/20 bg-accent/5">
                    <div className="flex items-center gap-4">
                        <div className="h-10 w-10 rounded-full bg-white flex items-center justify-center p-2 shadow-sm">
                            <svg viewBox="0 0 24 24" className="w-full h-full">
                                <path d="M22.56 12.25c0-.78-.07-1.53-.2-2.25H12v4.26h5.92a5.06 5.06 0 0 1-2.2 3.32v2.77h3.57c2.08-1.92 3.28-4.74 3.28-8.1z" fill="#4285F4" />
                                <path d="M12 23c2.97 0 5.46-.98 7.28-2.66l-3.57-2.77c-.98.66-2.23 1.06-3.71 1.06-2.86 0-5.29-1.93-6.16-4.53H2.18v2.84C3.99 20.53 7.7 23 12 23z" fill="#34A853" />
                                <path d="M5.84 14.09c-.22-.66-.35-1.36-.35-2.09s.13-1.43.35-2.09V7.07H2.18C1.43 8.55 1 10.22 1 12s.43 3.45 1.18 4.93l2.85-2.22.81-.62z" fill="#FBBC05" />
                                <path d="M12 5.38c1.62 0 3.06.56 4.21 1.64l3.15-3.15C17.45 2.09 14.97 1 12 1 7.7 1 3.99 3.47 2.18 7.07l3.66 2.84c.87-2.6 3.3-4.53 6.16-4.53z" fill="#EA4335" />
                            </svg>
                        </div>
                        <div>
                            <div className="flex items-center gap-2">
                                <h4 className="text-sm font-medium">Google</h4>
                                <span className="flex items-center gap-1 text-[10px] font-medium text-accent bg-accent/10 px-2 py-0.5 rounded-full">
                                    <CheckCircle2 size={10} /> Connected
                                </span>
                            </div>
                            <p className="text-xs text-foreground/50 font-light mt-0.5">erwan@stemhub.com</p>
                        </div>
                    </div>
                    <button className="text-xs font-medium text-foreground/50 hover:text-foreground transition-colors px-3 py-1.5 rounded-lg border border-foreground/10 hover:bg-foreground/5">
                        Disconnect
                    </button>
                </div>

                {/* GitHub disconnected state */}
                <div className="flex items-center justify-between p-5 rounded-xl border border-foreground/[0.08] bg-background-secondary/20">
                    <div className="flex items-center gap-4">
                        <div className="h-10 w-10 rounded-full bg-[#181717] flex items-center justify-center p-2 text-white">
                            <Github size={24} />
                        </div>
                        <div>
                            <h4 className="text-sm font-medium text-foreground/80">GitHub</h4>
                            <p className="text-xs text-foreground/50 font-light mt-0.5">Link repositories directly</p>
                        </div>
                    </div>
                    <button className="text-xs font-medium text-foreground transition-colors px-4 py-2 rounded-lg bg-foreground/5 hover:bg-foreground/10 border border-foreground/10">
                        Connect
                    </button>
                </div>
            </div>

            <div className="space-y-4 pt-4">
                <h3 className="text-sm font-medium text-foreground/80 mb-3">DAW Plugins</h3>

                <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                    {/* Ableton */}
                    <div className="p-5 rounded-xl border border-accent/20 bg-accent/5 flex flex-col justify-between h-32">
                        <div className="flex justify-between items-start">
                            <div className="flex items-center gap-2">
                                <div className="w-8 h-8 bg-black rounded-md flex items-center justify-center font-bold text-white text-xs border border-white/10">
                                    ///
                                </div>
                                <span className="text-sm font-medium">Ableton Live</span>
                            </div>
                            <span className="flex items-center gap-1 text-[10px] font-medium text-accent bg-accent/10 px-2 py-0.5 rounded-full">
                                <CheckCircle2 size={10} /> Active
                            </span>
                        </div>
                        <div className="flex items-center justify-between">
                            <span className="text-xs text-foreground/50 font-light">Plugin v1.2.4</span>
                            <button className="text-xs text-accent hover:underline">Configure</button>
                        </div>
                    </div>

                    {/* FL Studio */}
                    <div className="p-5 rounded-xl border border-foreground/[0.08] bg-background-secondary/20 flex flex-col justify-between h-32">
                        <div className="flex justify-between items-start">
                            <div className="flex items-center gap-2">
                                <div className="w-8 h-8 bg-[#FF6A00] rounded-md flex items-center justify-center text-white p-1">
                                    <svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-1 15h-2v-2h2v2zm0-4h-2V7h2v6z" /></svg>
                                </div>
                                <span className="text-sm font-medium text-foreground/80">FL Studio</span>
                            </div>
                            <button className="text-[10px] font-medium text-foreground/60 hover:text-foreground px-2 py-0.5 rounded border border-foreground/10 transition-colors">
                                Install
                            </button>
                        </div>
                        <div className="flex items-center justify-between opacity-50">
                            <span className="text-xs text-foreground/50 font-light">Not installed</span>
                        </div>
                    </div>
                </div>
            </div>
        </motion.div>
    );
}

function StorageSettings() {
    return (
        <motion.div
            initial={{ opacity: 0, y: 10 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -10 }}
            transition={{ duration: 0.3 }}
            className="space-y-8"
        >
            <div>
                <h2 className="text-xl font-medium tracking-tight mb-1" style={{ fontFamily: "var(--font-syne)" }}>Storage & Billing</h2>
                <p className="text-sm text-foreground/50 font-light">Manage your plan, storage limits, and payment methods.</p>
            </div>

            <div className="p-6 rounded-2xl border border-accent/20 bg-gradient-to-br from-accent/10 to-transparent">
                <div className="flex justify-between items-start mb-6">
                    <div>
                        <span className="inline-block px-3 py-1 rounded-full bg-accent/20 text-accent text-xs font-bold tracking-wider mb-2">PRO PLAN</span>
                        <h3 className="text-2xl font-medium tracking-tight" style={{ fontFamily: "var(--font-syne)" }}>$9.99<span className="text-sm text-foreground/50 font-light ml-1">/month</span></h3>
                    </div>
                    <button className="px-4 py-2 rounded-lg bg-background hover:bg-foreground/5 text-sm font-medium transition-colors border border-foreground/10">
                        Manage Plan
                    </button>
                </div>

                <div className="space-y-2">
                    <div className="flex justify-between text-sm">
                        <span className="text-foreground/70">Storage used</span>
                        <span className="font-medium">25 GB <span className="text-foreground/40 font-light">/ 100 GB</span></span>
                    </div>
                    <div className="w-full bg-background/50 rounded-full h-2 overflow-hidden border border-foreground/5">
                        <div className="bg-gradient-to-r from-accent to-purple-500 h-full rounded-full w-1/4"></div>
                    </div>
                    <p className="text-xs text-foreground/50 font-light pt-1">Your next billing date is April 4, 2026.</p>
                </div>
            </div>

            <div className="space-y-4">
                <h3 className="text-sm font-medium text-foreground/80 mb-3">Storage Breakdown</h3>

                <div className="space-y-3">
                    <div className="flex items-center justify-between p-3 rounded-lg hover:bg-foreground/5 transition-colors">
                        <div className="flex items-center gap-3">
                            <div className="w-2.5 h-2.5 rounded-full bg-accent"></div>
                            <span className="text-sm">Audio Files (.wav, .mp3)</span>
                        </div>
                        <span className="text-sm font-medium">18.2 GB</span>
                    </div>
                    <div className="flex items-center justify-between p-3 rounded-lg hover:bg-foreground/5 transition-colors">
                        <div className="flex items-center gap-3">
                            <div className="w-2.5 h-2.5 rounded-full bg-purple-500"></div>
                            <span className="text-sm">Project Files (.als, .flp)</span>
                        </div>
                        <span className="text-sm font-medium">5.1 GB</span>
                    </div>
                    <div className="flex items-center justify-between p-3 rounded-lg hover:bg-foreground/5 transition-colors">
                        <div className="flex items-center gap-3">
                            <div className="w-2.5 h-2.5 rounded-full bg-blue-500"></div>
                            <span className="text-sm">Plugin Presets & Meta</span>
                        </div>
                        <span className="text-sm font-medium">1.7 GB</span>
                    </div>
                </div>
            </div>

            <div className="pt-4 flex justify-between items-center border-t border-foreground/5">
                <span className="text-xs font-medium text-foreground/50">Need more space?</span>
                <button className="px-5 py-2.5 rounded-xl bg-foreground text-background text-sm font-medium transition-colors hover:bg-foreground/90">
                    Upgrade to Studio Plan
                </button>
            </div>
        </motion.div>
    );
}

function NotificationSettings() {
    return (
        <motion.div
            initial={{ opacity: 0, y: 10 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -10 }}
            transition={{ duration: 0.3 }}
            className="space-y-8"
        >
            <div>
                <h2 className="text-xl font-medium tracking-tight mb-1" style={{ fontFamily: "var(--font-syne)" }}>Notifications</h2>
                <p className="text-sm text-foreground/50 font-light">Choose what updates you want to receive and where.</p>
            </div>

            <div className="space-y-6">
                <div className="space-y-4">
                    <h3 className="text-sm font-medium text-foreground/80">Email Notifications</h3>

                    <div className="space-y-3">
                        <ToggleRow
                            title="New Pull Requests"
                            description="When someone opens a PR on your project"
                            defaultChecked={true}
                        />
                        <ToggleRow
                            title="Branch Updates"
                            description="When someone pushes to a branch you're collaborating on"
                            defaultChecked={false}
                        />
                        <ToggleRow
                            title="Mentions"
                            description="When someone mentions you in a comment or PR"
                            defaultChecked={true}
                        />
                        <ToggleRow
                            title="Marketing & Updates"
                            description="News about StemHub features and plugins"
                            defaultChecked={false}
                        />
                    </div>
                </div>

                <div className="pt-6 border-t border-foreground/5 space-y-4">
                    <h3 className="text-sm font-medium text-foreground/80">In-App Notifications</h3>

                    <div className="space-y-3">
                        <ToggleRow
                            title="Collaborator Activity"
                            description="Live updates when a collaborator joins a session"
                            defaultChecked={true}
                        />
                        <ToggleRow
                            title="Sync Status"
                            description="Alerts when your DAW finishes syncing a version"
                            defaultChecked={true}
                        />
                    </div>
                </div>
            </div>

            <div className="pt-6 flex justify-end">
                <button className="px-6 py-2.5 rounded-xl bg-accent hover:bg-accent/90 text-white text-sm font-medium transition-all shadow-lg shadow-accent/20">
                    Save Preferences
                </button>
            </div>
        </motion.div>
    );
}

function ToggleRow({ title, description, defaultChecked }: { title: string, description: string, defaultChecked: boolean }) {
    const [checked, setChecked] = useState(defaultChecked);

    return (
        <div className="flex items-center justify-between p-4 rounded-xl border border-foreground/[0.04] bg-background-secondary/10 hover:bg-background-secondary/30 transition-colors cursor-pointer" onClick={() => setChecked(!checked)}>
            <div>
                <h4 className="text-sm font-medium text-foreground/90">{title}</h4>
                <p className="text-xs text-foreground/50 font-light mt-0.5">{description}</p>
            </div>
            <div className={`relative inline-flex h-5 w-9 shrink-0 cursor-pointer items-center justify-center rounded-full border-2 border-transparent transition-colors duration-200 ease-in-out focus:outline-none ${checked ? 'bg-accent' : 'bg-foreground/20'}`}>
                <span className={`pointer-events-none inline-block h-4 w-4 transform rounded-full bg-white shadow ring-0 transition duration-200 ease-in-out ${checked ? 'translate-x-2' : '-translate-x-2'}`} />
            </div>
        </div>
    );
}
