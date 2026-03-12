#!/bin/bash
set -e

echo "Waiting for database..."

# Extract host and port from DATABASE_URL for the connectivity check
# Supports both postgresql:// and postgresql+asyncpg:// formats
if [ -n "$DATABASE_URL" ]; then
  DB_HOST=$(echo "$DATABASE_URL" | sed -E 's|^postgresql(\+asyncpg)?://[^@]+@([^:/]+).*|\2|')
  DB_PORT=$(echo "$DATABASE_URL" | sed -E 's|^postgresql(\+asyncpg)?://[^@]+@[^:]+:([0-9]+).*|\2|')
  DB_PORT=${DB_PORT:-5432}
else
  DB_HOST="db"
  DB_PORT="5432"
fi

echo "Connecting to database at ${DB_HOST}:${DB_PORT}..."

while ! python -c "import socket; socket.create_connection(('${DB_HOST}', ${DB_PORT}))" 2>/dev/null; do
  sleep 0.5
done
echo "Database is up!"

echo "Running database migrations..."
alembic upgrade head

echo "Starting server..."
PYTHONPATH=./src exec uvicorn stemhub.main:app --host 0.0.0.0 --port ${PORT:-8000}
