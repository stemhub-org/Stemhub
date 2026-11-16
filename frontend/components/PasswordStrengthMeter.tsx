"use client";

import { useDeferredValue, useEffect, useState } from "react";
import {
    loadPasswordStrength,
    type PasswordStrength,
    type PasswordStrengthChecker,
    type StrengthScore,
} from "@/lib/passwordStrength";

const LEVELS: Record<StrengthScore, { label: string; color: string }> = {
    0: { label: "Very weak", color: "bg-red-500" },
    1: { label: "Weak", color: "bg-orange-500" },
    2: { label: "Fair", color: "bg-yellow-500" },
    3: { label: "Good", color: "bg-green-500" },
    4: { label: "Strong", color: "bg-green-500" },
};

interface Props {
    password: string;
    /** Things the password should not be built from, such as the username or email. */
    userInputs?: string[];
    id?: string;
}

/** Shows how hard the password being typed would be to guess, with zxcvbn's advice. */
export function PasswordStrengthMeter({ password, userInputs = [], id }: Props) {
    const [check, setCheck] = useState<PasswordStrengthChecker | null>(null);
    const deferredPassword = useDeferredValue(password);
    const inputsKey = userInputs.join("\n");

    useEffect(() => {
        let cancelled = false;
        loadPasswordStrength()
            .then((loaded) => {
                if (!cancelled) setCheck(() => loaded);
            })
            .catch(() => undefined); // The form works without the meter.
        return () => {
            cancelled = true;
        };
    }, []);

    if (!deferredPassword || !check) return null;

    const strength: PasswordStrength = check(deferredPassword, inputsKey.split("\n"));
    const level = LEVELS[strength.score];
    const advice = strength.warning ?? strength.suggestions[0] ?? null;
    // A very weak password still lights one segment, in red.
    const filled = Math.max(strength.score, 1);

    return (
        <div id={id} aria-live="polite" className="space-y-1.5 pl-1" style={{ fontFamily: "var(--font-jakarta)" }}>
            <div className="flex gap-1" aria-hidden="true">
                {[1, 2, 3, 4].map((segment) => (
                    <div
                        key={segment}
                        data-testid="strength-segment"
                        className={`h-1 flex-1 rounded-full transition-colors duration-300 ${
                            filled >= segment ? level.color : "bg-foreground/[0.08]"
                        }`}
                    />
                ))}
            </div>
            <p className="text-xs font-light text-foreground/50">
                Password strength: <span className="text-foreground/80">{level.label}</span>
                {advice && <span className="text-foreground/40"> · {advice}</span>}
            </p>
        </div>
    );
}
