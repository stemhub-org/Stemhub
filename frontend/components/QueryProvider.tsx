"use client";

import { useState } from "react";
import type { ReactNode } from "react";
import { QueryClientProvider } from "@tanstack/react-query";
import { createQueryClient } from "@/lib/queryClient";

export function QueryProvider({ children }: { children: ReactNode }) {
    // One client per browser session; useState keeps it stable across renders.
    const [client] = useState(createQueryClient);
    return <QueryClientProvider client={client}>{children}</QueryClientProvider>;
}
