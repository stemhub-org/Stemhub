"""Read FL 20–2025 musical content without retaining private paths or parser errors.

Only schema fields are copied. An unreadable section is reported in unsupported,
independently of the other sections. Bump EXTRACTOR_VERSION whenever extraction
semantics change; the parser pin is part of the persistent cache key too.
"""

from __future__ import annotations

import hashlib
import math
import re
from pathlib import Path, PureWindowsPath
from typing import Any, Callable

from pydantic import TypeAdapter

from stemhub.parser_version import PYFLP_COMMIT
from . import schema as s, units as u
from .flp_events import parse as parse_events, fl_version
from .references import check_references

EXTRACTOR_VERSION = f"0.1.0-{PYFLP_COMMIT}"


class ProjectReadError(ValueError):
    pass


def read_flp(path: Path) -> s.ProjectModel:
    """A schema-valid model, including explicit reports of unsupported content."""
    import pyflp

    try:
        raw = parse_events(path.read_bytes())
        from pyflp.project import FileFormat, VALID_PPQS

        FileFormat(raw.header.format)
        if raw.header.ppq not in VALID_PPQS:
            raise ValueError("Invalid PPQ")
    except Exception as exc:
        raise ProjectReadError("The FL Studio project file is structurally invalid.") from exc
    unsupported = []
    try:
        project = pyflp.parse(path)
    except Exception:
        project, unsupported = _decode_sections(raw)
    return model_from_project(project, ppq=raw.header.ppq, unsupported=unsupported)


def _decode_sections(raw):
    """Fallback for a typed event decoder failure in the pinned PyFLP.

    Keep an undecodable event opaque so unrelated sections remain readable.
    This adapter deliberately uses the pinned fork's event constructors.
    """
    from pyflp._events import (
        EventEnum,
        EventTree,
        IndexedEvent,
        UnknownDataEvent,
        UnicodeEvent,
        AsciiEvent,
        U8Event,
        U16Event,
        U32Event,
    )
    from pyflp.plugin import PluginID, get_event_by_internal_name
    from pyflp.project import Project, FileFormat
    from pyflp.types import FLVersion
    import pyflp

    version_tuple = fl_version(raw)
    if version_tuple is None:
        raise ProjectReadError("The FL Studio project version could not be decoded.")
    version = FLVersion(*version_tuple[:4])
    events, unsupported = [], []
    plugin_name = None
    for ordinal, event in enumerate(raw.events):
        eid = EventEnum(event.id)
        event_type = None
        section = ""
        for enum in EventEnum.__subclasses__():
            if eid in enum:
                event_type = getattr(enum(eid), "type")
                name = enum.__name__
                if name.startswith("Pattern"):
                    section = "/patterns"
                elif name.startswith(("Mixer", "Insert", "Slot")):
                    section = "/mixer"
                elif name.startswith(("Arrangement", "Track", "TimeMarker")):
                    section = "/arrangements"
                elif name.startswith(("Channel", "Plugin")):
                    section = "/instruments"
                break
        if event_type is None:
            if event.id in pyflp.ODD_SIZE_IDS:
                event_type = UnknownDataEvent
            elif event.id < 64:
                event_type = U8Event
            elif event.id < 128:
                event_type = U16Event
            elif event.id < 192:
                event_type = U32Event
            elif event.id < 208 or event.id in pyflp.NEW_TEXT_IDS:
                event_type = UnicodeEvent if version >= FLVersion(11, 5, 0) else AsciiEvent
            elif eid == PluginID.Data and plugin_name is not None:
                event_type = get_event_by_internal_name(plugin_name)
            else:
                event_type = UnknownDataEvent
        try:
            decoded = event_type(
                eid, event.payload, **({"version": version} if event_type.NEEDS_VERSION else {})
            )
            if eid == PluginID.InternalName:
                plugin_name = decoded.value
        except Exception:
            decoded = UnknownDataEvent(eid, event.payload)
            if section not in unsupported:
                unsupported.append(section)
            if eid == PluginID.Data and "/mixer" not in unsupported:
                unsupported.append("/mixer")
        events.append(IndexedEvent(ordinal, decoded))
    return (
        Project(
            EventTree(init=events),
            channel_count=raw.header.channel_count,
            format=FileFormat(raw.header.format),
            ppq=raw.header.ppq,
        ),
        unsupported,
    )


