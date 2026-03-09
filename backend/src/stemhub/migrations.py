import logging
import os
from alembic.config import Config
from alembic.script import ScriptDirectory
from alembic.runtime.migration import MigrationContext
from stemhub.database import engine

logger = logging.getLogger(__name__)

def _get_current_revision(connection):
    context = MigrationContext.configure(connection)
    return context.get_current_revision()

async def check_migrations_async():
    """
    Check if the database is up to date with the latest Alembic migrations.
    Raises RuntimeError if migrations are pending.
    """
    try:
        # Determine paths relative to this file
        current_dir = os.path.dirname(os.path.abspath(__file__))
        backend_dir = os.path.dirname(os.path.dirname(current_dir))
        alembic_ini_path = os.path.join(backend_dir, "alembic.ini")
        alembic_dir_path = os.path.join(backend_dir, "alembic")
        
        # Load Alembic configuration
        alembic_cfg = Config(alembic_ini_path)
        alembic_cfg.set_main_option("script_location", alembic_dir_path)
        
        # Get the head revision from the script directory
        script = ScriptDirectory.from_config(alembic_cfg)
        head_revision = script.get_current_head()
        
        # Get the current revision from the database
        async with engine.connect() as connection:
            current_revision = await connection.run_sync(_get_current_revision)
            
        if head_revision is None:
            logger.info("No migrations found in the script directory.")
            return

        if current_revision != head_revision:
            error_msg = (
                f"Database schema is not up to date. "
                f"Current revision: {current_revision}. Head revision: {head_revision}. "
                f"Please run 'alembic upgrade head' or run via docker compose to apply migrations."
            )
            logger.error(error_msg)
            raise RuntimeError(error_msg)
            
        logger.info(f"Database schema is up to date (revision: {current_revision}).")
    except Exception as e:
        logger.error(f"Error checking migrations: {e}")
        raise
