FROM python:3.11-slim

WORKDIR /app

RUN apt-get update && apt-get install -y \
    gcc \
    libpq-dev \
    && rm -rf /var/lib/apt/lists/*

COPY pyproject.toml ./
COPY src/ ./src/
COPY alembic.ini ./
COPY alembic/ ./alembic/
COPY vendor/PyFLP_enhanced/ ./vendor/PyFLP_enhanced/

RUN pip install --no-cache-dir -e . \
    && export SETUPTOOLS_SCM_PRETEND_VERSION_FOR_PYFLP=0.0.0 \
    && pip install --no-cache-dir --no-build-isolation ./vendor/PyFLP_enhanced

COPY entrypoint.sh ./
RUN chmod +x entrypoint.sh

EXPOSE 8000

CMD ["./entrypoint.sh"]
