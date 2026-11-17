"use client";

import React, { useState, useRef } from "react";
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
    Download,
    Loader2
} from "lucide-react";
import { useToast } from "@/components/ToastProvider";
import { ErrorState } from "@/components/ui/ErrorState";
import { PasswordStrengthMeter } from "@/components/PasswordStrengthMeter";
import { PasswordVisibilityToggle } from "@/components/PasswordVisibilityToggle";
import { useChangeEmail, useChangePassword, useDeleteAccount, useUpdateProfile } from "@/lib/account";
import { apiFetchResponse, errorMessage } from "@/lib/api";
import { filenameFromContentDisposition, saveBlob } from "@/lib/download";
import { formatBytes } from "@/lib/format";
import { useCurrentUser, useStorageUsage } from "@/lib/queries";
import { useLogout } from "@/lib/useLogout";
import type { UserProfile } from "@/types/user";

const TABS = [
    { id: "profile", label: "Profile", icon: User },
    { id: "account", label: "Account", icon: Lock },
    { id: "integrations", label: "Integrations", icon: LinkIcon },
    { id: "storage", label: "Storage", icon: HardDrive },
    { id: "notifications", label: "Notifications", icon: Bell },
];

export default function SettingsPage() {
    const [activeTab, setActiveTab] = useState("profile");
    const { data: user, error, refetch } = useCurrentUser();

    if (error) {
        return <ErrorState error={error} title="Couldn't load your settings" onRetry={() => refetch()} className="max-w-xl mx-auto mt-12" />;
    }

    if (!user) {
        return (
            <div className="flex h-[50vh] items-center justify-center">
                <Loader2 size={32} className="animate-spin text-accent" />
            </div>
        );
    }

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
                    Manage your account, integrations, and storage.
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
                    className="flex-1 rounded-2xl border border-foreground/[0.08] bg-background-secondary/30 dark:bg-background-tertiary/30 backdrop-blur-xl p-8 min-h-[500px]"
                >
                    <AnimatePresence mode="wait">
                        {activeTab === "profile" && <ProfileSettings key="profile" user={user} />}
                        {activeTab === "account" && <AccountSettings key="account" user={user} />}
                        {activeTab === "integrations" && <IntegrationSettings key="integrations" user={user} />}
                        {activeTab === "storage" && <StorageSettings key="storage" />}
                        {activeTab === "notifications" && <NotificationSettings key="notifications" />}
                    </AnimatePresence>
                </motion.div>
            </div>
        </div>
    );
}

/** Empty text fields are saved as "not set". */
function orNull(value: string): string | null {
    const trimmed = value.trim();
    return trimmed ? trimmed : null;
}

