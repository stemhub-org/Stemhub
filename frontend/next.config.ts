import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  async redirects() {
    return [
      // The branch history page used to live at /projects/changes. Next.js
      // passes the query string (?id=…&branch_id=…) through to the destination.
      {
        source: "/projects/changes",
        destination: "/projects/history",
        permanent: true,
      },
    ];
  },
};

export default nextConfig;
