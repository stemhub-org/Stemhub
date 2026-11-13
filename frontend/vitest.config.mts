import { fileURLToPath } from "node:url";
import react from "@vitejs/plugin-react";
import { defineConfig } from "vitest/config";

export default defineConfig({
    plugins: [react()],
    resolve: {
        alias: { "@": fileURLToPath(new URL("./", import.meta.url)) },
    },
    test: {
        environment: "jsdom",
        // Let jsdom provide browser storage instead of Node's process-wide storage.
        execArgv: ["--no-webstorage"],
        setupFiles: ["./vitest.setup.ts"],
        include: ["**/*.test.{ts,tsx}"],
        exclude: ["node_modules/**", ".next/**"],
    },
});