function ProfileSettings({ user }: { user: UserProfile }) {
    const [form, setForm] = useState({
        username: user.username,
        avatar_url: user.avatar_url ?? "",
        bio: user.bio ?? "",
        location: user.location ?? "",
        website: user.website ?? ""
    });
    const updateProfile = useUpdateProfile();
    const toast = useToast();
    const isSaving = updateProfile.isPending;
    const fileInputRef = useRef<HTMLInputElement>(null);

    const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
        const file = e.target.files?.[0];
        if (file) {
            const reader = new FileReader();
            reader.onloadend = () => {
                setForm(f => ({ ...f, avatar_url: reader.result as string }));
            };
            reader.readAsDataURL(file);
        }
    };

    const handleSave = () => {
        updateProfile.mutate(
            {
                username: form.username.trim(),
                avatar_url: orNull(form.avatar_url),
                bio: orNull(form.bio),
                location: orNull(form.location),
                website: orNull(form.website),
            },
            {
                onSuccess: () => toast.success("Profile saved."),
                onError: (error) => toast.error(errorMessage(error, "Couldn't save your profile.")),
            },
        );
    };

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
                <input
                    type="file"
                    ref={fileInputRef}
                    onChange={handleFileChange}
                    accept="image/*"
                    className="hidden"
                />
                <div onClick={() => fileInputRef.current?.click()} className="h-24 w-24 rounded-full bg-gradient-to-br from-accent/20 to-accent/5 border border-accent/20 flex flex-col items-center justify-center relative overflow-hidden group cursor-pointer bg-cover bg-center" style={{ backgroundImage: form.avatar_url ? `url(${form.avatar_url})` : 'none' }}>
                    {!form.avatar_url && <User size={32} className="text-accent/50 group-hover:opacity-0 transition-opacity" />}
                    <div className="absolute inset-0 bg-background/60 flex flex-col items-center justify-center opacity-0 group-hover:opacity-100 transition-opacity">
                        <UploadCloud size={20} className="text-white mb-1" />
                        <span className="text-[10px] font-medium text-white">Upload</span>
                    </div>
                </div>
                <div>
                    <h3 className="text-sm font-medium mb-1">Profile Picture</h3>
                    <p className="text-xs text-foreground/50 font-light mb-3">JPEG, PNG, or WEBP. Max 2MB.</p>
                    <div className="flex gap-3">
                        <button onClick={() => fileInputRef.current?.click()} className="px-4 py-2 rounded-lg bg-foreground/5 hover:bg-foreground/10 text-xs font-medium transition-colors border border-foreground/10">
                            Change
                        </button>
                        <button onClick={() => setForm(f => ({ ...f, avatar_url: "" }))} className="px-4 py-2 rounded-lg text-red-400 hover:bg-red-400/10 text-xs font-medium transition-colors">
                            Remove
                        </button>
                    </div>
                </div>
            </div>

            <div className="space-y-5">
                <div className="space-y-2">
                    <label htmlFor="settings-username" className="text-sm font-medium text-foreground/80">Username</label>
                    <input
                        type="text"
                        id="settings-username"
                        value={form.username}
                        onChange={e => setForm(f => ({ ...f, username: e.target.value }))}
                        className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 dark:bg-background-tertiary/50 py-3 px-4 text-sm font-light text-foreground focus:border-accent/40 focus:outline-none transition-all"
                    />
                </div>
                <div className="space-y-2">
                    <label htmlFor="settings-location" className="text-sm font-medium text-foreground/80">Location</label>
                    <input
                        type="text"
                        id="settings-location"
                        value={form.location}
                        onChange={e => setForm(f => ({ ...f, location: e.target.value }))}
                        placeholder="e.g. Paris, France"
                        className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 dark:bg-background-tertiary/50 py-3 px-4 text-sm font-light text-foreground focus:border-accent/40 focus:outline-none transition-all"
                    />
                </div>
                <div className="space-y-2">
                    <label htmlFor="settings-website" className="text-sm font-medium text-foreground/80">Website</label>
                    <input
                        type="url"
                        id="settings-website"
                        value={form.website}
                        onChange={e => setForm(f => ({ ...f, website: e.target.value }))}
                        placeholder="https://yourwebsite.com"
                        className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 dark:bg-background-tertiary/50 py-3 px-4 text-sm font-light text-foreground focus:border-accent/40 focus:outline-none transition-all"
                    />
                </div>
                <div className="space-y-2">
                    <label htmlFor="settings-bio" className="text-sm font-medium text-foreground/80">Bio</label>
                    <textarea
                        rows={4}
                        id="settings-bio"
                        value={form.bio}
                        onChange={e => setForm(f => ({ ...f, bio: e.target.value }))}
                        placeholder="Electronic music producer..."
                        className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 dark:bg-background-tertiary/50 py-3 px-4 text-sm font-light text-foreground focus:border-accent/40 focus:outline-none transition-all resize-none"
                    />
                </div>
            </div>

            <div className="pt-4 flex justify-end">
                <button onClick={handleSave} disabled={isSaving} className="flex items-center gap-2 px-6 py-2.5 rounded-xl bg-accent hover:bg-accent/90 text-accent-foreground text-sm font-medium transition-all shadow-lg shadow-accent/20">
                    {isSaving ? <Loader2 size={16} className="animate-spin" /> : <Save size={16} />}
                    Save Changes
                </button>
            </div>
        </motion.div>
    );
}

