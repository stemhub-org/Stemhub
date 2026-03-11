"use client";

import { useEffect, useState } from "react";
import { motion } from "framer-motion";
import { ShieldCheck } from "lucide-react";
import { authFetch } from "@/lib/api";

interface User {
    id: string;
    email: string;
    username: string;
    created_at: string;
    is_active: boolean;
    is_admin: boolean;
    avatar_url: string | null;
}

function formatDate(iso: string) {
    return new Date(iso).toLocaleDateString("fr-FR", {
        day: "2-digit",
        month: "short",
        year: "numeric",
    });
}

function Avatar({ user }: { user: User }) {
    if (user.avatar_url) {
        return (
            <img
                src={user.avatar_url}
                alt={user.username}
                className="w-8 h-8 rounded-full object-cover"
            />
        );
    }
    const initials = user.username.slice(0, 2).toUpperCase();
    return (
        <div className="w-8 h-8 rounded-full bg-[#9C57DF]/15 text-[#9C57DF] text-xs font-semibold flex items-center justify-center">
            {initials}
        </div>
    );
}

export default function AdminUsersPage() {
    const [users, setUsers] = useState<User[] | null>(null);
    const [error, setError] = useState<string | null>(null);
    const [search, setSearch] = useState("");

    useEffect(() => {
        authFetch<User[]>("/api/admin/users")
            .then(setUsers)
            .catch((err) => setError(err.message));
    }, []);

    const filtered = users
        ? users.filter(
              (u) =>
                  u.username.toLowerCase().includes(search.toLowerCase()) ||
                  u.email.toLowerCase().includes(search.toLowerCase())
          )
        : null;

    return (
        <div className="p-8 max-w-5xl">
            <motion.div
                initial={{ opacity: 0, y: -8 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.35 }}
                className="mb-6 flex items-center justify-between"
            >
                <div>
                    <h1 className="text-2xl font-semibold tracking-tight text-[#1A1A1A]">Utilisateurs</h1>
                    <p className="text-sm text-[#1A1A1A]/50 mt-1">
                        {users ? `${users.length} compte${users.length > 1 ? "s" : ""} inscrits` : "Chargement…"}
                    </p>
                </div>
                <input
                    type="text"
                    placeholder="Rechercher…"
                    value={search}
                    onChange={(e) => setSearch(e.target.value)}
                    className="text-sm px-3 py-2 rounded-lg border border-[#1A1A1A]/15 bg-white text-[#1A1A1A] placeholder:text-[#1A1A1A]/30 outline-none focus:ring-2 focus:ring-[#9C57DF]/30 w-52"
                />
            </motion.div>

            {error && (
                <div className="mb-6 px-4 py-3 rounded-xl border border-red-200 bg-red-50 text-sm text-red-600">
                    {error}
                </div>
            )}

            <motion.div
                initial={{ opacity: 0, y: 12 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.4, delay: 0.08 }}
                className="rounded-2xl border border-[#1A1A1A]/10 bg-white overflow-hidden"
                style={{ boxShadow: "0 1px 4px 0 rgba(26,26,26,0.06)" }}
            >
                <table className="w-full text-sm">
                    <thead>
                        <tr className="border-b border-[#1A1A1A]/8">
                            <th className="text-left px-5 py-3.5 font-medium text-[#1A1A1A]/40 text-xs uppercase tracking-wider">Utilisateur</th>
                            <th className="text-left px-5 py-3.5 font-medium text-[#1A1A1A]/40 text-xs uppercase tracking-wider">Email</th>
                            <th className="text-left px-5 py-3.5 font-medium text-[#1A1A1A]/40 text-xs uppercase tracking-wider">Inscrit le</th>
                            <th className="text-left px-5 py-3.5 font-medium text-[#1A1A1A]/40 text-xs uppercase tracking-wider">Statut</th>
                        </tr>
                    </thead>
                    <tbody className="divide-y divide-[#1A1A1A]/6">
                        {filtered === null
                            ? Array.from({ length: 5 }).map((_, i) => (
                                  <tr key={i}>
                                      <td className="px-5 py-3.5">
                                          <div className="flex items-center gap-3">
                                              <div className="w-8 h-8 rounded-full bg-[#1A1A1A]/8 animate-pulse" />
                                              <div className="w-24 h-3.5 rounded bg-[#1A1A1A]/8 animate-pulse" />
                                          </div>
                                      </td>
                                      <td className="px-5 py-3.5"><div className="w-36 h-3.5 rounded bg-[#1A1A1A]/8 animate-pulse" /></td>
                                      <td className="px-5 py-3.5"><div className="w-20 h-3.5 rounded bg-[#1A1A1A]/8 animate-pulse" /></td>
                                      <td className="px-5 py-3.5"><div className="w-16 h-5 rounded-full bg-[#1A1A1A]/8 animate-pulse" /></td>
                                  </tr>
                              ))
                            : filtered.map((user) => (
                                  <tr key={user.id} className="hover:bg-[#1A1A1A]/2 transition-colors">
                                      <td className="px-5 py-3.5">
                                          <div className="flex items-center gap-3">
                                              <Avatar user={user} />
                                              <span className="font-medium text-[#1A1A1A]">{user.username}</span>
                                              {user.is_admin && (
                                                  <ShieldCheck size={13} className="text-[#9C57DF]" aria-label="Administrateur" />
                                              )}
                                          </div>
                                      </td>
                                      <td className="px-5 py-3.5 text-[#1A1A1A]/60">{user.email}</td>
                                      <td className="px-5 py-3.5 text-[#1A1A1A]/50">{formatDate(user.created_at)}</td>
                                      <td className="px-5 py-3.5">
                                          <span
                                              className={`inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium ${
                                                  user.is_active
                                                      ? "bg-emerald-50 text-emerald-600"
                                                      : "bg-[#1A1A1A]/6 text-[#1A1A1A]/40"
                                              }`}
                                          >
                                              {user.is_active ? "Actif" : "Inactif"}
                                          </span>
                                      </td>
                                  </tr>
                              ))}
                        {filtered !== null && filtered.length === 0 && (
                            <tr>
                                <td colSpan={4} className="px-5 py-10 text-center text-sm text-[#1A1A1A]/30">
                                    Aucun résultat pour « {search} »
                                </td>
                            </tr>
                        )}
                    </tbody>
                </table>
            </motion.div>
        </div>
    );
}
