from stemhub.dependency_guard import ensure_pyflp_available


def test_pyflp_import_is_available() -> None:
    ensure_pyflp_available()
