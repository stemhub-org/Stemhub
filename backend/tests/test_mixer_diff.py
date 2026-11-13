from types import SimpleNamespace

from stemhub.fl_mixer import (
    FlEffectSlot,
    FlMixer,
    FlMixerInsert,
    diff_fl_mixers,
    parse_fl_mixer,
)


def test_diff_fl_mixers_reports_expected_change_types() -> None:
    base_mixer = FlMixer(
        inserts=(
            FlMixerInsert(
                index=2,
                name="Old Bus",
                enabled=True,
                volume=12800,
                pan=-120,
                slots=(
                    FlEffectSlot(
                        index=0,
                        name="Balance",
                        internal_name="Fruity Balance",
                        enabled=True,
                        dry_wet=3200,
                        plugin_name="Fruity Balance",
                    ),
                ),
            ),
            FlMixerInsert(
                index=7,
                name="Remove Me",
                enabled=True,
                volume=10000,
                pan=0,
                slots=(),
            ),
        )
    )
    target_mixer = FlMixer(
        inserts=(
            FlMixerInsert(
                index=2,
                name="Drum Bus",
                enabled=False,
                volume=14000,
                pan=120,
                slots=(
                    FlEffectSlot(
                        index=0,
                        name="Soft Clipper",
                        internal_name="Fruity Soft Clipper",
                        enabled=False,
                        dry_wet=6400,
                        plugin_name="Fruity Soft Clipper",
                    ),
                    FlEffectSlot(
                        index=1,
                        name="Stereo Enhancer",
                        internal_name="Fruity Stereo Enhancer",
                        enabled=True,
                        dry_wet=1600,
                        plugin_name="Fruity Stereo Enhancer",
                    ),
                ),
            ),
            FlMixerInsert(
                index=9,
                name="Added",
                enabled=True,
                volume=12800,
                pan=0,
                slots=(),
            ),
        )
    )

    diff_result = diff_fl_mixers(base_mixer, target_mixer)

    assert [change.type for change in diff_result.changes] == [
        "insert_renamed",
        "insert_enabled_changed",
        "insert_volume_changed",
        "insert_pan_changed",
        "slot_plugin_changed",
        "slot_enabled_changed",
        "slot_dry_wet_changed",
        "slot_added",
        "insert_removed",
        "insert_added",
    ]
    assert diff_result.summary.total_changes == 10
    assert diff_result.summary.inserts_changed == 3
    assert diff_result.summary.slots_changed == 2
    assert diff_result.summary.parameter_changes == 5


def test_diff_fl_mixers_numbers_effect_slots_as_fl_studio_displays_them() -> None:
    # Slot indexes are 0-based; FL Studio shows the first effect slot as 1.
    def mixer_with_slot(dry_wet: int) -> FlMixer:
        slot = FlEffectSlot(
            index=0,
            name="Balance",
            internal_name="Fruity Balance",
            enabled=True,
            dry_wet=dry_wet,
            plugin_name="Fruity Balance",
        )
        return FlMixer(
            inserts=(FlMixerInsert(index=2, name="Drum Bus", enabled=True, volume=12800, pan=0, slots=(slot,)),)
        )

    diff_result = diff_fl_mixers(mixer_with_slot(3200), mixer_with_slot(6400))

    assert [
        (change.type, change.insert_index, change.slot_index, change.before, change.after, change.message)
        for change in diff_result.changes
    ] == [
        (
            "slot_dry_wet_changed",
            2,
            0,
            3200,
            6400,
            'Insert 2 "Drum Bus" effect slot 1 dry/wet changed: 3200 -> 6400',
        ),
    ]


def test_diff_fl_mixers_serializes_added_inserts_with_the_new_field_names() -> None:
    slot = FlEffectSlot(
        index=3,
        name="Limiter",
        internal_name="Fruity Limiter",
        enabled=True,
        dry_wet=12800,
        plugin_name="Fruity Limiter",
    )
    added = FlMixerInsert(index=4, name="Vocals", enabled=True, volume=12800, pan=0, slots=(slot,))

    diff_result = diff_fl_mixers(FlMixer(inserts=()), FlMixer(inserts=(added,)))

    assert len(diff_result.changes) == 1
    change = diff_result.changes[0]
    assert change.type == "insert_added"
    assert change.insert_index == 4
    assert change.after == {
        "index": 4,
        "name": "Vocals",
        "enabled": True,
        "volume": 12800,
        "pan": 0,
        "slots": [
            {
                "index": 3,
                "name": "Limiter",
                "internal_name": "Fruity Limiter",
                "enabled": True,
                "dry_wet": 12800,
                "plugin_name": "Fruity Limiter",
            }
        ],
    }


def test_diff_fl_mixers_reports_a_project_file_change_when_the_mixer_is_unreadable() -> None:
    base_mixer = FlMixer(
        inserts=(),
        flp_sha256="old-hash",
        flp_size_bytes=100,
        mixer_supported=False,
    )
    target_mixer = FlMixer(
        inserts=(),
        flp_sha256="new-hash",
        flp_size_bytes=120,
        mixer_supported=False,
    )

    diff_result = diff_fl_mixers(base_mixer, target_mixer)

    assert diff_result.summary.total_changes == 1
    assert diff_result.summary.inserts_changed == 0
    assert diff_result.summary.slots_changed == 0
    assert diff_result.summary.parameter_changes == 0
    assert len(diff_result.changes) == 1
    change = diff_result.changes[0]
    assert change.type == "project_file_changed"
    assert change.insert_index is None
    assert change.before == {"sha256": "old-hash", "size_bytes": 100}
    assert change.after == {"sha256": "new-hash", "size_bytes": 120}
    assert "project file" in change.message
    assert "binary" not in change.message
    assert "snapshot" not in change.message


def test_diff_fl_mixers_reports_master_volume_change() -> None:
    # PyFLP yields the Master insert with iid -1.
    def pyflp_project(master_volume: int) -> SimpleNamespace:
        master = SimpleNamespace(iid=-1, name="Master", enabled=True, volume=master_volume, pan=0)
        return SimpleNamespace(mixer=[master])

    base_mixer = parse_fl_mixer(pyflp_project(12800))
    target_mixer = parse_fl_mixer(pyflp_project(10000))

    diff_result = diff_fl_mixers(base_mixer, target_mixer)

    assert [
        (change.type, change.insert_index, change.before, change.after, change.message)
        for change in diff_result.changes
    ] == [
        ("insert_volume_changed", 0, 12800, 10000, "Master volume changed: 12800 -> 10000"),
    ]
    assert diff_result.summary.inserts_changed == 1
    assert diff_result.summary.parameter_changes == 1
