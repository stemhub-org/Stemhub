"use client";

import { motion, AnimatePresence } from "framer-motion";
import { Plus, X } from "lucide-react";
import { Button } from "@/components/ui/Button";
import { Input } from "@/components/ui/Input";
import { Textarea } from "@/components/ui/Textarea";

const CATEGORIES = ["General", "Electronic", "Hip-Hop", "Pop", "Rock", "Jazz", "Classical", "Other"];

interface NewProjectModalProps {
    open: boolean;
    onClose: () => void;
    name: string;
    onNameChange: (value: string) => void;
    description: string;
    onDescriptionChange: (value: string) => void;
    category: string;
    onCategoryChange: (value: string) => void;
    creating: boolean;
    onCreate: () => void;
}

export default function NewProjectModal({
    open,
    onClose,
    name,
    onNameChange,
    description,
    onDescriptionChange,
    category,
    onCategoryChange,
    creating,
    onCreate,
}: NewProjectModalProps) {
    return (
        <AnimatePresence>
            {open && (
                <>
                    <motion.div
                        initial={{ opacity: 0 }}
                        animate={{ opacity: 1 }}
                        exit={{ opacity: 0 }}
                        className="fixed inset-0 z-50 bg-black/50 backdrop-blur-sm"
                        onClick={onClose}
                    />
                    <motion.div
                        initial={{ opacity: 0, scale: 0.95, y: 20 }}
                        animate={{ opacity: 1, scale: 1, y: 0 }}
                        exit={{ opacity: 0, scale: 0.95, y: 20 }}
                        transition={{ type: "spring", damping: 25, stiffness: 300 }}
                        className="fixed inset-0 z-50 flex items-center justify-center p-4"
                        onClick={(e) => e.stopPropagation()}
                    >
                        <div className="w-full max-w-md rounded-xl bg-background-secondary dark:bg-background-tertiary border border-border-subtle p-6 shadow-2xl">
                            <div className="flex items-center justify-between mb-6">
                                <h2 className="text-lg font-medium text-foreground">New Project</h2>
                                <button
                                    onClick={onClose}
                                    className="text-foreground-muted hover:text-foreground transition-colors"
                                >
                                    <X size={20} />
                                </button>
                            </div>

                            <div className="space-y-4">
                                <div>
                                    <label className="block text-sm font-medium text-foreground mb-1.5">
                                        Project Name *
                                    </label>
                                    <Input
                                        type="text"
                                        value={name}
                                        onChange={(e) => onNameChange(e.target.value)}
                                        placeholder="My Awesome Track"
                                        autoFocus
                                    />
                                </div>
                                <div>
                                    <label className="block text-sm font-medium text-foreground mb-1.5">
                                        Description
                                    </label>
                                    <Textarea
                                        value={description}
                                        onChange={(e) => onDescriptionChange(e.target.value)}
                                        placeholder="A short description of your project"
                                        rows={3}
                                    />
                                </div>
                                <div>
                                    <label className="block text-sm font-medium text-foreground mb-1.5">
                                        Category
                                    </label>
                                    <select
                                        value={category}
                                        onChange={(e) => onCategoryChange(e.target.value)}
                                        className="w-full rounded-lg border border-border-subtle bg-background px-3 py-2.5 text-sm text-foreground focus:border-accent focus:outline-none focus:ring-1 focus:ring-accent/50 transition-colors"
                                    >
                                        {CATEGORIES.map((c) => (
                                            <option key={c} value={c}>
                                                {c}
                                            </option>
                                        ))}
                                    </select>
                                </div>
                            </div>

                            <div className="flex justify-end gap-3 mt-6">
                                <Button variant="ghost" onClick={onClose}>
                                    Cancel
                                </Button>
                                <Button
                                    variant="solid"
                                    icon={<Plus size={16} />}
                                    loading={creating}
                                    disabled={!name.trim()}
                                    onClick={onCreate}
                                >
                                    {creating ? "Creating…" : "Create Project"}
                                </Button>
                            </div>
                        </div>
                    </motion.div>
                </>
            )}
        </AnimatePresence>
    );
}