function AccountSettings({ user }: { user: UserProfile }) {
    const [emailForm, setEmailForm] = useState(user.email);
    const changeEmail = useChangeEmail();
    const toast = useToast();
    const isUpdatingEmail = changeEmail.isPending;
    
    const [passwordForm, setPasswordForm] = useState({
        current_password: "",
        new_password: "",
        confirm_password: ""
    });
    const changePassword = useChangePassword();
    const isUpdatingPassword = changePassword.isPending;
    const [showCurrentPassword, setShowCurrentPassword] = useState(false);
    // One toggle for the new password and its confirmation, as on sign-up.
    const [showNewPassword, setShowNewPassword] = useState(false);
    const [passwordError, setPasswordError] = useState("");
    const [passwordSuccess, setPasswordSuccess] = useState("");

    const handleUpdateEmail = () => {
        const newEmail = emailForm.trim();
        if (!newEmail || newEmail === user.email) return;
        changeEmail.mutate(
            { new_email: newEmail },
            {
                onSuccess: () => toast.success("Email updated."),
                onError: (error) => toast.error(errorMessage(error, "Couldn't change your email.")),
            },
        );
    };

    const handleUpdatePassword = () => {
        setPasswordError("");
        setPasswordSuccess("");
        
        if (!passwordForm.current_password || !passwordForm.new_password || !passwordForm.confirm_password) {
            setPasswordError("Please fill in all fields.");
            return;
        }
        
        if (passwordForm.new_password !== passwordForm.confirm_password) {
            setPasswordError("The new passwords do not match.");
            return;
        }

        changePassword.mutate(
            { current_password: passwordForm.current_password, new_password: passwordForm.new_password },
            {
                onSuccess: () => {
                    setPasswordSuccess("Password updated.");
                    setPasswordForm({ current_password: "", new_password: "", confirm_password: "" });
                },
                onError: (error) => setPasswordError(errorMessage(error, "Couldn't change your password.")),
            },
        );
    };

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
                    <label htmlFor="settings-email" className="text-sm font-medium text-foreground/80">Email Address</label>
                    <div className="flex gap-4">
                        <input
                            type="email"
                            id="settings-email"
                            value={emailForm}
                            onChange={(e) => setEmailForm(e.target.value)}
                            className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 dark:bg-background-tertiary/50 py-3 px-4 text-sm font-light text-foreground focus:border-accent/40 focus:outline-none transition-all"
                        />
                        <button 
                            onClick={handleUpdateEmail}
                            disabled={isUpdatingEmail || emailForm.trim() === user.email}
                            className="shrink-0 px-4 py-3 rounded-xl bg-foreground/5 hover:bg-foreground/10 text-sm font-medium transition-colors border border-foreground/10 disabled:opacity-50"
                        >
                            {isUpdatingEmail ? <Loader2 size={16} className="animate-spin" /> : "Change"}
                        </button>
                    </div>
                </div>

                <div className="space-y-2 pt-2">
                    <label htmlFor="settings-current-password" className="text-sm font-medium text-foreground/80">Current Password</label>
                    <div className="relative">
                        <input
                            type={showCurrentPassword ? "text" : "password"}
                            placeholder="••••••••"
                            id="settings-current-password"
                            autoComplete="current-password"
                            value={passwordForm.current_password}
                            onChange={(e) => setPasswordForm(f => ({ ...f, current_password: e.target.value }))}
                            className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 dark:bg-background-tertiary/50 py-3 pl-4 pr-11 text-sm font-light text-foreground focus:border-accent/40 focus:outline-none transition-all"
                        />
                        <PasswordVisibilityToggle
                            visible={showCurrentPassword}
                            onToggle={() => setShowCurrentPassword(!showCurrentPassword)}
                        />
                    </div>
                </div>
                <div className="grid grid-cols-1 md:grid-cols-2 gap-5">
                    <div className="space-y-2">
                        <label htmlFor="settings-new-password" className="text-sm font-medium text-foreground/80">New Password</label>
                        <div className="relative">
                            <input
                                type={showNewPassword ? "text" : "password"}
                                placeholder="••••••••"
                                id="settings-new-password"
                                autoComplete="new-password"
                                aria-describedby="settings-new-password-strength"
                                value={passwordForm.new_password}
                                onChange={(e) => setPasswordForm(f => ({ ...f, new_password: e.target.value }))}
                                className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 dark:bg-background-tertiary/50 py-3 pl-4 pr-11 text-sm font-light text-foreground focus:border-accent/40 focus:outline-none transition-all"
                            />
                            <PasswordVisibilityToggle
                                visible={showNewPassword}
                                onToggle={() => setShowNewPassword(!showNewPassword)}
                            />
                        </div>
                        <PasswordStrengthMeter
                            id="settings-new-password-strength"
                            password={passwordForm.new_password}
                            userInputs={[user.username, user.email]}
                        />
                    </div>
                    <div className="space-y-2">
                        <label htmlFor="settings-confirm-password" className="text-sm font-medium text-foreground/80">Confirm New Password</label>
                        <input
                            type={showNewPassword ? "text" : "password"}
                            placeholder="••••••••"
                            id="settings-confirm-password"
                            autoComplete="new-password"
                            value={passwordForm.confirm_password}
                            onChange={(e) => setPasswordForm(f => ({ ...f, confirm_password: e.target.value }))}
                            className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 dark:bg-background-tertiary/50 py-3 px-4 text-sm font-light text-foreground focus:border-accent/40 focus:outline-none transition-all"
                        />
                    </div>
                </div>
                
                {passwordError && (
                    <p role="alert" className="text-sm text-red-500 font-medium">{passwordError}</p>
                )}
                {passwordSuccess && (
                    <p role="status" className="text-sm text-green-500 font-medium">{passwordSuccess}</p>
                )}

                <div className="pt-2">
                    <button 
                        onClick={handleUpdatePassword}
                        disabled={isUpdatingPassword}
                        className="px-6 py-2.5 rounded-xl bg-foreground/5 hover:bg-foreground/10 text-sm font-medium transition-colors border border-foreground/10 text-foreground flex items-center gap-2 disabled:opacity-50"
                    >
                        {isUpdatingPassword ? <Loader2 size={16} className="animate-spin" /> : "Update Password"}
                    </button>
                </div>
            </div>

            <ExportData />

            <DeleteAccount user={user} />
        </motion.div>
    );
}

