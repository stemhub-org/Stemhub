FROM python:3.11-slim

WORKDIR /app

RUN apt-get update && apt-get install -y \
    gcc \
    libpq-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

COPY pyproject.toml ./
COPY src/ ./src/
COPY alembic.ini ./
COPY alembic/ ./alembic/

RUN pip install --no-cache-dir -e . \
    && export SETUPTOOLS_SCM_PRETEND_VERSION_FOR_PYFLP=0.0.0 \
    && pip install --no-cache-dir --no-build-isolation "git+https://github.com/stemhub-org/PyFLP_v2.git@main"

COPY entrypoint.sh ./
RUN chmod +x entrypoint.sh

EXPOSE 8000

CMD ["./entrypoint.sh"]
