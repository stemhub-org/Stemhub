#!/bin/bash
set -e

echo "Waiting for database..."

while ! python -c "import socket; socket.create_connection(('db', 5432))" 2>/dev/null; do
  sleep 0.1
done
echo "Database is up!"

echo "Running database migrations..."
alembic upgrade head

echo "Starting server..."
PYTHONPATH=./src exec uvicorn stemhub.main:app --host 0.0.0.0 --port 8000 --reload
