import asyncio
import pytest
from unittest.mock import patch, MagicMock

from stemhub.migrations import check_migrations_async

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
