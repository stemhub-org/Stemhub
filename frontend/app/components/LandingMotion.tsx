"use client";

import type { ReactNode } from "react";
import { MotionConfig } from "framer-motion";

// Client boundary for the landing page's motion settings, so the page itself can stay a
// server component and build the isometric figures on the server.
export default function LandingMotion({ children }: { children: ReactNode }) {
  return <MotionConfig reducedMotion="user">{children}</MotionConfig>;
}
