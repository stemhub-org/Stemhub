"""add genres to user

Revision ID: 136d2794af3b
Revises: d4f1a6d2a9c0
Create Date: 2026-03-05 15:45:38.192754

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects import postgresql

# revision identifiers, used by Alembic.
revision: str = '136d2794af3b'
down_revision: Union[str, None] = 'd4f1a6d2a9c0'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    bind = op.get_bind()
    inspector = sa.inspect(bind)
    tables = set(inspector.get_table_names())

    if 'users' in tables:
        user_columns = {column['name'] for column in inspector.get_columns('users')}

        if 'genres' not in user_columns:
            op.add_column('users', sa.Column('genres', postgresql.JSONB(astext_type=sa.Text()), nullable=True))

        for column_name in ('auth_provider', 'failed_login_attempts', 'is_verified', 'locked_until'):
            if column_name in user_columns:
                op.drop_column('users', column_name)

    if 'version' in tables:
        version_columns = {column['name'] for column in inspector.get_columns('version')}

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
        elif 'artifact_path' not in version_columns:
            op.add_column('version', sa.Column('artifact_path', sa.String(), nullable=True))
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
        elif 'artifact_size_bytes' not in version_columns:
            op.add_column('version', sa.Column('artifact_size_bytes', sa.BigInteger(), nullable=True))
            version_columns.add('artifact_size_bytes')

        if 'artifact_checksum' not in version_columns:
            op.add_column('version', sa.Column('artifact_checksum', sa.String(length=128), nullable=True))
        if 'source_daw' not in version_columns:
            op.add_column('version', sa.Column('source_daw', sa.String(length=50), nullable=True))
        if 'source_project_filename' not in version_columns:
            op.add_column('version', sa.Column('source_project_filename', sa.String(length=255), nullable=True))
        if 'snapshot_manifest' not in version_columns:
            op.add_column('version', sa.Column('snapshot_manifest', postgresql.JSONB(astext_type=sa.Text()), nullable=True))

        if 'storage_path' in version_columns and 'artifact_path' in version_columns:
            op.drop_column('version', 'storage_path')


def downgrade() -> None:
    bind = op.get_bind()
    inspector = sa.inspect(bind)
    tables = set(inspector.get_table_names())

    if 'version' in tables:
        version_columns = {column['name'] for column in inspector.get_columns('version')}

        if 'snapshot_manifest' in version_columns:
            op.drop_column('version', 'snapshot_manifest')
        if 'source_project_filename' in version_columns:
            op.drop_column('version', 'source_project_filename')
        if 'source_daw' in version_columns:
            op.drop_column('version', 'source_daw')
        if 'artifact_checksum' in version_columns:
            op.drop_column('version', 'artifact_checksum')

        if 'artifact_size_bytes' in version_columns and 'artifact_size' not in version_columns:
            op.alter_column(
                'version',
                'artifact_size_bytes',
                new_column_name='artifact_size',
                existing_type=sa.BigInteger(),
                existing_nullable=True,
            )
            version_columns.remove('artifact_size_bytes')
            version_columns.add('artifact_size')
        elif 'artifact_size_bytes' in version_columns:
            op.drop_column('version', 'artifact_size_bytes')

        if 'artifact_path' in version_columns and 'storage_path' not in version_columns:
            op.alter_column(
                'version',
                'artifact_path',
                new_column_name='storage_path',
                existing_type=sa.String(),
                existing_nullable=True,
            )
        elif 'artifact_path' in version_columns:
            op.drop_column('version', 'artifact_path')

    if 'users' in tables:
        user_columns = {column['name'] for column in inspector.get_columns('users')}

        if 'locked_until' not in user_columns:
            op.add_column(
                'users',
                sa.Column(
                    'locked_until',
                    postgresql.TIMESTAMP(timezone=True),
                    autoincrement=False,
                    nullable=True,
                ),
            )
        if 'is_verified' not in user_columns:
            op.add_column(
                'users',
                sa.Column(
                    'is_verified',
                    sa.BOOLEAN(),
                    server_default=sa.text('false'),
                    autoincrement=False,
                    nullable=False,
                ),
            )
        if 'failed_login_attempts' not in user_columns:
            op.add_column(
                'users',
                sa.Column(
                    'failed_login_attempts',
                    sa.INTEGER(),
                    server_default=sa.text('0'),
                    autoincrement=False,
                    nullable=False,
                ),
            )
        if 'auth_provider' not in user_columns:
            op.add_column(
                'users',
                sa.Column(
                    'auth_provider',
                    sa.VARCHAR(),
                    server_default=sa.text("'local'::character varying"),
                    autoincrement=False,
                    nullable=False,
                ),
            )
        if 'genres' in user_columns:
            op.drop_column('users', 'genres')