function ExportData() {
    const [exporting, setExporting] = useState(false);
    const toast = useToast();

    const handleExport = async () => {
        setExporting(true);
        try {
            const response = await apiFetchResponse("/auth/me/export");
            const filename = filenameFromContentDisposition(response.headers.get("Content-Disposition")) ?? "stemhub-data.json";
            saveBlob(await response.blob(), filename);
        } catch (error) {
            toast.error(errorMessage(error, "Couldn't export your data."));
        } finally {
            setExporting(false);
        }
    };

    return (
        <div className="space-y-4">
            <h2 className="text-xl font-medium tracking-tight" style={{ fontFamily: "var(--font-syne)" }}>Your Data</h2>
            <div className="p-5 rounded-xl border border-foreground/[0.08] bg-background-secondary/30 dark:bg-background-tertiary/30 flex flex-col md:flex-row items-start md:items-center justify-between gap-4">
                <p className="text-xs text-foreground/60 font-light max-w-md">
                    Download a copy of everything StemHub holds about you, as a JSON file: your profile, your
                    projects and their history, your contributions and your sign-in history. Audio files are
                    listed, and can be downloaded from each project.
                </p>
                <button
                    type="button"
                    onClick={handleExport}
                    disabled={exporting}
                    className="shrink-0 flex items-center gap-2 px-5 py-2.5 rounded-xl bg-foreground/5 hover:bg-foreground/10 text-sm font-medium transition-colors border border-foreground/10 disabled:opacity-50"
                >
                    {exporting ? <Loader2 size={16} className="animate-spin" /> : <Download size={16} />}
                    Download my data
                </button>
            </div>
        </div>
    );
}

