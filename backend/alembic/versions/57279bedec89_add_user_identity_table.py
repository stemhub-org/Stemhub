"""add user_identity table

Social sign-in now finds users by the provider's stable account id instead
of by email. Existing Google users have no row yet: their next Google
sign-in links them by (verified) email and creates it.

Revision ID: 57279bedec89
Revises: 2e0e729975d1
Create Date: 2026-11-15 12:30:47.289193

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = '57279bedec89'
down_revision: Union[str, None] = '2e0e729975d1'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table('user_identity',
    sa.Column('id', sa.UUID(), nullable=False),
    sa.Column('user_id', sa.UUID(), nullable=False),
    sa.Column('provider', sa.String(length=32), nullable=False),
    sa.Column('provider_user_id', sa.String(length=255), nullable=False),
    sa.Column('email', sa.String(), nullable=True),
    sa.Column('created_at', sa.DateTime(timezone=True), nullable=False),
    sa.ForeignKeyConstraint(['user_id'], ['users.id'], ondelete='CASCADE'),
    sa.PrimaryKeyConstraint('id'),
    sa.UniqueConstraint('provider', 'provider_user_id', name='uq_user_identity_provider_account'),
    sa.UniqueConstraint('user_id', 'provider', name='uq_user_identity_user_provider')
    )
    op.create_index(op.f('ix_user_identity_user_id'), 'user_identity', ['user_id'], unique=False)


def downgrade() -> None:
    op.drop_index(op.f('ix_user_identity_user_id'), table_name='user_identity')
    op.drop_table('user_identity')
