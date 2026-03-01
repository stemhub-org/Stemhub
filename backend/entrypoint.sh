#!/bin/bash
set -e

echo "Running database migrations..."
alembic upgrade head

echo "Starting server..."
exec uvicorn stemhub.main:app --host 0.0.0.0 --port 8000