const DELETION_GRACE_DAYS = 30;

function DeleteAccount({ user }: { user: UserProfile }) {
    const [confirming, setConfirming] = useState(false);
    const [typedUsername, setTypedUsername] = useState("");
    const [password, setPassword] = useState("");
    const [showPassword, setShowPassword] = useState(false);
    const deleteAccount = useDeleteAccount();
    const logout = useLogout();
    const toast = useToast();

    const handleDelete = () => {
        deleteAccount.mutate(
            { confirm_username: typedUsername, password: password || undefined },
            {
                onSuccess: ({ erased_at }) => {
                    const date = new Date(erased_at).toLocaleDateString("en-US", { day: "numeric", month: "long", year: "numeric" });
                    toast.success(`Your account will be deleted on ${date}. Sign in before then to cancel.`);
                    void logout();
                },
            },
        );
    };

    return (
        <div className="space-y-4">
            <div className="flex items-center gap-3 text-red-500">
                <ShieldAlert size={20} />
                <h2 className="text-xl font-medium tracking-tight" style={{ fontFamily: "var(--font-syne)" }}>Danger Zone</h2>
            </div>
            <div className="p-5 rounded-xl border border-red-500/20 bg-red-500/5 space-y-4">
                <div className="flex flex-col md:flex-row items-start md:items-center justify-between gap-4">
                    <div>
                        <h3 className="text-sm font-medium text-red-400 mb-1">Delete Account</h3>
                        <p className="text-xs text-foreground/60 font-light">
                            Remove your account and all your projects. You have {DELETION_GRACE_DAYS} days to change your mind.
                        </p>
                    </div>
                    {!confirming && (
                        <button
                            type="button"
                            onClick={() => setConfirming(true)}
                            className="shrink-0 flex items-center gap-2 px-5 py-2.5 rounded-xl bg-red-500/10 hover:bg-red-500/20 text-red-500 text-sm font-medium transition-colors border border-red-500/20"
                        >
                            <Trash2 size={16} /> Delete Account
                        </button>
                    )}
                </div>

                {confirming && (
                    <div className="space-y-4 border-t border-red-500/20 pt-4">
                        <ul className="list-disc space-y-1 pl-5 text-xs font-light text-foreground/70">
                            <li>You are signed out everywhere, and your profile and projects are hidden at once.</li>
                            <li>
                                After {DELETION_GRACE_DAYS} days, your account and your projects, with their files, are
                                deleted for good.
                            </li>
                            <li>Your versions and pull requests in other people&apos;s projects stay, without your name.</li>
                            <li>Signing in again within {DELETION_GRACE_DAYS} days cancels the deletion.</li>
                        </ul>
                        <div className="space-y-2">
                            <label htmlFor="delete-confirm-username" className="text-sm font-medium text-foreground/80">
                                Type your username, <span className="font-semibold">{user.username}</span>, to confirm
                            </label>
                            <input
                                id="delete-confirm-username"
                                autoComplete="off"
                                value={typedUsername}
                                onChange={(e) => setTypedUsername(e.target.value)}
                                className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 dark:bg-background-tertiary/50 py-3 px-4 text-sm font-light text-foreground focus:border-red-500/40 focus:outline-none transition-all"
                            />
                        </div>
                        <div className="space-y-2">
                            <label htmlFor="delete-password" className="text-sm font-medium text-foreground/80">Password</label>
                            <div className="relative">
                                <input
                                    id="delete-password"
                                    type={showPassword ? "text" : "password"}
                                    autoComplete="current-password"
                                    aria-describedby="delete-password-hint"
                                    value={password}
                                    onChange={(e) => setPassword(e.target.value)}
                                    className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 dark:bg-background-tertiary/50 py-3 pl-4 pr-11 text-sm font-light text-foreground focus:border-red-500/40 focus:outline-none transition-all"
                                />
                                <PasswordVisibilityToggle visible={showPassword} onToggle={() => setShowPassword(!showPassword)} />
                            </div>
                            <p id="delete-password-hint" className="text-xs font-light text-foreground/40">
                                Leave empty if you only sign in with Google.
                            </p>
                        </div>
                        {deleteAccount.error && (
                            <p role="alert" className="text-sm text-red-500 font-medium">
                                {errorMessage(deleteAccount.error, "Couldn't delete your account.")}
                            </p>
                        )}
                        <div className="flex flex-wrap gap-3">
                            <button
                                type="button"
                                onClick={handleDelete}
                                disabled={typedUsername !== user.username || deleteAccount.isPending}
                                className="flex items-center gap-2 px-5 py-2.5 rounded-xl bg-red-500 hover:bg-red-600 text-white text-sm font-medium transition-colors disabled:opacity-40 disabled:cursor-not-allowed"
                            >
                                {deleteAccount.isPending ? <Loader2 size={16} className="animate-spin" /> : <Trash2 size={16} />}
                                Delete my account
                            </button>
                            <button
                                type="button"
                                onClick={() => {
                                    setConfirming(false);
                                    setTypedUsername("");
                                    setPassword("");
                                    deleteAccount.reset();
                                }}
                                className="px-5 py-2.5 rounded-xl text-sm font-medium text-foreground/60 hover:text-foreground transition-colors"
                            >
                                Cancel
                            </button>
                        </div>
                    </div>
                )}
            </div>
        </div>
    );
}

