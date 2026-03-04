declare module "next-themes" {
  import type React from "react";

  export interface ThemeProviderProps {
    children: React.ReactNode;
    attribute?: string;
    defaultTheme?: string;
    value?: Record<string, string>;
    enableSystem?: boolean;
    disableTransitionOnChange?: boolean;
  }

  export function useTheme(): {
    theme?: string;
    setTheme: (theme: string) => void;
    resolvedTheme?: string;
    systemTheme?: string;
  };

  export const ThemeProvider: React.ComponentType<ThemeProviderProps>;
}

