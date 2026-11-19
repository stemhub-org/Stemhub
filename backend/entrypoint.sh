#!/bin/bash
set -e

echo "Waiting for database..."

# Use the same PostgreSQL URL variants as the application, including IPv6 and
# the default port. Do not interpolate credentials or hostnames into Python code.
PYTHONPATH=./src python -m stemhub.wait_for_database
echo "Database is up!"

echo "Running database migrations..."
alembic upgrade head

echo "Starting server..."
PYTHONPATH=./src exec uvicorn stemhub.main:app --host 0.0.0.0 --port ${PORT:-8000}
