"use client";

import { forwardRef } from "react";
import type { TextareaHTMLAttributes } from "react";
import { cn } from "@/lib/utils";
import { fieldClasses } from "./field";

export interface TextareaProps extends TextareaHTMLAttributes<HTMLTextAreaElement> {
    invalid?: boolean;
}

export const Textarea = forwardRef<HTMLTextAreaElement, TextareaProps>(({ invalid = false, className, ...rest }, ref) => {
    return (
        <textarea
            ref={ref}
            aria-invalid={invalid || undefined}
            className={cn(fieldClasses(invalid), "resize-none py-3", className)}
            {...rest}
        />
    );
});

Textarea.displayName = "Textarea";
