# StemHub Backend

This backend depends on `PyFLP_v2` through a Git submodule, so StemHub does not vendor the full library history.

## Bootstrap (local + CI)

```bash
./backend/scripts/bootstrap-backend.sh
```

> **Note:** The backend no longer creates database tables automatically on startup. You must ensure all Alembic migrations are applied before starting the server. `bootstrap-backend.sh` handles this for local development.

## Storage configuration

Uploaded files (the content-addressed blobs of each version's project file and
assets, and each project's preview) go where `STEMHUB_STORAGE_PROVIDER` says:

- `localfs` (default): stored on disk under `STEMHUB_STORAGE_ROOT` (default
  `backend/data/artifacts`, a historical directory name kept so existing blobs
  are still found). The former name `STEMHUB_ARTIFACTS_ROOT` is still read when
  `STEMHUB_STORAGE_ROOT` is unset, with a deprecation warning in the logs.
- `gcs`: stored in Google Cloud Storage.

Example settings:

```bash
STEMHUB_STORAGE_PROVIDER=localfs
STEMHUB_STORAGE_ROOT=./backend/data/artifacts
```

For Google Cloud Storage:

```bash
STEMHUB_STORAGE_PROVIDER=gcs
STEMHUB_GCS_BUCKET=your-bucket-name
STEMHUB_GCS_PROJECT=your-gcp-project-id
# Option A (recommended in GKE/App Runner/etc.):
# GOOGLE_APPLICATION_CREDENTIALS=/path/to/service-account.json
# Option B:
# STEMHUB_GCS_CREDENTIALS_JSON='{"type":"service_account",...}'
```

Equivalent commands:

```bash
git submodule sync --recursive
git submodule update --init --recursive
pip install -e backend/vendor/PyFLP_v2
pip install -e backend
```

## Verify dependency wiring

```bash
python -c "import pyflp; print(pyflp.__file__)"
```

## How the parser is pinned

The parser version is the commit of the `backend/vendor/PyFLP_v2` submodule,
never a branch of the fork:

- Local setups and CI install PyFLP from the submodule.
- The Docker image copies the submodule into the build and installs it
  (non-editable), so a CI or `docker compose` build needs the submodule checked
  out (`git submodule update --init --recursive`), and the image must be
  rebuilt after a bump.
- `src/stemhub/parser_version.py` records the full commit as `PYFLP_COMMIT`.
  Bump it together with the submodule pointer:
  `tests/test_parser_version.py` fails while they differ.

## Parser fixture corpus

StemHub keeps a parser validation corpus in `backend/tests/fixtures/parser_corpus/corpus.json`.

Run the dedicated harness with:

```bash
.venv/bin/pytest backend/tests/test_parser_fixture_corpus.py
```

When adding new roadmap parser work, prefer extending the corpus and its stable expectations instead of relying on ad hoc local project files. See `backend/tests/fixtures/parser_corpus/README.md` for the fixture format and extension rules.

## Contributing to `PyFLP_v2`

1. Work inside `backend/vendor/PyFLP_v2` on a branch.
2. Push your branch and open a PR in `stemhub-org/PyFLP_v2`.
3. After merge, update the submodule pointer in StemHub, and set
   `PYFLP_COMMIT` in `backend/src/stemhub/parser_version.py` to the output of
   `git -C backend/vendor/PyFLP_v2 rev-parse HEAD`:

```bash
git submodule update --remote -- backend/vendor/PyFLP_v2
git add backend/vendor/PyFLP_v2 .gitmodules backend/src/stemhub/parser_version.py
git commit -m "chore: bump PyFLP_v2 submodule"
```

The StemHub workflow `sync-pyflp-submodule.yml` can also open this PR automatically (against `dev`, with `PYFLP_COMMIT` updated) when upstream pushes trigger repository dispatch.

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