class _Reader:
    def __init__(self) -> None:
        self.unsupported: list[dict] = []

    def report(self, path: str, reason: str = "This content could not be decoded.") -> None:
        entry = {"path": path, "reason": reason}
        if entry not in self.unsupported and len(self.unsupported) < s.MAX_REPORTS:
            self.unsupported.append(entry)

    def attr(self, obj: Any, name: str, default: Any = None) -> Any:
        value = getattr(obj, name, None)
        return default if value is None else value

    def items(self, source: Callable, path: str, convert: Callable, limit: int) -> list:
        result = []
        try:
            for ordinal, item in enumerate(source()):
                if ordinal >= limit:
                    self.report(path, "The schema's item limit was reached.")
                    break
                try:
                    result.append(convert(item, ordinal))
                except Exception:
                    self.report(path)
        except Exception:
            self.report(path)
        return result

    def text(self, value: Any, fallback: str = "") -> str:
        return str(value if value is not None else fallback).replace("\x00", "")[: s.MAX_NAME_LENGTH]

    def plugin(self, owner: Any) -> s.Plugin:
        from pyflp.plugin import PluginID, VSTPlugin

        plugin = self.attr(owner, "plugin")
        internal = self.attr(owner, "internal_name", "")
        is_vst = isinstance(plugin, VSTPlugin) or internal.lower() == "fruity wrapper"
        guid = self.attr(plugin, "guid") if is_vst else None
        plugin_id = guid.hex() if isinstance(guid, bytes) else self.attr(plugin, "fourcc") if is_vst else None
        if plugin_id is not None and not re.fullmatch(r"[\x20-\x7e]{1,128}", str(plugin_id)):
            plugin_id = None
        # Hash just the saved plugin data; no binary state is copied to JSON.
        state = None
        if hasattr(owner, "events") and PluginID.Data in owner.events.ids:
            encoded = bytes(owner.events.first(PluginID.Data))
            start = 1
            while encoded[start] & 0x80:
                start += 1
            data = encoded[start + 1 :]
            state = {"sha256": hashlib.sha256(data).hexdigest(), "size_bytes": len(data)}
        name = self.attr(plugin, "name") if is_vst else internal
        return s.Plugin(
            format="vst3" if guid else "vst2" if is_vst else "fl_native",
            name=self.text(name or self.attr(owner, "name") or "Unknown plugin"),
            vendor=self.text(self.attr(plugin, "vendor")) or None if is_vst else None,
            plugin_id=str(plugin_id) if plugin_id is not None else None,
            state=state,
        )

    def sample(self, channel: Any) -> Any:
        from pyflp.channel import ChannelID

        value = None
        if hasattr(channel, "events") and ChannelID.SamplePath in channel.events.ids:
            value = channel.events.first(ChannelID.SamplePath).value
        if not value:
            return None
        path = str(value).replace("\\", "/")
        if path.startswith("%FL"):
            return s.FactorySample(location="factory", path=path)
        if not path.startswith("/") and not PureWindowsPath(path).is_absolute() and ":" not in path:
            try:
                return s.ProjectSample(location="project", asset_path=path)
            except ValueError:
                pass
        # Absolute paths never survive. Invalid basenames are refused too.
        return s.ExternalSample(location="external", file_name=path.rsplit("/", 1)[-1])

    def routing(self, channel: Any) -> dict:
        number = self.attr(channel, "insert")
        if number is None:
            return {"status": "unknown"}
        return {"status": "known", "insert_id": f"insert:{number}"}

    def instrument(self, channel: Any, ordinal: int) -> Any:
        from pyflp.channel import Sampler, Layer

        iid = channel.iid
        if isinstance(channel, Layer):
            raise ValueError("Layer channels are not supported by schema 0.1.0")
        sampler = isinstance(channel, Sampler)
        color = self.attr(channel, "color")
        color_text = (
            None
            if color is None
            else "#" + "".join(f"{round(v * 255):02X}" for v in (color.red, color.green, color.blue))
        )
        data = dict(
            id=f"instrument:{iid}",
            kind="sampler" if sampler else "plugin",
            name=self.text(self.attr(channel, "display_name")),
            color=color_text,
            enabled=self.attr(channel, "enabled", True),
            volume={"fader": u.CHANNEL_VOLUME.from_raw(self.attr(channel, "volume", 10000)), "db": None},
            pan=u.CHANNEL_PAN.from_raw(self.attr(channel, "pan", 6400)),
            insert=self.routing(channel),
            sample=self.sample(channel) if sampler else None,
            plugin=None if sampler else self.plugin(channel),
            extensions={"fl_studio": {"channel_iid": iid}},
        )
        return TypeAdapter(s.Instrument).validate_python(data)

    def automation(self, channel: Any, ordinal: int) -> s.Automation:
        # FL stores automation positions in quarter-note beats, not project ticks.
        points = self.items(
            lambda: channel,
            "/automation",
            lambda p, i: s.AutomationPoint(
                position_ticks=round(p.position * self.ppq), value=p.value, tension=p.tension
            ),
            s.MAX_AUTOMATION_POINTS,
        )
        return s.Automation(
            id=f"automation:{channel.iid}",
            name=self.text(self.attr(channel, "display_name")),
            target={"status": "unknown"},
            points=sorted(points, key=lambda p: p.position_ticks),
        )

    def pattern(self, pattern: Any, ordinal: int) -> s.Pattern:
        def note(n: Any, index: int) -> s.Note:
            return s.Note(
                instrument_id=f"instrument:{n.rack_channel}",
                pitch=u.pitch_from_raw(n["key"]),
                start_ticks=n.position,
                length_ticks=n.length,
                velocity=u.NOTE_VELOCITY.from_raw(n.velocity),
                pan=u.NOTE_PAN.from_raw(n.pan),
                fine_pitch_cents=u.fine_pitch_cents_from_raw(n.fine_pitch),
                extensions={"fl_studio": {"ordinal": index}},
            )

        notes = self.items(
            lambda: pattern.notes, "/patterns", note, min(s.MAX_NOTES - self.note_count, s.MAX_NOTES)
        )
        known = {i.id for i in self.instruments}
        if any(n.instrument_id not in known for n in notes):
            self.report("/patterns", "Notes referencing unsupported channels were omitted.")
        notes = [n for n in notes if n.instrument_id in known]
        self.note_count += len(notes)
        length = self.attr(pattern, "length", 0) or max(
            (n.start_ticks + n.length_ticks for n in notes), default=0
        )
        return s.Pattern(
            id=f"pattern:{pattern.iid}",
            name=self.text(self.attr(pattern, "name")),
            length_ticks=length,
            notes=sorted(notes, key=lambda n: (n.start_ticks, n.extensions["fl_studio"]["ordinal"])),
        )

    def insert(self, insert: Any, ordinal: int) -> s.Insert:
        number = insert.iid + 1

        def slot(slot: Any, index: int) -> s.EffectSlot:
            return s.EffectSlot(
                index=slot.index & 0xFF,
                enabled=self.attr(slot, "enabled", True),
                dry_wet=u.SLOT_DRY_WET.from_raw(self.attr(slot, "mix", 12800)),
                plugin=self.plugin(slot),
                extensions={
                    "fl_studio": {
                        "raw_index": slot.index,
                        "name": self.text(self.attr(slot, "name")) or None,
                        "internal_name": self.text(self.attr(slot, "internal_name")) or None,
                    }
                },
            )

        slots = self.items(
            lambda: (slot for slot in insert if self.attr(slot, "internal_name") or self.attr(slot, "name")),
            f"/mixer/inserts/{ordinal}/effect_slots",
            slot,
            s.MAX_EFFECT_SLOTS,
        )
        sends = self.items(
            lambda: insert.routes,
            f"/mixer/inserts/{ordinal}/sends",
            lambda r, i: s.Send(
                to_insert_id=f"insert:{r.destination}",
                level=u.SEND_LEVEL.from_raw(12800 if r.level is None else r.level),
            ),
            s.MAX_INSERTS - 1,
        )
        return s.Insert(
            id=f"insert:{number}",
            number=number,
            name=self.text(self.attr(insert, "name")),
            enabled=self.attr(insert, "enabled", True),
            volume={"fader": u.INSERT_VOLUME.from_raw(self.attr(insert, "volume", 12800)), "db": None},
            pan=u.INSERT_PAN.from_raw(self.attr(insert, "pan", 0)),
            stereo_separation=u.INSERT_STEREO_SEPARATION.from_raw(self.attr(insert, "stereo_separation", 0)),
            sends=sends,
            effect_slots=slots,
        )

    def arrangement(self, arrangement: Any, ordinal: int) -> s.Arrangement:
        clip_ordinal = 0

        def track(track: Any, index: int) -> s.Track:
            def clip(clip: Any, unused: int) -> Any:
                nonlocal clip_ordinal
                current_ordinal = clip_ordinal
                clip_ordinal += 1
                pattern = self.attr(clip, "pattern")
                if pattern is not None:
                    kind, source = "pattern", f"pattern:{pattern.iid}"
                else:
                    channel = clip.channel
                    kind = "automation" if f"automation:{channel.iid}" in self.automation_ids else "audio"
                    source = f"{kind}:{channel.iid}"
                if source not in self.source_ids:
                    raise ValueError("Unsupported clip source")
                offsets = clip.offsets
                converted = [v if isinstance(v, int) and v >= 0 else None for v in offsets]
                if kind != "pattern" and any(v != -1 for v in offsets):
                    self.report("/arrangements", "Audio/automation clip offsets are not decoded into ticks.")
                if clip.muted is None:
                    self.report("/arrangements", "Clip mute flags are not decoded; muted is a placeholder.")
                self.clip_count += 1
                return TypeAdapter(s.Clip).validate_python(
                    dict(
                        kind=kind,
                        source_id=source,
                        start_ticks=clip.position,
                        length_ticks=clip.length,
                        offset_start_ticks=converted[0],
                        offset_end_ticks=converted[1],
                        muted=bool(clip.muted),
                        extensions={"fl_studio": {"ordinal": current_ordinal, "item_flags": clip.item_flags}},
                    )
                )

            clips = self.items(lambda: track, "/arrangements", clip, max(0, s.MAX_CLIPS - self.clip_count))
            return s.Track(
                id=f"track:{track.iid}",
                number=track.iid,
                name=self.text(self.attr(track, "name")),
                enabled=self.attr(track, "enabled", True),
                clips=sorted(clips, key=lambda c: (c.start_ticks, c.extensions["fl_studio"]["ordinal"])),
            )

        tracks = self.items(lambda: arrangement.tracks, "/arrangements", track, s.MAX_TRACKS)
        markers = self.items(
            lambda: arrangement.timemarkers,
            "/arrangements",
            lambda m, i: s.Marker(
                position_ticks=m.position,
                name=self.text(self.attr(m, "name")),
                extensions={"fl_studio": {"action": m.action}},
            ),
            s.MAX_MARKERS,
        )
        return s.Arrangement(
            id=f"arrangement:{arrangement.iid}",
            name=self.text(self.attr(arrangement, "name")),
            markers=markers,
            tracks=[t for t in tracks if t.name or t.clips or not t.enabled],
        )


