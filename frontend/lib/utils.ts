/**
 * Joins class name fragments, skipping falsy values. Does NOT resolve
 * conflicting Tailwind utilities — callers must not pass a className that
 * overrides a property the base recipe already sets for the same element.
 */
export function cn(...inputs: Array<string | false | null | undefined>): string {
    return inputs.filter(Boolean).join(" ");
}
