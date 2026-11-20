FROM python:3.11-slim

WORKDIR /app

# Keep the image's package installers above the audited security fixes.
RUN python -m pip install --no-cache-dir --upgrade 'pip>=26.2' 'setuptools>=84.1.0'

RUN apt-get update && apt-get install -y \
    gcc \
    libpq-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# PyFLP comes from the backend/vendor/PyFLP_v2 submodule, so the image runs the
# commit recorded as PYFLP_COMMIT in src/stemhub/parser_version.py. It is
# installed before the rest of the backend sources are copied, so editing them
# does not rebuild this layer. The build context has no .git, so setuptools_scm
# takes its version from SETUPTOOLS_SCM_PRETEND_VERSION_FOR_PYFLP.
#
# Some builders hand over the repository without submodule contents, so
# vendor/PyFLP_v2 is an empty directory (Railway does this). The image then
# fetches the PYFLP_COMMIT commit from PYFLP_REPOSITORY instead, so it still
# runs the pinned parser.
ARG PYFLP_REPOSITORY=https://github.com/stemhub-org/PyFLP_v2.git
COPY src/stemhub/parser_version.py /tmp/parser_version.py
COPY vendor/PyFLP_v2/ ./vendor/PyFLP_v2/
RUN set -eu; \
    if [ ! -f vendor/PyFLP_v2/pyproject.toml ]; then \
        commit="$(python -c 'import runpy; print(runpy.run_path("/tmp/parser_version.py")["PYFLP_COMMIT"])')"; \
        echo "vendor/PyFLP_v2 is empty in the build context: fetching PyFLP_v2 ${commit}"; \
        rm -rf vendor/PyFLP_v2; \
        git init -q vendor/PyFLP_v2; \
        git -C vendor/PyFLP_v2 fetch -q --depth 1 "${PYFLP_REPOSITORY}" "${commit}"; \
        git -C vendor/PyFLP_v2 -c advice.detachedHead=false checkout -q FETCH_HEAD; \
        rm -rf vendor/PyFLP_v2/.git; \
    fi; \
    SETUPTOOLS_SCM_PRETEND_VERSION_FOR_PYFLP=0.0.0 \
        pip install --no-cache-dir ./vendor/PyFLP_v2; \
    rm /tmp/parser_version.py

COPY pyproject.toml ./
COPY src/ ./src/
COPY alembic.ini ./
COPY alembic/ ./alembic/

RUN pip install --no-cache-dir -e .

COPY entrypoint.sh ./
RUN chmod +x entrypoint.sh

EXPOSE 8000

CMD ["./entrypoint.sh"]
