# Troubleshooting Guide: Auth & Database Issues After Pulling

This guide explains how to fix common issues that occur after pulling new code from the `dev` branch, specifically regarding Google Authentication and database migration errors.

---

## 1. Database Migration "Multiple Heads" Error

### Symptoms
- The backend container starts but crashes immediately.
- `docker compose logs backend` shows:
  `FAILED: Multiple head revisions are present for given argument 'head'`

### Why it happens
This occurs when two developers create different database migrations at the same time. When they both merge into `dev`, Alembic gets confused about which one is the "latest".

### The Fix
Run this command from your project root:
```bash
docker compose exec backend alembic merge -m "merge heads" heads
```
Then restart the backend:
```bash
docker compose restart backend
```

---

## 2. Google Authentication "Connection Refused"

### Symptoms
- Clicking "Login with Google" results in a "This site can't be reached" or `ERR_CONNECTION_REFUSED` error.
- The URL bar shows `localhost:8000`.

### Why it happens
1. **Backend is Down**: Usually caused by the "Multiple Heads" error mentioned above. Check `docker compose logs backend`.
2. **Missing Environment Variables**: The container doesn't know your `GOOGLE_CLIENT_ID` or `BACKEND_URL`.

### The Fix
1. Ensure your `.env` file is complete.
2. Check `docker-compose.yml` to make sure the variables are passed to the `backend` service.
3. Rebuild and restart:
   ```bash
   docker compose up -d --build
   ```

---

## 3. Recommended Workflow After Pulling `dev`

To avoid these problems entirely, use this routine every time you pull:

1. **Pull and Rebuild**:
   ```bash
   git pull origin dev && docker compose up -d --build
   ```
2. **Check Logs**:
   ```bash
   docker compose logs backend
   ```
3. **Fix Heads (If needed)**:
   If you see the "Multiple heads" error, run the merge command:
   ```bash
   docker compose exec backend alembic merge -m "fix pull merge" heads
   ```

---

## 💡 Quick Health Check
Run this to see if your backend is actually listening:
```bash
curl http://localhost:8000/
```
*Expected response: `{"message":"Welcome to the StemHub API"}`*
