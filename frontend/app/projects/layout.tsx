import type { ReactNode } from "react";
import { TermsGate } from "@/components/TermsGate";

export default function ProjectsLayout({ children }: { children: ReactNode }) {
    return <TermsGate>{children}</TermsGate>;
}
