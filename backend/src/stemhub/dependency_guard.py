"""Runtime checks for external backend dependencies."""


def ensure_pyflp_available() -> None:
    """Raise a clear error if the PyFLP submodule dependency is not installed."""
    try:
        import pyflp  # noqa: F401
    except ModuleNotFoundError as exc:
        raise RuntimeError(
            "PyFLP_enhanced is not installed. Run: "
            "`git submodule update --init --recursive && "
            "pip install -e backend/vendor/PyFLP_enhanced`."
        ) from exc

