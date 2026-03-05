"""update_models

Revision ID: 58dbef40b2ff
Revises: 4aad2f560761
Create Date: 2026-03-03 14:10:12.298460

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects import postgresql


# revision identifiers, used by Alembic.
revision: str = '58dbef40b2ff'
down_revision: Union[str, None] = '4aad2f560761'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    bind = op.get_bind()
    inspector = sa.inspect(bind)
    tables = set(inspector.get_table_names())

    if 'version' in tables:
        version_columns = {column["name"] for column in inspector.get_columns('version')}

        if 'storage_path' in version_columns and 'artifact_path' not in version_columns:
            op.alter_column(
                'version',
                'storage_path',
                new_column_name='artifact_path',
                existing_type=sa.String(),
                existing_nullable=True,
            )
            version_columns.remove('storage_path')
            version_columns.add('artifact_path')

        if 'artifact_size' in version_columns and 'artifact_size_bytes' not in version_columns:
            op.alter_column(
                'version',
                'artifact_size',
                new_column_name='artifact_size_bytes',
                existing_type=sa.BigInteger(),
                existing_nullable=True,
            )
            version_columns.remove('artifact_size')
            version_columns.add('artifact_size_bytes')

        if 'artifact_size_bytes' not in version_columns:
            op.add_column('version', sa.Column('artifact_size_bytes', sa.BigInteger(), nullable=True))
        if 'artifact_checksum' not in version_columns:
            op.add_column('version', sa.Column('artifact_checksum', sa.String(length=128), nullable=True))
        if 'source_daw' not in version_columns:
            op.add_column('version', sa.Column('source_daw', sa.String(length=50), nullable=True))
        if 'source_project_filename' not in version_columns:
            op.add_column('version', sa.Column('source_project_filename', sa.String(length=255), nullable=True))
        if 'snapshot_manifest' not in version_columns:
            op.add_column('version', sa.Column('snapshot_manifest', postgresql.JSONB(), nullable=True))


def downgrade() -> None:
    bind = op.get_bind()
    inspector = sa.inspect(bind)
    tables = set(inspector.get_table_names())

    if 'version' in tables:
        version_columns = {column["name"] for column in inspector.get_columns('version')}

        if 'snapshot_manifest' in version_columns:
            op.drop_column('version', 'snapshot_manifest')
        if 'source_project_filename' in version_columns:
            op.drop_column('version', 'source_project_filename')
        if 'source_daw' in version_columns:
            op.drop_column('version', 'source_daw')
        if 'artifact_checksum' in version_columns:
            op.drop_column('version', 'artifact_checksum')
        if 'artifact_size_bytes' in version_columns:
            op.drop_column('version', 'artifact_size_bytes')
        elif 'artifact_size' in version_columns:
            op.drop_column('version', 'artifact_size')
        if 'artifact_path' in version_columns and 'storage_path' not in version_columns:
            op.alter_column(
                'version',
                'artifact_path',
                new_column_name='storage_path',
                existing_type=sa.String(),
                existing_nullable=True,
            )
