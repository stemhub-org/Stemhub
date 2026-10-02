import asyncio
from pathlib import Path

import pytest
import sqlalchemy as sa
from alembic.config import Config
from alembic.operations import Operations
from alembic.runtime.migration import MigrationContext
from alembic.script import ScriptDirectory
from unittest.mock import patch, MagicMock

from stemhub.migrations import check_migrations_async

BACKEND_DIR = Path(__file__).resolve().parents[1]
MESSAGE_RENAME_REVISION = "316caf0fcfa9"
PREVIOUS_HEAD = "1ddecc33a8b7"


def _script_directory() -> ScriptDirectory:
    config = Config(str(BACKEND_DIR / "alembic.ini"))
    config.set_main_option("script_location", str(BACKEND_DIR / "alembic"))
    return ScriptDirectory.from_config(config)


def _version_columns(connection) -> set[str]:
    return {column["name"] for column in sa.inspect(connection).get_columns("version")}


def _run(connection, step) -> None:
    with Operations.context(MigrationContext.configure(connection)):
        step()

def test_check_migrations_up_to_date():
    with patch('stemhub.migrations.engine') as mock_engine, \
         patch('stemhub.migrations.ScriptDirectory') as mock_script_dir_class:
        
        # We need to mock an async context manager: async with engine.connect() as connection
        # and inside we await connection.run_sync()
        mock_conn = MagicMock()
        
        # We simulate the await on run_sync which returns a coroutine returning the revision
        async def mock_run_sync(func, *args, **kwargs):
            return "fake_head_rev"
            
        mock_conn.run_sync = mock_run_sync
        
        # Async context manager mock
        class AsyncContextManagerMock:
            async def __aenter__(self):
                return mock_conn
            async def __aexit__(self, exc_type, exc, tb):
                pass
                
        mock_engine.connect.return_value = AsyncContextManagerMock()
        
        # Mock ScriptDirectory (Alembic files)
        mock_script_dir = MagicMock()
        mock_script_dir_class.from_config.return_value = mock_script_dir
        mock_script_dir.get_current_head.return_value = "fake_head_rev"
        
        # Should not raise an exception
        asyncio.run(check_migrations_async())
        

def test_check_migrations_pending():
    with patch('stemhub.migrations.engine') as mock_engine, \
         patch('stemhub.migrations.ScriptDirectory') as mock_script_dir_class:
        
        mock_conn = MagicMock()
        
        async def mock_run_sync(func, *args, **kwargs):
            return "old_rev"
            
        mock_conn.run_sync = mock_run_sync
        
        class AsyncContextManagerMock:
            async def __aenter__(self):
                return mock_conn
            async def __aexit__(self, exc_type, exc, tb):
                pass
                
        mock_engine.connect.return_value = AsyncContextManagerMock()
        
        # Mock ScriptDirectory (Alembic files)
        mock_script_dir = MagicMock()
        mock_script_dir_class.from_config.return_value = mock_script_dir
        mock_script_dir.get_current_head.return_value = "fake_head_rev"
        
        # Should raise an exception because revisions don't match
        with pytest.raises(RuntimeError, match="Database schema is not up to date"):
            asyncio.run(check_migrations_async())

def test_check_migrations_no_scripts():
    with patch('stemhub.migrations.engine') as mock_engine, \
         patch('stemhub.migrations.ScriptDirectory') as mock_script_dir_class:
        
        mock_conn = MagicMock()
        
        async def mock_run_sync(func, *args, **kwargs):
            return "any_rev"
            
        mock_conn.run_sync = mock_run_sync
        
        class AsyncContextManagerMock:
            async def __aenter__(self):
                return mock_conn
            async def __aexit__(self, exc_type, exc, tb):
                pass
                
        mock_engine.connect.return_value = AsyncContextManagerMock()
        
        # Mock ScriptDirectory (Alembic files)
        mock_script_dir = MagicMock()
        mock_script_dir_class.from_config.return_value = mock_script_dir
        mock_script_dir.get_current_head.return_value = None
        
        # Should simply return if no scripts are found
        asyncio.run(check_migrations_async())


def test_alembic_history_has_a_single_head_after_the_message_rename():
    script = _script_directory()

    assert script.get_heads() == [MESSAGE_RENAME_REVISION]
    assert script.get_revision(MESSAGE_RENAME_REVISION).down_revision == PREVIOUS_HEAD


def test_message_rename_migration_renames_the_column_and_clears_the_plugin_placeholder():
    migration = _script_directory().get_revision(MESSAGE_RENAME_REVISION).module
    engine = sa.create_engine("sqlite://")

    with engine.begin() as connection:
        connection.execute(sa.text("CREATE TABLE version (id INTEGER PRIMARY KEY, commit_message VARCHAR(500))"))
        connection.execute(sa.text(
            "INSERT INTO version (id, commit_message) VALUES "
            "(1, 'Save from plugin'), (2, 'Mixed the drums'), (3, NULL), (4, 'save from plugin')"
        ))

        _run(connection, migration.upgrade)

        assert _version_columns(connection) == {"id", "message"}
        rows = connection.execute(sa.text("SELECT id, message FROM version ORDER BY id")).all()
        # Only the exact placeholder the plugin sent is cleared; real messages stay.
        assert rows == [(1, None), (2, "Mixed the drums"), (3, None), (4, "save from plugin")]

        # Idempotent: a second run finds the column renamed already.
        _run(connection, migration.upgrade)
        assert _version_columns(connection) == {"id", "message"}


def test_message_rename_migration_downgrades_back_to_commit_message():
    migration = _script_directory().get_revision(MESSAGE_RENAME_REVISION).module
    engine = sa.create_engine("sqlite://")

    with engine.begin() as connection:
        connection.execute(sa.text("CREATE TABLE version (id INTEGER PRIMARY KEY, commit_message VARCHAR(500))"))
        connection.execute(sa.text("INSERT INTO version (id, commit_message) VALUES (1, 'Save from plugin'), (2, 'Kept')"))
        _run(connection, migration.upgrade)

        _run(connection, migration.downgrade)

        assert _version_columns(connection) == {"id", "commit_message"}
        rows = connection.execute(sa.text("SELECT id, commit_message FROM version ORDER BY id")).all()
        # The cleared placeholder cannot come back.
        assert rows == [(1, None), (2, "Kept")]
