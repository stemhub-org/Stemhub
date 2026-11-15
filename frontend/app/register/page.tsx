"use client";

import { useState } from "react";
import { motion } from "framer-motion";
import { Mail, Lock, User } from "lucide-react";
import { PasswordVisibilityToggle } from "@/components/PasswordVisibilityToggle";
import Link from "next/link";
import { useRouter } from "next/navigation";
import { PasswordStrengthMeter } from "@/components/PasswordStrengthMeter";
import { API_URL, apiFetch, errorMessage } from "@/lib/api";
import {
  PASSWORD_MIN_LENGTH,
  USERNAME_MAX_LENGTH,
  USERNAME_MIN_LENGTH,
  newPasswordError,
  usernameError,
} from "@/lib/validation";

export default function RegisterPage() {
  const [showPassword, setShowPassword] = useState(false);
  const [username, setUsername] = useState("");
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [confirmPassword, setConfirmPassword] = useState("");
  const [acceptTerms, setAcceptTerms] = useState(false);
  const [error, setError] = useState("");
  const router = useRouter();

  const handleRegister = async (e: React.FormEvent) => {
    e.preventDefault();
    setError("");

    const invalid =
      usernameError(username) ??
      newPasswordError(password) ??
      (password !== confirmPassword ? "Passwords do not match" : null) ??
      (acceptTerms ? null : "Please accept the Terms of Service and the Privacy Policy.");
    if (invalid) {
      setError(invalid);
      return;
    }

    try {
      await apiFetch("/auth/register", {
        method: "POST",
        body: JSON.stringify({ username, email, password, accept_terms: acceptTerms }),
        redirectOnUnauthorized: false,
      });
      router.push("/login");
    } catch (err) {
      setError(errorMessage(err, "Error while creating your account"));
    }
  };

  return (
    <div className="flex min-h-screen items-center justify-center px-6">
      <div className="pointer-events-none fixed top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2">
        <div
          className="h-[800px] w-[800px] rounded-full opacity-20 blur-[150px]"
          style={{
            background:
              "radial-gradient(circle, rgba(200,255,0,0.3) 0%, transparent 70%)",
          }}
        />
      </div>

      <motion.div
        className="relative z-10 w-full max-w-md"
        initial={{ opacity: 0, y: 30 }}
        animate={{ opacity: 1, y: 0 }}
        transition={{ duration: 0.8, ease: [0.16, 1, 0.3, 1] }}
      >
        <Link href="/" className="mb-12 block text-center">
          <span
            className="text-2xl font-medium tracking-tight"
            style={{ fontFamily: "var(--font-syne)" }}
          >
            stemhub<span className="text-accent">.</span>
          </span>
        </Link>

        <div className="mb-10 text-center">
          <h1
            className="mb-3 text-3xl font-extralight tracking-tight"
            style={{ fontFamily: "var(--font-syne)" }}
          >
            Create an account
          </h1>
          <p
            className="text-sm font-light text-foreground/50"
            style={{ fontFamily: "var(--font-jakarta)" }}
          >
            Start versioning your music projects.
          </p>
        </div>

        {error && (
          <p role="alert" className="mb-4 text-center text-sm font-light text-red-500">
            {error}
          </p>
        )}

        <button
          type="button"
          onClick={() => { window.location.href = `${API_URL}/auth/login/google`; }}
          className="flex w-full items-center justify-center gap-3 rounded-xl border border-foreground/[0.08] bg-background-secondary/50 dark:bg-background-tertiary/50 px-6 py-3.5 text-sm font-light transition-all duration-300 hover:border-foreground/20 hover:bg-background-secondary dark:hover:bg-background-tertiary"
          style={{ fontFamily: "var(--font-jakarta)" }}
        >
          <svg width="18" height="18" viewBox="0 0 24 24">
            <path
              d="M22.56 12.25c0-.78-.07-1.53-.2-2.25H12v4.26h5.92a5.06 5.06 0 0 1-2.2 3.32v2.77h3.57c2.08-1.92 3.28-4.74 3.28-8.1z"
              fill="#4285F4"
            />
            <path
              d="M12 23c2.97 0 5.46-.98 7.28-2.66l-3.57-2.77c-.98.66-2.23 1.06-3.71 1.06-2.86 0-5.29-1.93-6.16-4.53H2.18v2.84C3.99 20.53 7.7 23 12 23z"
              fill="#34A853"
            />
            <path
              d="M5.84 14.09c-.22-.66-.35-1.36-.35-2.09s.13-1.43.35-2.09V7.07H2.18C1.43 8.55 1 10.22 1 12s.43 3.45 1.18 4.93l2.85-2.22.81-.62z"
              fill="#FBBC05"
            />
            <path
              d="M12 5.38c1.62 0 3.06.56 4.21 1.64l3.15-3.15C17.45 2.09 14.97 1 12 1 7.7 1 3.99 3.47 2.18 7.07l3.66 2.84c.87-2.6 3.3-4.53 6.16-4.53z"
              fill="#EA4335"
            />
          </svg>
          Sign up with Google
        </button>

        <div className="my-8 flex items-center gap-4">
          <div className="h-[1px] flex-1 bg-foreground/[0.06]" />
          <span
            className="text-xs font-light text-foreground/30"
            style={{ fontFamily: "var(--font-jakarta)" }}
          >
            or
          </span>
          <div className="h-[1px] flex-1 bg-foreground/[0.06]" />
        </div>

        <form className="space-y-4" onSubmit={handleRegister}>
          <div className="relative">
            <User
              size={16}
              strokeWidth={1.5}
              className="absolute top-1/2 left-4 -translate-y-1/2 text-foreground/30"
            />
            <input
              type="text"
              name="username"
              autoComplete="username"
              required
              minLength={USERNAME_MIN_LENGTH}
              maxLength={USERNAME_MAX_LENGTH}
              value={username}
              onChange={(e) => setUsername(e.target.value)}
              placeholder="Username"
              className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 dark:bg-background-tertiary/50 py-3.5 pr-4 pl-11 text-sm font-light text-foreground placeholder:text-foreground/30 transition-all duration-300 focus:border-accent/40 focus:outline-none"
              style={{ fontFamily: "var(--font-jakarta)" }}
            />
          </div>

          <div className="relative">
            <Mail
              size={16}
              strokeWidth={1.5}
              className="absolute top-1/2 left-4 -translate-y-1/2 text-foreground/30"
            />
            <input
              type="email"
              name="email"
              autoComplete="email"
              required
              value={email}
              onChange={(e) => setEmail(e.target.value)}
              placeholder="Email"
              className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 dark:bg-background-tertiary/50 py-3.5 pr-4 pl-11 text-sm font-light text-foreground placeholder:text-foreground/30 transition-all duration-300 focus:border-accent/40 focus:outline-none"
              style={{ fontFamily: "var(--font-jakarta)" }}
            />
          </div>

          <div className="relative">
            <Lock
              size={16}
              strokeWidth={1.5}
              className="absolute top-1/2 left-4 -translate-y-1/2 text-foreground/30"
            />
            <input
              type={showPassword ? "text" : "password"}
              name="password"
              autoComplete="new-password"
              required
              minLength={PASSWORD_MIN_LENGTH}
              aria-describedby="register-password-hint register-password-strength"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              placeholder="Password"
              className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 dark:bg-background-tertiary/50 py-3.5 pr-11 pl-11 text-sm font-light text-foreground placeholder:text-foreground/30 transition-all duration-300 focus:border-accent/40 focus:outline-none"
              style={{ fontFamily: "var(--font-jakarta)" }}
            />
            <PasswordVisibilityToggle visible={showPassword} onToggle={() => setShowPassword(!showPassword)} />
          </div>
          <p
            id="register-password-hint"
            className="-mt-2 pl-1 text-xs font-light text-foreground/30"
            style={{ fontFamily: "var(--font-jakarta)" }}
          >
            At least {PASSWORD_MIN_LENGTH} characters.
          </p>
          <PasswordStrengthMeter
            id="register-password-strength"
            password={password}
            userInputs={[username, email]}
          />

          <div className="relative">
            <Lock
              size={16}
              strokeWidth={1.5}
              className="absolute top-1/2 left-4 -translate-y-1/2 text-foreground/30"
            />
            <input
              type={showPassword ? "text" : "password"}
              name="confirm-password"
              autoComplete="new-password"
              required
              value={confirmPassword}
              onChange={(e) => setConfirmPassword(e.target.value)}
              placeholder="Confirm password"
              className="w-full rounded-xl border border-foreground/[0.08] bg-background-secondary/50 dark:bg-background-tertiary/50 py-3.5 pr-4 pl-11 text-sm font-light text-foreground placeholder:text-foreground/30 transition-all duration-300 focus:border-accent/40 focus:outline-none"
              style={{ fontFamily: "var(--font-jakarta)" }}
            />
          </div>

          <label
            className="flex cursor-pointer items-start gap-2.5 pl-1 text-xs font-light leading-relaxed text-foreground/50"
            style={{ fontFamily: "var(--font-jakarta)" }}
          >
            <input
              type="checkbox"
              name="accept-terms"
              checked={acceptTerms}
              onChange={(e) => setAcceptTerms(e.target.checked)}
              className="mt-0.5 h-3.5 w-3.5 shrink-0 cursor-pointer accent-current"
            />
            <span>
              I accept the{" "}
              <Link href="/terms" target="_blank" className="text-foreground/70 underline-offset-2 hover:text-accent hover:underline">
                Terms of Service
              </Link>{" "}
              and the{" "}
              <Link href="/privacy" target="_blank" className="text-foreground/70 underline-offset-2 hover:text-accent hover:underline">
                Privacy Policy
              </Link>
              .
            </span>
          </label>

          <button
            type="submit"
            className="w-full rounded-xl bg-foreground py-3.5 text-sm font-light tracking-wide text-background transition-all duration-300 hover:bg-accent"
            style={{ fontFamily: "var(--font-jakarta)" }}
          >
            Create my account
          </button>
        </form>

        <p
          className="mt-6 text-center text-sm font-light text-foreground/40"
          style={{ fontFamily: "var(--font-jakarta)" }}
        >
          Already have an account?{" "}
          <Link
            href="/login"
            className="text-foreground/70 transition-colors duration-300 hover:text-accent"
          >
            Log in
          </Link>
        </p>
      </motion.div>
    </div>
  );
}
