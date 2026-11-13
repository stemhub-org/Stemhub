import type { MetadataRoute } from "next";
import { SITE_URL } from "@/lib/site";

// The public pages only; everything behind sign-in stays out (see robots.ts).
const PUBLIC_PATHS = ["/", "/login", "/register"];

export default function sitemap(): MetadataRoute.Sitemap {
  return PUBLIC_PATHS.map((path) => ({ url: new URL(path, SITE_URL).toString() }));
}
