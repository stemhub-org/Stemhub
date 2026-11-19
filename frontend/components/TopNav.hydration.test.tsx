import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { renderToString } from "react-dom/server";
import { describe, expect, it, vi } from "vitest";
import { ProjectHeader } from "@/app/projects/components/ProjectHeader";
import TopNav from "./TopNav";

vi.mock("next/navigation", () => ({ useRouter: () => ({ push: vi.fn() }) }));
const theme = vi.hoisted(() => ({ resolved: "light" as string | undefined }));
vi.mock("next-themes", () => ({ useTheme: () => ({ resolvedTheme: theme.resolved, setTheme: vi.fn() }) }));

// The server does not know the theme (it lives in the browser), so the first
// render must not depend on it, or hydration fails with a mismatch.
function firstRender(resolved: string | undefined, element: () => React.ReactElement): string {
    theme.resolved = resolved;
    return renderToString(<QueryClientProvider client={new QueryClient()}>{element()}</QueryClientProvider>);
}

describe("headers before hydration", () => {
    it.each([
        ["TopNav", () => <TopNav />],
        ["ProjectHeader", () => <ProjectHeader username="jb" />],
    ])("%s renders the same HTML whatever the theme", (_name, element) => {
        const server = firstRender(undefined, element);

        expect(firstRender("dark", element)).toBe(server);
        expect(firstRender("light", element)).toBe(server);
    });
});
