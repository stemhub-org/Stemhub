export const EASE_BRAND = [0.16, 1, 0.3, 1] as const;

export const DURATION = {
    fast: 0.2,
    base: 0.3,
    slow: 0.5,
    entrance: 0.6,
} as const;

export const transitionBase = { duration: DURATION.base, ease: EASE_BRAND };
export const transitionEntrance = { duration: DURATION.entrance, ease: EASE_BRAND };
