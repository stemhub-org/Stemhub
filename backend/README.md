# StemHub Backend

This backend depends on `PyFLP_enhanced` through a Git submodule, so StemHub does not vendor the full library history.

## Bootstrap (local + CI)

```bash
./backend/scripts/bootstrap-backend.sh
```

Equivalent commands:

```bash
git submodule sync --recursive
git submodule update --init --recursive
pip install -e backend/vendor/PyFLP_enhanced
pip install -e backend
```

## Verify dependency wiring

```bash
python -c "import pyflp; print(pyflp.__version__)"
```

## Contributing to `PyFLP_enhanced`

1. Work inside `backend/vendor/PyFLP_enhanced` on a branch.
2. Push your branch and open a PR in `aernw1/PyFLP_enhanced`.
3. After merge, update the submodule pointer in StemHub:

```bash
git submodule update --remote -- backend/vendor/PyFLP_enhanced
git add backend/vendor/PyFLP_enhanced .gitmodules
git commit -m "chore: bump PyFLP_enhanced submodule"
```

The StemHub workflow `sync-pyflp-submodule.yml` can also open this PR automatically when upstream pushes trigger repository dispatch.

## Docker

### Prerequisites

- Docker & Docker Compose installed
- A `.env` file at the project root (see `.env.example`)

### Start everything

```bash
# Stop local PostgreSQL first (it uses the same port)
sudo systemctl stop postgresql

# Start all services (DB, Backend, Frontend, pgAdmin)
docker compose up -d --build
```

| Service    | URL                   |
| ---------- | --------------------- |
| Backend    | http://localhost:8000 |
| Frontend   | http://localhost:3000 |
| pgAdmin    | http://localhost:5050 |
| PostgreSQL | localhost:5432        |

### pgAdmin login

- **Email:** see `PGADMIN_DEFAULT_EMAIL` in `.env`
- **Password:** see `PGADMIN_DEFAULT_PASSWORD` in `.env`
- When adding a server, use host `db`, port `5432`, and your Postgres credentials.

## Database Migrations (Alembic)

### Workflow: adding or modifying a table

```bash
# 1. Edit backend/src/stemhub/models.py

# 2. Rebuild the backend image
docker compose up -d --build backend

# 3. Generate a migration (auto-detects changes in models.py)
docker compose exec backend alembic revision --autogenerate -m "describe your change"

# 4. Apply the migration to the database
docker compose exec backend alembic upgrade head

# 5. Copy the new migration file to your host
docker compose cp backend:/app/alembic/versions/<filename>.py backend/alembic/versions/
```

> **Important:** Always run Alembic commands inside the Docker container (`docker compose exec backend ...`), not directly on your machine.

### Useful commands

```bash
# Check current DB version
docker compose exec backend alembic current

# View migration history
docker compose exec backend alembic history

# Rollback the last migration
docker compose exec backend alembic downgrade -1
```
