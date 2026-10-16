FROM python:3.11-slim

WORKDIR /app

RUN apt-get update && apt-get install -y \
    gcc \
    libpq-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# PyFLP comes from the backend/vendor/PyFLP_v2 submodule, so the image runs the
# commit recorded in src/stemhub/parser_version.py. It is installed before the
# backend sources are copied, so editing them does not rebuild this layer. The
# build context has no .git, so setuptools_scm takes its version from
# SETUPTOOLS_SCM_PRETEND_VERSION_FOR_PYFLP.
COPY vendor/PyFLP_v2/ ./vendor/PyFLP_v2/
RUN SETUPTOOLS_SCM_PRETEND_VERSION_FOR_PYFLP=0.0.0 \
    pip install --no-cache-dir ./vendor/PyFLP_v2

COPY pyproject.toml ./
COPY src/ ./src/
COPY alembic.ini ./
COPY alembic/ ./alembic/

RUN pip install --no-cache-dir -e .

COPY entrypoint.sh ./
RUN chmod +x entrypoint.sh

EXPOSE 8000

CMD ["./entrypoint.sh"]
