import hashlib
import io

import pytest

from stemhub.storage import LocalFilesystemStorageService, StorageNotFoundError


def test_local_filesystem_storage_persists_artifact_with_expected_metadata(tmp_path) -> None:
    storage = LocalFilesystemStorageService(tmp_path)
    payload = b"demo snapshot payload"

    stored = storage.store_version_artifact(
        project_id="11111111-1111-1111-1111-111111111111",
        branch_id="22222222-2222-2222-2222-222222222222",
        version_id="33333333-3333-3333-3333-333333333333",
        filename="../unsafe-demo.flp",
        source=io.BytesIO(payload),
    )

    expected_relative_path = (
        "projects/11111111-1111-1111-1111-111111111111/"
        "branches/22222222-2222-2222-2222-222222222222/"
        "versions/33333333-3333-3333-3333-333333333333/"
        "snapshot/unsafe-demo.flp"
    )

    assert stored.path == expected_relative_path
    assert stored.size_bytes == len(payload)
    assert stored.checksum_sha256 == hashlib.sha256(payload).hexdigest()
    assert storage.resolve_artifact_path(stored.path).read_bytes() == payload


def test_local_filesystem_storage_rejects_path_traversal(tmp_path) -> None:
    storage = LocalFilesystemStorageService(tmp_path)

    with pytest.raises(StorageNotFoundError):
        storage.resolve_artifact_path("../outside.flp")
