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
from stemhub.models import Branch, Version

BACKEND_DIR = Path(__file__).resolve().parents[1]
MESSAGE_RENAME_REVISION = "316caf0fcfa9"
PREVIOUS_HEAD = "1ddecc33a8b7"
ACTORS_REVISION = "c021108cacff"


def _script_directory() -> ScriptDirectory:
    config = Config(str(BACKEND_DIR / "alembic.ini"))
    config.set_main_option("script_location", str(BACKEND_DIR / "alembic"))
    return ScriptDirectory.from_config(config)


def _version_columns(connection) -> set[str]:
    return _columns(connection, "version")


def _columns(connection, table: str) -> set[str]:
    return {column["name"] for column in sa.inspect(connection).get_columns(table)}


def _foreign_keys(connection, table: str) -> set[tuple[str, str]]:
    return {
        (key["constrained_columns"][0], key["referred_table"])
        for key in sa.inspect(connection).get_foreign_keys(table)
    }


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


def test_alembic_history_has_a_single_head_after_the_actor_columns():
    script = _script_directory()

    assert script.get_heads() == [ACTORS_REVISION]
    assert script.get_revision(ACTORS_REVISION).down_revision == MESSAGE_RENAME_REVISION
    assert script.get_revision(MESSAGE_RENAME_REVISION).down_revision == PREVIOUS_HEAD


def _create_pre_actor_tables(connection) -> None:
    connection.execute(sa.text("CREATE TABLE users (id CHAR(32) PRIMARY KEY)"))
    connection.execute(sa.text("CREATE TABLE branch (id CHAR(32) PRIMARY KEY, name VARCHAR(255))"))
    connection.execute(sa.text(
        "CREATE TABLE version (id CHAR(32) PRIMARY KEY, created_by CHAR(32) REFERENCES users (id))"
    ))
    connection.execute(sa.text("INSERT INTO branch (id, name) VALUES ('b1', 'main')"))
    connection.execute(sa.text("INSERT INTO version (id, created_by) VALUES ('v1', NULL)"))


def test_actor_migration_adds_nullable_user_foreign_keys_and_keeps_rows():
    migration = _script_directory().get_revision(ACTORS_REVISION).module
    engine = sa.create_engine("sqlite://")

    with engine.begin() as connection:
        _create_pre_actor_tables(connection)

        _run(connection, migration.upgrade)

        assert _columns(connection, "branch") == {"id", "name", "created_by", "deleted_by"}
        assert _columns(connection, "version") == {"id", "created_by", "deleted_by"}
        assert _foreign_keys(connection, "branch") == {("created_by", "users"), ("deleted_by", "users")}
        assert _foreign_keys(connection, "version") == {("created_by", "users"), ("deleted_by", "users")}
        # Existing rows stay, with no actor: nothing to backfill from.
        assert connection.execute(sa.text("SELECT id, name, created_by, deleted_by FROM branch")).all() == [
            ("b1", "main", None, None)
        ]
        assert connection.execute(sa.text("SELECT id, deleted_by FROM version")).all() == [("v1", None)]

        # Idempotent: a second run finds the columns already there.
        _run(connection, migration.upgrade)
        assert _columns(connection, "branch") == {"id", "name", "created_by", "deleted_by"}


def test_actor_migration_lets_a_user_be_deleted_and_clears_their_actor_columns():
    """Erasing an account deletes its users row. Having created or deleted a
    branch, or deleted a version, in someone else's project must not block
    that: the row stays and loses its actor."""
    migration = _script_directory().get_revision(ACTORS_REVISION).module
    engine = sa.create_engine("sqlite://")

    with engine.begin() as connection:
        # SQLite only enforces foreign keys when asked to, before any write.
        connection.execute(sa.text("PRAGMA foreign_keys = ON"))
        _create_pre_actor_tables(connection)
        _run(connection, migration.upgrade)
        assert connection.execute(sa.text("PRAGMA foreign_keys")).scalar() == 1
        connection.execute(sa.text("INSERT INTO users (id) VALUES ('u1')"))
        connection.execute(sa.text("UPDATE branch SET created_by = 'u1', deleted_by = 'u1'"))
        connection.execute(sa.text("UPDATE version SET deleted_by = 'u1'"))

        connection.execute(sa.text("DELETE FROM users WHERE id = 'u1'"))

        assert connection.execute(sa.text("SELECT id, created_by, deleted_by FROM branch")).all() == [
            ("b1", None, None)
        ]
        assert connection.execute(sa.text("SELECT id, deleted_by FROM version")).all() == [("v1", None)]


def test_actor_columns_of_the_models_match_the_migration():
    """The models must declare the same ON DELETE rule, or the next
    autogenerated migration would try to undo it."""
    for column in (Branch.__table__.c.created_by, Branch.__table__.c.deleted_by, Version.__table__.c.deleted_by):
        [foreign_key] = column.foreign_keys
        assert foreign_key.ondelete == "SET NULL", column


def test_actor_migration_downgrades_back_to_the_previous_columns():
    migration = _script_directory().get_revision(ACTORS_REVISION).module
    engine = sa.create_engine("sqlite://")

    with engine.begin() as connection:
        _create_pre_actor_tables(connection)
        _run(connection, migration.upgrade)

        _run(connection, migration.downgrade)

        assert _columns(connection, "branch") == {"id", "name"}
        assert _columns(connection, "version") == {"id", "created_by"}
        assert _foreign_keys(connection, "version") == {("created_by", "users")}


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
