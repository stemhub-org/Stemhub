"use client";

import { forwardRef } from "react";
import type { InputHTMLAttributes } from "react";
import { cn } from "@/lib/utils";
import { fieldClasses } from "./field";

export interface InputProps extends InputHTMLAttributes<HTMLInputElement> {
    invalid?: boolean;
}

export const Input = forwardRef<HTMLInputElement, InputProps>(({ invalid = false, className, ...rest }, ref) => {
    return (
        <input
            ref={ref}
            aria-invalid={invalid || undefined}
            className={cn(fieldClasses(invalid), "h-11", className)}
            {...rest}
        />
    );
});

Input.displayName = "Input";