def model_from_project(
    project: Any, *, ppq: int | None = None, unsupported: tuple[str, ...] | list[str] = ()
) -> s.ProjectModel:
    from pyflp.channel import Automation, ChannelID, ChannelType

    r = _Reader()
    for path in unsupported:
        r.report(path)
    r.ppq = ppq or project.ppq
    r.note_count = r.clip_count = 0
    channels = r.items(lambda: project.channels, "/instruments", lambda c, i: c, s.MAX_INSTRUMENTS)
    automations = [c for c in channels if isinstance(c, Automation)]
    # Channel type, rather than PyFLP's Python class, distinguishes audio clips
    # (which PyFLP presents as samplers once their sample has been loaded).
    audio = r.items(
        lambda: channels,
        "/audio_sources",
        lambda c, i: (
            c
            if hasattr(c, "events")
            and c.events.first(ChannelID.Type).value == ChannelType.Instrument
            and not r.attr(c, "internal_name")
            else None
        ),
        s.MAX_AUDIO_SOURCES,
    )
    audio = [c for c in audio if c is not None]
    excluded = {id(c) for c in [*automations, *audio]}
    r.instruments = r.items(
        lambda: (c for c in channels if id(c) not in excluded),
        "/instruments",
        r.instrument,
        s.MAX_INSTRUMENTS,
    )
    sources = r.items(
        lambda: audio,
        "/audio_sources",
        lambda c, i: s.AudioSource(
            id=f"audio:{c.iid}",
            name=r.text(r.attr(c, "display_name")),
            sample=r.sample(c),
            insert=r.routing(c),
        ),
        s.MAX_AUDIO_SOURCES,
    )
    automation = r.items(lambda: automations, "/automation", r.automation, s.MAX_AUTOMATION)
    r.automation_ids = {a.id for a in automation}
    patterns = r.items(lambda: project.patterns, "/patterns", r.pattern, s.MAX_PATTERNS)
    r.source_ids = {p.id for p in patterns} | {a.id for a in sources} | r.automation_ids
    arrangements = r.items(lambda: project.arrangements, "/arrangements", r.arrangement, s.MAX_ARRANGEMENTS)
    if not arrangements:
        arrangements = [s.Arrangement(id="arrangement:0", name="", markers=[], tracks=[])]
        r.report("/arrangements")
    inserts = r.items(lambda: project.mixer, "/mixer", r.insert, s.MAX_INSERTS)
    # Referenced default inserts must be listed even when FL stores a sparse mixer.
    numbers = {i.number for i in inserts}
    required = {
        int(item.insert.insert_id.split(":")[1])
        for item in [*r.instruments, *sources]
        if isinstance(item.insert, s.KnownInsert)
    }
    required |= {int(send.to_insert_id.split(":")[1]) for item in inserts for send in item.sends}
    for number in sorted(required - numbers):
        inserts.append(
            s.Insert(
                id=f"insert:{number}",
                number=number,
                name="",
                enabled=True,
                volume={"fader": 1.0, "db": None},
                pan=0.0,
                stereo_separation=0.0,
                sends=[],
                effect_slots=[],
            )
        )
    # Unknown routing/targets are explicit, never silently represented as unrouted.
    for key, items in (("instruments", r.instruments), ("audio_sources", sources)):
        if any(isinstance(item.insert, s.UnknownValue) for item in items):
            r.report(f"/{key}", "Some channel routing could not be decoded.")
    if automation:
        r.report("/automation", "Automation targets are not decoded.")
    try:
        tempo = float(project.tempo) if project.tempo is not None else None
        if tempo is not None and (not math.isfinite(tempo) or not 0 < tempo <= 999):
            tempo = None
    except Exception:
        tempo = None
    if tempo is None:
        r.report("/tempo_bpm")
    signature = {"numerator": 4, "denominator": 4}
    try:
        signature = s.TimeSignature(
            numerator=r.attr(project.arrangements.time_signature, "num", 4),
            denominator=r.attr(project.arrangements.time_signature, "beat", 4),
        ).model_dump()
    except Exception:
        r.report("/time_signature")
    current = arrangements[0].id
    try:
        selected = project.arrangements.current
        candidate = f"arrangement:{selected.iid}" if selected is not None else current
        if candidate in {a.id for a in arrangements}:
            current = candidate
        else:
            r.report("/current_arrangement_id")
    except Exception:
        r.report("/current_arrangement_id")

    def metadata(name: str) -> str | None:
        try:
            return r.text(getattr(project, name, None)) or None
        except Exception:
            r.report("/project")
            return None

    try:
        version = str(project.version)
        if not re.fullmatch(r"[0-9]{1,4}(?:\.[0-9]{1,6}){0,4}", version):
            version = None
    except Exception:
        version = None
    model = s.ProjectModel.model_validate(
        dict(
            schema="stemhub.project-model",
            schema_version=s.SCHEMA_VERSION,
            generator={"name": "stemhub-fl", "version": "0.1.0", "pyflp": PYFLP_COMMIT},
            source={"daw": "fl_studio", "daw_version": version, "ppq": r.ppq},
            project={"title": metadata("title"), "artists": metadata("artists"), "genre": metadata("genre")},
            tempo_bpm=tempo,
            time_signature=signature,
            instruments=r.instruments,
            audio_sources=sources,
            patterns=patterns,
            arrangements=arrangements,
            current_arrangement_id=current,
            mixer={"inserts": sorted(inserts, key=lambda i: i.number)},
            automation=automation,
            unsupported=r.unsupported,
            warnings=[],
            extensions={},
        )
    )
    if check_references(model):
        raise ProjectReadError("The extracted model contains invalid references.")
    return model
