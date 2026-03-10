from stemhub.flp_mixer_snapshot import (
    MixerInsertSnapshot,
    MixerProjectSnapshot,
    MixerSlotSnapshot,
    diff_mixer_project_snapshots,
)


def test_diff_mixer_project_snapshots_reports_expected_change_types() -> None:
    base_snapshot = MixerProjectSnapshot(
        inserts=(
            MixerInsertSnapshot(
                iid=2,
                name="Old Bus",
                enabled=True,
                volume=12800,
                pan=-120,
                slots=(
                    MixerSlotSnapshot(
                        index=0,
                        name="Balance",
                        internal_name="Fruity Balance",
                        enabled=True,
                        mix=3200,
                        plugin_key="Fruity Balance",
                    ),
                ),
            ),
            MixerInsertSnapshot(
                iid=7,
                name="Remove Me",
                enabled=True,
                volume=10000,
                pan=0,
                slots=(),
            ),
        )
    )
    target_snapshot = MixerProjectSnapshot(
        inserts=(
            MixerInsertSnapshot(
                iid=2,
                name="Drum Bus",
                enabled=False,
                volume=14000,
                pan=120,
                slots=(
                    MixerSlotSnapshot(
                        index=0,
                        name="Soft Clipper",
                        internal_name="Fruity Soft Clipper",
                        enabled=False,
                        mix=6400,
                        plugin_key="Fruity Soft Clipper",
                    ),
                    MixerSlotSnapshot(
                        index=1,
                        name="Stereo Enhancer",
                        internal_name="Fruity Stereo Enhancer",
                        enabled=True,
                        mix=1600,
                        plugin_key="Fruity Stereo Enhancer",
                    ),
                ),
            ),
            MixerInsertSnapshot(
                iid=9,
                name="Added",
                enabled=True,
                volume=12800,
                pan=0,
                slots=(),
            ),
        )
    )

    diff_result = diff_mixer_project_snapshots(base_snapshot, target_snapshot)

    assert [change.type for change in diff_result.changes] == [
        "insert_renamed",
        "insert_enabled_changed",
        "insert_volume_changed",
        "insert_pan_changed",
        "slot_plugin_changed",
        "slot_enabled_changed",
        "slot_mix_changed",
        "slot_added",
        "insert_removed",
        "insert_added",
    ]
    assert diff_result.summary.total_changes == 10
    assert diff_result.summary.inserts_changed == 3
    assert diff_result.summary.slots_changed == 2
    assert diff_result.summary.parameter_changes == 5