function IntegrationSettings({ user }: { user: UserProfile }) {
    // From GET /auth/me; an older backend omits it, which reads as not linked.
    const googleLinked = user.linked_providers?.includes("google") ?? false;

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
                <p className="text-sm text-foreground/50 font-light">The accounts you sign in with and the StemHub plugin.</p>
            </div>

            <div className="space-y-4">
                <h3 className="text-sm font-medium text-foreground/80 mb-3">Authentication</h3>

                <div className={`flex items-center gap-4 p-5 rounded-xl border ${googleLinked ? "border-accent/20 bg-accent/5" : "border-foreground/[0.08] bg-background-secondary/20 dark:bg-background-tertiary/20"}`}>
                    <div className="h-10 w-10 shrink-0 rounded-full bg-white flex items-center justify-center p-2 shadow-sm">
                        <svg viewBox="0 0 24 24" className="w-full h-full" aria-hidden="true">
                            <path d="M22.56 12.25c0-.78-.07-1.53-.2-2.25H12v4.26h5.92a5.06 5.06 0 0 1-2.2 3.32v2.77h3.57c2.08-1.92 3.28-4.74 3.28-8.1z" fill="#4285F4" />
                            <path d="M12 23c2.97 0 5.46-.98 7.28-2.66l-3.57-2.77c-.98.66-2.23 1.06-3.71 1.06-2.86 0-5.29-1.93-6.16-4.53H2.18v2.84C3.99 20.53 7.7 23 12 23z" fill="#34A853" />
                            <path d="M5.84 14.09c-.22-.66-.35-1.36-.35-2.09s.13-1.43.35-2.09V7.07H2.18C1.43 8.55 1 10.22 1 12s.43 3.45 1.18 4.93l2.85-2.22.81-.62z" fill="#FBBC05" />
                            <path d="M12 5.38c1.62 0 3.06.56 4.21 1.64l3.15-3.15C17.45 2.09 14.97 1 12 1 7.7 1 3.99 3.47 2.18 7.07l3.66 2.84c.87-2.6 3.3-4.53 6.16-4.53z" fill="#EA4335" />
                        </svg>
                    </div>
                    <div>
                        <div className="flex items-center gap-2">
                            <h4 className="text-sm font-medium">Google</h4>
                            {googleLinked ? (
                                <span className="flex items-center gap-1 text-[10px] font-medium text-accent bg-accent/10 px-2 py-0.5 rounded-full">
                                    <CheckCircle2 size={10} /> Linked
                                </span>
                            ) : (
                                <span className="text-[10px] font-medium text-foreground/60 bg-foreground/5 px-2 py-0.5 rounded-full">Not linked</span>
                            )}
                        </div>
                        <p className="text-xs text-foreground/50 font-light mt-0.5">
                            {googleLinked
                                ? "You can sign in with Google."
                                : `Sign in with Google using ${user.email} to link it.`}
                        </p>
                    </div>
                </div>
            </div>

            <div className="space-y-4 pt-4">
                <h3 className="text-sm font-medium text-foreground/80 mb-3">StemHub plugin</h3>

                <div className="p-5 rounded-xl border border-foreground/[0.08] bg-background-secondary/20 dark:bg-background-tertiary/20 space-y-2">
                    <div className="flex items-center gap-2">
                        <span className="text-sm font-medium">FL Studio plugin</span>
                        <ComingSoonBadge />
                    </div>
                    <p className="text-xs text-foreground/50 font-light">
                        The installer is on its way. This page will offer the download as soon as it is ready.
                    </p>
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
                <h2 className="text-xl font-medium tracking-tight mb-1" style={{ fontFamily: "var(--font-syne)" }}>Storage</h2>
                <p className="text-sm text-foreground/50 font-light">What StemHub keeps for your projects.</p>
            </div>

            <div className="p-6 rounded-2xl border border-accent/20 bg-gradient-to-br from-accent/10 to-transparent">
                <span className="inline-block px-3 py-1 rounded-full bg-accent/20 text-accent text-xs font-bold tracking-wider mb-2">BETA</span>
                <h3 className="text-2xl font-medium tracking-tight mb-2" style={{ fontFamily: "var(--font-syne)" }}>Free during the beta</h3>
                <p className="text-sm text-foreground/60 font-light">
                    Storage is free while StemHub is in beta. No plan, no payment details.
                </p>
            </div>

            <StorageUsageList />
        </motion.div>
    );
}

