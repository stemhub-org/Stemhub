"use client";

import { createContext, useCallback, useContext, useMemo, useRef, useState } from "react";
import type { ReactNode } from "react";
import { AnimatePresence, motion } from "framer-motion";
import { CheckCircle2, AlertCircle, X } from "lucide-react";

type ToastVariant = "success" | "error";

interface ToastItem {
    id: number;
    message: string;
    variant: ToastVariant;
}

interface ToastContextValue {
    success: (message: string) => void;
    error: (message: string) => void;
}

const ToastContext = createContext<ToastContextValue | null>(null);

const AUTO_DISMISS_MS = 4000;

export function ToastProvider({ children }: { children: ReactNode }) {
    const [toasts, setToasts] = useState<ToastItem[]>([]);
    const nextId = useRef(0);
    const timers = useRef(new Map<number, ReturnType<typeof setTimeout>>());

    const dismiss = useCallback((id: number) => {
        setToasts((prev) => prev.filter((t) => t.id !== id));
        const timer = timers.current.get(id);
        if (timer) {
            clearTimeout(timer);
            timers.current.delete(id);
        }
    }, []);

    const push = useCallback((message: string, variant: ToastVariant) => {
        const id = nextId.current++;
        setToasts((prev) => [...prev, { id, message, variant }]);
        timers.current.set(id, setTimeout(() => dismiss(id), AUTO_DISMISS_MS));
    }, [dismiss]);

    const value = useMemo<ToastContextValue>(() => ({
        success: (message: string) => push(message, "success"),
        error: (message: string) => push(message, "error"),
    }), [push]);

    return (
        <ToastContext.Provider value={value}>
            {children}
            <div className="fixed bottom-6 right-6 z-[100] flex flex-col gap-2 w-full max-w-sm pointer-events-none">
                <AnimatePresence>
                    {toasts.map((t) => (
                        <motion.div
                            key={t.id}
                            initial={{ opacity: 0, y: 12, scale: 0.97 }}
                            animate={{ opacity: 1, y: 0, scale: 1 }}
                            exit={{ opacity: 0, scale: 0.97 }}
                            transition={{ duration: 0.2 }}
                            className="pointer-events-auto flex items-start gap-3 rounded-lg border border-border-subtle bg-background-secondary dark:bg-background-tertiary p-4 shadow-2xl"
                        >
                            {t.variant === "success" ? (
                                <CheckCircle2 size={18} className="text-green-500 shrink-0 mt-0.5" />
                            ) : (
                                <AlertCircle size={18} className="text-red-500 shrink-0 mt-0.5" />
                            )}
                            <p className="flex-1 text-sm text-foreground">{t.message}</p>
                            <button
                                type="button"
                                onClick={() => dismiss(t.id)}
                                aria-label="Dismiss"
                                className="text-foreground-muted hover:text-foreground transition-colors"
                            >
                                <X size={16} />
                            </button>
                        </motion.div>
                    ))}
                </AnimatePresence>
            </div>
        </ToastContext.Provider>
    );
}

export function useToast(): ToastContextValue {
    const ctx = useContext(ToastContext);
    if (!ctx) throw new Error("useToast must be used within a ToastProvider");
    return ctx;
}