function StorageUsageList() {
    const { data: usage, error, refetch } = useStorageUsage();

    if (error) {
        return <ErrorState error={error} title="Couldn't load your storage" onRetry={() => refetch()} />;
    }
    if (!usage) {
        return <Loader2 size={20} className="animate-spin text-accent" />;
    }

    return (
        <div className="space-y-4">
            <div className="flex justify-between items-baseline">
                <h3 className="text-sm font-medium text-foreground/80">Used by your projects</h3>
                <span className="text-lg font-medium">{formatBytes(usage.total_bytes)}</span>
            </div>

            {usage.projects.length === 0 ? (
                <p className="text-sm text-foreground/50 font-light">No project yet.</p>
            ) : (
                <ul className="space-y-1">
                    {usage.projects.map((project) => (
                        <li key={project.id} className="flex items-center justify-between gap-4 p-3 rounded-lg hover:bg-foreground/5 transition-colors">
                            <span className="text-sm truncate">{project.name}</span>
                            <span className="flex items-center gap-4 shrink-0">
                                <span className="text-xs text-foreground/50 font-light">
                                    {project.file_count === 1 ? "1 file" : `${project.file_count} files`}
                                </span>
                                <span className="text-sm font-medium w-20 text-right">{formatBytes(project.size_bytes)}</span>
                            </span>
                        </li>
                    ))}
                </ul>
            )}

            <p className="text-xs text-foreground/50 font-light">
                Counts the files saved in your projects&apos; versions. A file used by several versions of a project is counted once.
            </p>
        </div>
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

            <div className="p-5 rounded-xl border border-foreground/[0.08] bg-background-secondary/20 dark:bg-background-tertiary/20 space-y-2">
                <ComingSoonBadge />
                <p className="text-sm text-foreground/60 font-light">
                    Email and in-app notifications are not available yet. You will be able to choose them here.
                </p>
            </div>
        </motion.div>
    );
}

function ComingSoonBadge() {
    return (
        <span className="inline-block text-[10px] font-medium text-foreground/60 bg-foreground/5 px-2 py-0.5 rounded-full">
            Coming soon
        </span>
    );
}
